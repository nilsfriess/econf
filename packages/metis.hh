#pragma once

#include "../econf.hh"

inline void build_metis(const econf::BuildEnv &env) {
  constexpr econf::git::Source metis{
      .url = "https://github.com/KarypisLab/METIS.git",
      .commit = "076b60ea8f90b3e213db22ad7d8f706b62c687b5",
  };

  const auto builddir = env.build_dir / "metis";

  // Clone the lib
  econf::git::checkout(env, metis, builddir);

  // Build the lib
  {
    econf::cmd::Command cmd("make");
    cmd.add("config");
    cmd.fmt("prefix={}", env.install_dir().string());
    cmd.fmt("gklib_path={}", env.install_dir().string());
    cmd.fmt("cc={}", env.cc);
    cmd.fmt("shared=1");
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

ECONF_REGISTER("metis", build_metis, {"gklib"});
