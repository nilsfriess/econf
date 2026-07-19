#pragma once

#include "../econf.hh"

inline void build_mpich(const econf::BuildEnv &env) {
  constexpr econf::git::Source mpich{
      .url = "https://github.com/pmodels/mpich.git",
      .tag = "v5.0.1",
      .resursive = true,
  };

  const auto srcdir = env.build_dir / "mpich";
  econf::git::checkout(env, mpich, srcdir);

  // Mpich is autotools, not CMake: autogen -> configure -> make -> install.
  // Each step is cwd-driven, so all of them run from the source directory.
  econf::cmd::check(econf::cmd::Command{"./autogen.sh"}, srcdir);

  econf::cmd::Command configure{"./configure"};
  configure.fmt("--prefix={}", env.install_dir().string());
  configure.fmt("--without-hip");
  econf::cmd::check(configure, srcdir);

  econf::cmd::Command make{"make"};
  make.add("-j", std::to_string(env.args.jobs));
  econf::cmd::check(make, srcdir);

  econf::cmd::Command install{"make"};
  install.add("install");
  econf::cmd::check(install, srcdir);
}

ECONF_REGISTER_VARIANT("mpi", "mpich", build_mpich);
