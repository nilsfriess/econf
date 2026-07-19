#pragma once

#include "cmd.hh"
#include "log.hh"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <source_location>
#include <string_view>
#include <unistd.h>

namespace econf {
// TODO: Should argc be removed?
inline void rebuild_if_necessary(
    [[maybe_unused]] int argc, char **argv,
    std::source_location loc = std::source_location::current()) {
  std::string_view binary = argv[0];
  const auto binary_last_modified = std::filesystem::last_write_time(binary);

  // True if `path` exists and is newer than the current binary. Uses the
  // error_code overload so a missing/unreadable file is simply "not newer".
  auto is_newer = [&](const std::filesystem::path &path) {
    std::error_code ec;
    auto mtime = std::filesystem::last_write_time(path, ec);
    return !ec && mtime > binary_last_modified;
  };

  bool needs_rebuild = false;

  // The build program itself, and the header that pulls in every package.
  for (const char *file : {"./econf.hh", loc.file_name(), "./packages.hh"})
    if (is_newer(file)) {
      needs_rebuild = true;
      break;
    }

  // Any split header under include/ or package definition under packages/
  // (both scanned recursively).
  for (const char *dir : {"include", "packages"}) {
    if (needs_rebuild)
      break;
    if (!std::filesystem::exists(dir))
      continue;
    std::error_code ec;
    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(dir, ec))
      if (entry.is_regular_file() && is_newer(entry.path())) {
        needs_rebuild = true;
        break;
      }
  }

  if (!needs_rebuild)
    return;

  log::info("rebuilding econf...");
  cmd::Command rebuild{"clang++"};
  rebuild.add("-std=c++20", "-ggdb", "-O0", "-Wall", "-Wextra", "-Wpedantic",
              "-pedantic");
  rebuild.add("-o", std::string(argv[0]), loc.file_name());
  int status = cmd::run_and_wait(rebuild);
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
