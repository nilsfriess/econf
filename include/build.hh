#pragma once

#include "cmd.hh"
#include "env.hh"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace econf {
namespace build {
inline void cmake(const BuildEnv &env, std::vector<std::string_view> args,
                  const std::filesystem::path &source_path) {
  auto build_path = source_path;
  build_path += "_build";

  cmd::Command cmd{"cmake"};
  cmd.fmt("-DCMAKE_CXX_COMPILER={}", env.cxx);
  cmd.fmt("-DCMAKE_C_COMPILER={}", env.cc);
  cmd.fmt("-DCMAKE_BUILD_TYPE={}",
          env.args.mode == build_mode::debug ? "Debug" : "Release");
  cmd.fmt("-DCMAKE_INSTALL_PREFIX={}", env.install_dir().string());
  cmd.fmt("-DCMAKE_PREFIX_PATH={}", env.install_dir().string());
  cmd.add("-S", source_path.string());
  cmd.add("-B", build_path.string());
  for (auto a : args)
    cmd.add(std::string(a));
  cmd::check(cmd);
}

inline void cmake_make(const BuildEnv &env, std::vector<std::string_view> args,
                       const std::filesystem::path &source_path) {
  auto build_path = source_path;
  build_path += "_build";

  cmd::Command cmd{"cmake"};
  cmd.add("--build", build_path.string());
  cmd.add("--parallel", std::to_string(env.args.jobs));
  for (auto a : args)
    cmd.add(std::string(a));
  cmd::check(cmd);
}

// Installs a built package into env.install_dir(). The prefix was baked in at
// configure time (CMAKE_INSTALL_PREFIX), so `cmake --install` needs nothing
// more than the build directory.
inline void cmake_install(const BuildEnv &env,
                          const std::filesystem::path &source_path) {
  auto build_path = source_path;
  build_path += "_build";

  cmd::Command cmd{"cmake"};
  cmd.add("--install", build_path.string());
  cmd::check(cmd);
}

inline void cmake_make_install(const BuildEnv &env,
                               std::vector<std::string_view> args,
                               const std::filesystem::path &source_path) {
  cmake(env, args, source_path);
  cmake_make(env, {}, source_path);
  cmake_install(env, source_path);
}
} // namespace build
} // namespace econf
