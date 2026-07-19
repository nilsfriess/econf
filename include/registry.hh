#pragma once

#include "env.hh"
#include "log.hh"

#include <chrono>
#include <exception>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace econf {
namespace package {
class Registry {
public:
  using BuildFn = std::function<void(const BuildEnv &)>;
  struct Entry {
    BuildFn build;
    std::set<std::string> deps;
    std::string is_variant_of;
  };

  static Registry &instance() {
    static Registry r;
    return r;
  }

  void add(std::string name, BuildFn fn, std::set<std::string> deps = {}) {
    entries_[std::move(name)] = Entry{std::move(fn), std::move(deps)};
  }

  /* @brief Add a so-called *variant package* (e.g. MPICH is a variant of
   *        MPI; OpenBLAS is a variant of BLAS)
   *
   * When a package only needs some MPI implementation, it can just put
   * "mpi" in its dependency list. An MPI implementation package (e.g. MPICH)
   * can then be registered using this function (usually via the macro
   * ECONF_REGISTER_VARIANT) as an "mpi" variant. Running
   *    ./econf --download strumpack --download mpich
   * will then recognize that mpich is a package that satisfies the mpi
   * dependency of strumpack. Packages can of course still require specific
   * mpi variants as dependencies by just using there full name in the
   * dependency list (but this should generally be avoided, because on a HPC
   * cluster, for instance, one would typically use the system MPI; this can be
   * achieved by passing --download system-mpi which is a package that marks the
   * mpi dependecy as satisfied without actually doing anything.
   */
  void add_variant(std::string variant, std::string name, BuildFn fn,
                   std::set<std::string> deps = {}) {
    entries_[std::move(name)] = Entry{
        .build = std::move(fn),
        .deps = std::move(deps),
        .is_variant_of = std::move(variant),
    };
  }

  const Entry *find(std::string_view name) const {
    auto it = entries_.find(std::string(name));
    return it == entries_.end() ? nullptr : &it->second;
  }

  std::vector<std::string> names() const {
    std::vector<std::string> r;
    for (auto &[k, _] : entries_)
      r.push_back(k);
    return r;
  }

  // Names of all packages registered as a variant of `category`, e.g.
  // variants_of("mpi") -> {"mpich", "openmpi", "system-mpi"}. Lets an unfilled
  // category dependency tell the user which providers could satisfy it.
  std::vector<std::string> variants_of(std::string_view category) const {
    std::vector<std::string> r;
    for (auto &[name, e] : entries_)
      if (e.is_variant_of == category)
        r.push_back(name);
    return r;
  }

private:
  std::unordered_map<std::string, Entry> entries_;
};

// RAII guard around a package build: logs on entry and again when the scope
// exits, reporting success or failure. The destructor runs on every path, so
// the closing line prints even when the build throws (cmd::check does).
struct BuildScope {
  std::string_view name;
  std::chrono::steady_clock::time_point start;

  explicit BuildScope(std::string_view n)
      : name(n), start(std::chrono::steady_clock::now()) {
    log::info("==> building {}", name);
  }

  ~BuildScope() {
    auto secs =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    if (std::uncaught_exceptions() > 0)
      log::error("==> {} failed after {:.1f}s", name, secs);
    else
      log::info("==> {} done ({:.1f}s)", name, secs);
  }
};
} // namespace package
} // namespace econf

#define ECONF_REGISTER(name, fn, ...)                                          \
  namespace {                                                                  \
  struct fn##_registrar_ {                                                     \
    fn##_registrar_() {                                                        \
      ::econf::package::Registry::instance().add(                              \
          name, fn, std::set<std::string>{__VA_ARGS__});                       \
    }                                                                          \
  } fn##_registrar_instance_;                                                  \
  }

#define ECONF_REGISTER_VARIANT(variant, name, fn, ...)                         \
  namespace {                                                                  \
  struct fn##_registrar_ {                                                     \
    fn##_registrar_() {                                                        \
      ::econf::package::Registry::instance().add_variant(                      \
          variant, name, fn, std::set<std::string>{__VA_ARGS__});              \
    }                                                                          \
  } fn##_registrar_instance_;                                                  \
  }
