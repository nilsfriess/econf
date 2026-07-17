#pragma once

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <ranges>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>

namespace econf {
struct BuildEnv {
  std::filesystem::path build_dir = "./build";
  std::string cc = "clang";
  std::string cxx = "clang++";
};

namespace package {
class Registry {
public:
  using BuildFn = std::function<void(const BuildEnv &)>;
  struct Entry {
    BuildFn build;
    std::vector<std::string> deps;
  };

  static Registry &instance() {
    static Registry r;
    return r;
  }

  void add(std::string name, BuildFn fn, std::vector<std::string> deps = {}) {
    entries_[std::move(name)] = Entry{std::move(fn), std::move(deps)};
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

private:
  std::unordered_map<std::string, Entry> entries_;
};
} // namespace package

namespace log {
enum class levels { error = -1, info = 0, verbose = 1 };

static constexpr auto curr_level = levels::verbose;

template <typename... Args>
void error(std::string_view fmt_msg, Args &&...args) {
  std::cout << std::vformat(std::format("[error] {}\n", fmt_msg),
                            std::make_format_args(args...))
            << std::flush;
}

template <typename... Args>
void info(std::string_view fmt_msg, Args &&...args) {
  std::cout << std::vformat(std::format("[info] {}\n", fmt_msg),
                            std::make_format_args(args...))
            << std::flush;
}

template <typename... Args>
void verbose(std::string_view fmt_msg, Args &&...args) {
  if (curr_level < levels::verbose)
    return;
  std::cout << std::vformat(std::format("[verbose] {}\n", fmt_msg),
                            std::make_format_args(args...))
            << std::flush;
}
} // namespace log

namespace cmd {
// Runs `command` to completion and returns its exit status, or -1 if it
// could not be started/waited on.
inline int run_and_wait(std::string_view command) {
  std::vector<std::string> args;

  for (auto &&part : std::views::split(command, ' ')) {
    args.emplace_back(part.begin(), part.end());
  }

  std::vector<char *> argv;
  for (auto &arg : args) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);

  pid_t pid = fork();
  if (pid < 0) {
    log::error("fork failed: {}", std::strerror(errno));
    return -1;
  }

  if (pid == 0) {
    execvp(argv[0], argv.data());
    log::error("failed to run '{}': {}", command, std::strerror(errno));
    std::_Exit(127);
  }

  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    log::error("waitpid failed: {}", std::strerror(errno));
    return -1;
  }

  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
} // namespace cmd

namespace util {
inline void git_clone(std::string_view url, std::filesystem::path path) {
  cmd::run_and_wait(std::format("git clone {} {}", url, path.string()));
}
} // namespace util

inline void rebuild_if_necessary(int argc, char **argv) {
  std::string_view binary = argv[0];
  std::vector<std::string_view> files = {"./econf.hh", "./econf.cc"};

  auto binary_last_modified = std::filesystem::last_write_time(binary);

  bool needs_rebuild = false;
  for (auto file : files) {
    auto file_last_modified = std::filesystem::last_write_time(file);
    if (file_last_modified > binary_last_modified) {
      needs_rebuild = true;
      break;
    }
  }

  if (!needs_rebuild)
    return;

  log::info("rebuilding econf...");
  int status = cmd::run_and_wait("clang++ -std=c++20 -oeconf econf.cc");
  if (status != 0) {
    log::error("rebuild failed with exit status {}", status);
    std::exit(1);
  }

  log::info("rebuild succeeded, restarting...");
  execvp(argv[0], argv);
  log::error("failed to restart '{}': {}", binary, std::strerror(errno));
  std::exit(1);
}

} // namespace econf

#define ECONF_REGISTER(name, fn, ...)                                          \
  namespace {                                                                  \
  struct fn##_registrar_ {                                                     \
    fn##_registrar_() {                                                        \
      ::econf::package::Registry::instance().add(                              \
          name, fn, std::vector<std::string>{__VA_ARGS__});                    \
    }                                                                          \
  } fn##_registrar_instance_;                                                  \
  }
