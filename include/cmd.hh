#pragma once

#include "error.hh"
#include "log.hh"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace econf {
namespace cmd {
// A command is just its argv: one string per token, program first. Building it
// up keeps each option next to its value instead of scattering values into a
// positional format-argument pack, and it is passed straight to execvp with no
// re-splitting (so tokens may safely contain spaces).
struct Command {
  std::vector<std::string> argv;

  Command() { argv.reserve(16); }
  explicit Command(std::string prog) : Command() {
    argv.push_back(std::move(prog));
  }

  // Append one or more literal tokens, e.g. add("-S", source_path.string()).
  template <typename... Ts> Command &add(Ts &&...ts) {
    (argv.emplace_back(std::forward<Ts>(ts)), ...);
    return *this;
  }

  // Append one formatted token, keeping the option glued to its value, e.g.
  // fmt("-DCMAKE_BUILD_TYPE={}", mode).
  template <typename... Ts>
  Command &fmt(std::format_string<Ts...> f, Ts &&...ts) {
    argv.push_back(std::format(f, std::forward<Ts>(ts)...));
    return *this;
  }

  operator std::span<const std::string>() const { return argv; }
};

inline std::string join(std::span<const std::string> args) {
  std::string out;
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (i)
      out += ' ';
    out += args[i];
  }
  return out;
}

// Runs `args` to completion and returns its exit status, or -1 if it
// could not be started/waited on. If `cwd` is non-empty the child changes into
// that directory before exec; the parent's working directory is untouched.
inline int run_and_wait(std::span<const std::string> args,
                        const std::filesystem::path &cwd = {}) {
  if (args.empty()) {
    log::error("run_and_wait called with an empty command");
    return -1;
  }

  log::verbose("Running command `{}`", join(args));

  std::vector<char *> argv;
  argv.reserve(args.size() + 1);
  for (auto &arg : args)
    argv.push_back(const_cast<char *>(arg.c_str())); // execvp won't modify
  argv.push_back(nullptr);

  pid_t pid = fork();
  if (pid < 0) {
    log::error("fork failed: {}", std::strerror(errno));
    return -1;
  }

  if (pid == 0) {
    if (!cwd.empty() && chdir(cwd.c_str()) != 0) {
      log::error("failed to enter '{}': {}", cwd.string(),
                 std::strerror(errno));
      std::_Exit(127);
    }
    execvp(argv[0], argv.data());
    log::error("failed to run '{}': {}", args[0], std::strerror(errno));
    std::_Exit(127);
  }

  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    log::error("waitpid failed: {}", std::strerror(errno));
    return -1;
  }

  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Throws econf::Error on non-zero exit
inline void check(std::span<const std::string> args,
                  const std::filesystem::path &cwd = {}) {
  int status = run_and_wait(args, cwd);
  if (status != 0)
    fail("command failed (exit {}): {}", status, join(args));
}

struct Output {
  int status;      // child exit code, or -1 if it could not be run/waited on
  std::string out; // everything the child wrote to stdout
};

// Like run_and_wait, but captures the child's stdout instead of letting it flow
// to the terminal (stderr stays attached to the terminal). Use this when you
// need to read a command's output, e.g. `git --version`.
inline Output run_and_capture(std::span<const std::string> args,
                              const std::filesystem::path &cwd = {}) {
  if (args.empty()) {
    log::error("run_and_capture called with an empty command");
    return {-1, {}};
  }

  log::verbose("Running command {}", join(args));

  int fds[2]; // fds[0] = read end (parent), fds[1] = write end (child stdout)
  if (pipe(fds) != 0) {
    log::error("pipe failed: {}", std::strerror(errno));
    return {-1, {}};
  }

  std::vector<char *> argv;
  argv.reserve(args.size() + 1);
  for (auto &arg : args)
    argv.push_back(const_cast<char *>(arg.c_str()));
  argv.push_back(nullptr);

  pid_t pid = fork();
  if (pid < 0) {
    log::error("fork failed: {}", std::strerror(errno));
    close(fds[0]);
    close(fds[1]);
    return {-1, {}};
  }

  if (pid == 0) {
    // Child: point stdout at the pipe, then exec. Errors go to stderr so they
    // don't end up in the captured output.
    close(fds[0]);
    if (dup2(fds[1], STDOUT_FILENO) < 0)
      std::_Exit(127);
    close(fds[1]);
    if (!cwd.empty() && chdir(cwd.c_str()) != 0) {
      std::fprintf(stderr, "failed to enter '%s': %s\n", cwd.c_str(),
                   std::strerror(errno));
      std::_Exit(127);
    }
    execvp(argv[0], argv.data());
    std::fprintf(stderr, "failed to run '%s': %s\n", argv[0],
                 std::strerror(errno));
    std::_Exit(127);
  }

  // Parent: drain the pipe to EOF *before* waiting. If we waited first, a child
  // that fills the pipe buffer would block on write while we block on wait.
  close(fds[1]);
  std::string out;
  char buf[4096];
  for (;;) {
    ssize_t n = read(fds[0], buf, sizeof buf);
    if (n > 0)
      out.append(buf, static_cast<std::size_t>(n));
    else if (n == 0)
      break; // write end closed: child exited
    else if (errno != EINTR) {
      log::error("read failed: {}", std::strerror(errno));
      break;
    }
  }
  close(fds[0]);

  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    log::error("waitpid failed: {}", std::strerror(errno));
    return {-1, std::move(out)};
  }
  return {WIFEXITED(status) ? WEXITSTATUS(status) : -1, std::move(out)};
}
} // namespace cmd
} // namespace econf
