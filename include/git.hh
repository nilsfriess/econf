#pragma once

#include "cmd.hh"
#include "env.hh"
#include "error.hh"
#include "log.hh"

#include <compare>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

namespace econf {
namespace git {
struct Source {
  std::string_view url{};
  std::string_view tag{};
  std::string_view commit{};
  bool resursive = false;
};

struct version {
  int major;
  int minor;
  int patch;

  // Member-wise in declaration order: major, then minor, then patch. That is
  // the correct lexicographic ordering, and it synthesizes >=, >, <, <=.
  auto operator<=>(const version &) const = default;
};

// Runs `git --version` and parses the result, e.g. "git version 2.43.0".
inline version installed_version() {
  cmd::Command cmd{"git"};
  cmd.add("--version");
  auto [status, out] = cmd::run_and_capture(cmd);
  if (status != 0)
    fail("'git --version' failed (exit {})", status);

  // Output may carry a vendor suffix, e.g. "2.39.3 (Apple Git-146)"; scan from
  // the first digit and take the leading major.minor.patch.
  version v{};
  auto pos = out.find_first_of("0123456789");
  if (pos == std::string::npos || std::sscanf(out.c_str() + pos, "%d.%d.%d",
                                              &v.major, &v.minor, &v.patch) < 2)
    fail("could not parse git version from '{}'", out);
  return v;
}

inline void checkout(const BuildEnv &env, const Source &src,
                     const std::filesystem::path &path) {
  if (not std::filesystem::exists(env.build_dir)) {
    if (not std::filesystem::create_directory(env.build_dir)) {
      fail("Could not create build directory `{}`", env.build_dir.string());
    }
  }

  if (std::filesystem::exists(path / ".git")) {
    log::verbose("{} already cloned", path.string());
    return;
  }
  if (std::filesystem::exists(path))
    fail("{} exists but is not a git checkout", path.string());

  if (!src.tag.empty() and src.commit.empty()) { // Only tag was provided
    cmd::Command cmd{"git"};
    cmd.add("clone", "--depth", "1");
    if (src.resursive)
      cmd.add("--recursive");
    cmd.add("--branch", std::string(src.tag));
    cmd.add(std::string(src.url), path.string());
    cmd::check(cmd);
  } else if (not src.commit.empty()) { // Commit was provided (tag is ignored)
    if (!src.tag.empty()) {
      log::warn("Both a tag ({}) and a commit ({}) were provided for git URL "
                "{}, tag will be ignored and commit will be used instead",
                src.tag, src.commit, src.url);
    }

    // Since git does not provide a way to directly clone a repo with a given
    // revision, we first initialise an empty repo, then add the git.url as a
    // remote and then fetch the specific commit.
    if (not std::filesystem::create_directory(path)) {
      // This should actually not happen, because above we check if that
      // directory already exists and fail() in that case
      fail("Could not create directory {} to clone {} into", path.string(),
           src.url);
    }
    {
      cmd::Command cmd{"git"};
      cmd.add("-C", path.string(), "init"); // init a git repo in the dest dir
      cmd::check(cmd);
    }
    {
      cmd::Command cmd{"git"};
      cmd.add("-C", path.string());
      cmd.add("remote", "add", "origin", std::string(src.url));
      cmd::check(cmd);
    }
    {
      cmd::Command cmd{"git"};
      cmd.add("-C", path.string());
      cmd.add("fetch", "origin", "--depth=1", std::string(src.commit));
      cmd::check(cmd);
    }
    {
      cmd::Command cmd{"git"};
      cmd.add("-C", path.string());
      cmd.add("reset", "--hard", "FETCH_HEAD");
      cmd::check(cmd);
    }
  } else if (src.tag.empty() and src.commit.empty()) {
    todo("Git clone from neither commit nor tag");
  }
}
} // namespace git
} // namespace econf
