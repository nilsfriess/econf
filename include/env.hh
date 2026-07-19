#pragma once

#include <filesystem>
#include <format>
#include <string>
#include <thread>

namespace econf {
enum class build_mode {
  debug,
  release,
};

inline unsigned default_jobs() {
  unsigned n = std::thread::hardware_concurrency();
  return n ? n : 1;
}

struct Args {
  build_mode mode = build_mode::release;
  unsigned jobs = default_jobs(); // nproc, resolved once when BuildEnv is built
};

struct BuildEnv {
  std::filesystem::path build_dir = std::filesystem::current_path() / "build";
  std::string cc = "/usr/bin/clang";
  std::string cxx = "/usr/bin/clang++";
  std::string dist_name = "econf"; // infix in the install dir name
  Args args{};

  // Shared install prefix, a sibling of build_dir. The mode suffix keeps debug
  // and release trees from clobbering each other; everything installs here so
  // downstream projects can point CMAKE_PREFIX_PATH at it.
  std::filesystem::path install_dir() const {
    const char *m = args.mode == build_mode::debug ? "debug" : "release";
    return build_dir.parent_path() / std::format("dist-{}-{}", dist_name, m);
  }
};
} // namespace econf
