#pragma once

#include "../econf.hh"

inline void build_gklib(const econf::BuildEnv &env) {
  constexpr econf::git::Source gklib{
      .url = "https://github.com/KarypisLab/GKlib.git",
      .commit = "3b7d61b9f885063c89901f3901fb4426f9cfb58f",
  };

  const auto builddir = env.build_dir / "gklib";

  // Clone the lib
  econf::git::checkout(env, gklib, builddir);

  // Build the lib
  {
    econf::cmd::Command cmd("make");
    cmd.add("config");
    cmd.fmt("prefix={}", env.install_dir().string());
    cmd.fmt("cc={}", env.cc);
    cmd.add("shared=1");
    if (env.args.mode == econf::build_mode::debug)
      cmd.add("debug=set");
    econf::cmd::check(cmd.argv, builddir);
  }

  // Install the lib
  {
    econf::cmd::Command cmd("make");
    cmd.add("install");
    econf::cmd::check(cmd.argv, builddir);
  }
}

ECONF_REGISTER("gklib", build_gklib);
