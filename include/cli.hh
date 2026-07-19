#pragma once

#include "cmd.hh"
#include "env.hh"
#include "error.hh"
#include "log.hh"
#include "registry.hh"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <iostream>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace econf {
// Parses a non-negative integer, or fails with a message naming the flag.
inline unsigned parse_uint(std::string_view flag, std::string_view v) {
  unsigned n = 0;
  const char *end = v.data() + v.size();
  auto [ptr, ec] = std::from_chars(v.data(), end, n);
  if (ec != std::errc{} || ptr != end)
    fail("{} expects a non-negative integer, got '{}'", flag, v);
  return n;
}

// Everything an option's handler may touch while parsing. Build configuration
// lives in `env`; the requested targets and the help request do not belong in
// `env`, so they ride alongside it here.
struct ParseState {
  BuildEnv &env;
  std::vector<std::string_view> &targets;
  bool &show_help;
  bool &dry_run;
};

// A single command-line option. This table is the one source of truth: both
// the parser and the help printer are driven by it, so adding an option means
// adding one row here and nothing else. The handlers are captureless lambdas,
// so they decay to plain function pointers and the whole table is constexpr.
struct Opt {
  std::string_view flag;    // e.g. "--cc"
  std::string_view metavar; // e.g. "<path>"; empty => flag consumes no value
  std::string_view help;
  void (*apply)(ParseState &, std::string_view value); // value empty for flags

  bool takes_value() const { return !metavar.empty(); }
};

inline constexpr Opt options[] = {
    {"--cc", "<path>", "Path to a C compiler",
     [](ParseState &s, std::string_view v) { s.env.cc = v; }},
    {"--cxx", "<path>", "Path to a C++ compiler",
     [](ParseState &s, std::string_view v) { s.env.cxx = v; }},
    {"--debug", "", "Build packages in debug mode",
     [](ParseState &s, std::string_view) {
       s.env.args.mode = build_mode::debug;
     }},
    {"--release", "", "Build packages in release mode (default)",
     [](ParseState &s, std::string_view) {
       s.env.args.mode = build_mode::release;
     }},
    {"--parallel", "<n>", "Parallel build jobs (default: nproc)",
     [](ParseState &s, std::string_view v) {
       s.env.args.jobs = parse_uint("--parallel", v);
     }},
    {"--dist-name", "<name>",
     "Label in the install dir: dist-<name>-<mode> (default: econf)",
     [](ParseState &s, std::string_view v) { s.env.dist_name = v; }},
    {"--download", "<package>", "Download and build the given package",
     [](ParseState &s, std::string_view v) { s.targets.push_back(v); }},
    {"--dry-run", "", "Resolve and print the build order, then stop",
     [](ParseState &s, std::string_view) { s.dry_run = true; }},
    {"--help", "", "Print this help",
     [](ParseState &s, std::string_view) { s.show_help = true; }},
};

inline void print_help(std::string_view prog) {
  std::cout << std::format("Usage\n\t{} [options]\n\nOptions\n", prog);
  for (const auto &o : options) {
    auto left = std::format("{} {}", o.flag, o.metavar);
    std::cout << std::format("  {:<34}{}\n", left, o.help);
  }
  std::cout << std::format("\nKnown packages: {}\n",
                           cmd::join(package::Registry::instance().names()));
}

[[noreturn]] inline void usage_error(std::string_view prog,
                                     std::string_view msg) {
  log::error("{}", msg);
  print_help(prog);
  std::exit(1);
}

// Parses argv into `env` (build configuration) and returns the packages the
// user asked to build, in the order given. On a usage mistake (unknown option,
// missing or invalid value) it prints the error and the help, then exits.
inline std::vector<std::string_view> parse_args(BuildEnv &env, int argc,
                                                char **argv, bool &dry_run) {
  std::vector<std::string_view> targets;
  bool show_help = false;
  ParseState state{env, targets, show_help, dry_run};

  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];

    const Opt *opt = nullptr;
    for (const auto &o : options)
      if (o.flag == arg) {
        opt = &o;
        break;
      }
    if (!opt)
      usage_error(argv[0], std::format("unknown option: {}", arg));

    std::string_view value;
    if (opt->takes_value()) {
      if (i + 1 >= argc)
        usage_error(argv[0], std::format("{} requires a value", arg));
      value = argv[++i];
    }

    try {
      opt->apply(state, value);
    } catch (const Error &e) {
      usage_error(argv[0], e.what());
    }
  }

  if (show_help) {
    print_help(argv[0]);
    std::exit(0);
  }
  return targets;
}

// Resolves `targets` and all their transitive dependencies into the order they
// must be built: every package appears after each of its dependencies. Every
// dependency must itself be a registered package. Anything that cannot be
// planned -- an unknown target, an unknown dependency, or a dependency cycle --
// aborts via fail() with a message that says what is wrong and how to fix it.
// The returned entry pointers are owned by the registry singleton and stay
// valid for the rest of the run.
inline std::vector<std::pair<std::string, const package::Registry::Entry *>>
plan_build_order(const std::vector<std::string_view> &targets) {
  using Entry = package::Registry::Entry;
  auto &registry = package::Registry::instance();

  // A variant package (e.g. mpich) satisfies a category dependency (e.g. "mpi").
  // The provider for each category is chosen explicitly -- it is whichever
  // variant the user named on the command line. Two variants of one category is
  // a conflict: there is no way to know which the user meant.
  std::unordered_map<std::string, std::string> provider; // category -> package
  for (const auto &name : targets) {
    const Entry *e = registry.find(name);
    if (!e || e->is_variant_of.empty())
      continue;
    auto [it, inserted] = provider.try_emplace(e->is_variant_of, name);
    if (!inserted && it->second != name)
      fail("both '{}' and '{}' are variants of '{}'; request only one", name,
           it->second, e->is_variant_of);
  }
  for (const auto &[category, pkg] : provider)
    log::verbose("using '{}' as the '{}' provider", pkg, category);

  // Resolve a target or dependency name to the concrete package that satisfies
  // it: a real package resolves to itself; a category (like "mpi") resolves to
  // its chosen provider. `required_by`, when set, names whoever asked for
  // `name`, so an unresolved name can point back at them.
  auto resolve = [&](const std::string &name, const std::string *required_by)
      -> std::pair<std::string, const Entry *> {
    if (const Entry *e = registry.find(name))
      return {name, e};
    if (auto it = provider.find(name); it != provider.end()) {
      const Entry *e = registry.find(it->second);
      assert(e && "provider must be a registered package");
      return {it->second, e};
    }
    // Unresolved. If some package is a variant of `name`, then `name` is a
    // category and the user just has not picked a provider for it.
    auto variants = registry.variants_of(name);
    std::sort(variants.begin(), variants.end());
    if (!variants.empty()) {
      if (required_by)
        fail("'{}' needs a '{}' provider, but none was selected.\n"
             "  Add one of its variants as a target: {}.\n"
             "  For example: --download {}",
             *required_by, name, cmd::join(variants), variants.front());
      fail("'{}' is a category, not a package; request one of its providers: "
           "{}.\n"
           "  For example: --download {}",
           name, cmd::join(variants), variants.front());
    }
    if (!required_by)
      fail("unknown package '{}' (known: {})", name,
           cmd::join(registry.names()));
    fail("'{}' depends on '{}', which is not a known package.\n"
         "  Fix the dependency name, or add a package for it under packages/.\n"
         "  Known packages: {}",
         *required_by, name, cmd::join(registry.names()));
  };

  // Walk the transitive closure of `targets`, recording the graph as in-degrees
  // (dependencies left to build) plus a reverse adjacency list (who depends on
  // each package). Every node is a concrete package: category dependencies are
  // rewritten to their provider the moment they are seen, so a category name
  // never becomes a node of its own.
  std::unordered_map<std::string, const Entry *> resolved;
  std::unordered_map<std::string, std::size_t> indeg;
  std::unordered_map<std::string, std::vector<std::string>> dependents;
  std::unordered_map<std::string, std::string> required_by;

  std::vector<std::string> worklist(targets.begin(), targets.end());
  while (!worklist.empty()) {
    std::string raw = std::move(worklist.back());
    worklist.pop_back();

    auto req = required_by.find(raw);
    auto [name, e] =
        resolve(raw, req == required_by.end() ? nullptr : &req->second);
    if (resolved.contains(name))
      continue;

    resolved.emplace(name, e);
    indeg[name] = e->deps.size();
    for (const auto &dep : e->deps) {
      auto [depname, depentry] = resolve(dep, &name);
      dependents[depname].push_back(name);
      if (!resolved.contains(depname)) {
        required_by.try_emplace(depname, name);
        worklist.push_back(depname);
      }
    }
  }

  // Kahn's algorithm over the closure: repeatedly take a package whose
  // dependencies are all built and release the packages waiting on it.
  std::vector<std::pair<std::string, const Entry *>> order;
  order.reserve(resolved.size());

  std::queue<std::string> ready;
  for (const auto &[name, degree] : indeg)
    if (degree == 0)
      ready.push(name);

  while (!ready.empty()) {
    std::string name = std::move(ready.front());
    ready.pop();
    order.emplace_back(name, resolved[name]);

    auto it = dependents.find(name);
    if (it == dependents.end())
      continue;
    for (const auto &dependent : it->second)
      if (--indeg[dependent] == 0)
        ready.push(dependent);
  }

  // Whatever still carries unbuilt dependencies is caught in (or behind) a
  // cycle, which no build order can satisfy.
  if (order.size() != resolved.size()) {
    std::vector<std::string> cyclic;
    for (const auto &[name, degree] : indeg)
      if (degree > 0)
        cyclic.push_back(name);
    std::sort(cyclic.begin(), cyclic.end());
    fail("dependency cycle detected among: {}.\n"
         "  Remove one of the dependencies above to break the cycle.",
         cmd::join(cyclic));
  }

  return order;
}

inline void run(BuildEnv &env, int argc, char **argv) {
  bool dry_run = false;
  auto targets = parse_args(env, argc, argv, dry_run);

  // Resolve the requested targets and their dependencies into one build order,
  // then build each package exactly once, dependencies first.
  auto build_plan = plan_build_order(targets);
  log::info("Resolved dependency graph, packages will be built in the "
            "following order:");
  for (const auto &[name, entry] : build_plan) {
    log::info("  {}", name);
  }

  // With --dry-run we stop here: the plan above is the whole point, and nothing
  // should be downloaded, configured, or installed.
  if (dry_run) {
    log::info("dry run: stopping before building");
    return;
  }

  for (const auto &[name, entry] : build_plan) {
    package::BuildScope scope{name};
    entry->build(env);
  }
}
} // namespace econf
