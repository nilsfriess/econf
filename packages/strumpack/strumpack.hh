#pragma once

#include "../../econf.hh"

inline void build_strumpack(const econf::BuildEnv &env) {
  using namespace std::literals;

  auto path = env.build_dir / "strumpack";
  const auto git_url = "https://github.com/pghysels/STRUMPACK.git"sv;

  econf::util::git_clone(git_url, path);
}

ECONF_REGISTER("strumpack", build_strumpack);
