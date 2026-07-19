#pragma once

#include "../econf.hh"

inline void build_openmpi(const econf::BuildEnv &env) {
  constexpr econf::git::Source openmpi{
      .url = "https://github.com/open-mpi/ompi.git",
      .tag = "v5.0.10",
      .resursive = true,
  };

  const auto srcdir = env.build_dir / "openmpi";
  econf::git::checkout(env, openmpi, srcdir);

  // OpenMPI is autotools, not CMake: autogen -> configure -> make -> install.
  // Each step is cwd-driven, so all of them run from the source directory.
  econf::cmd::check(econf::cmd::Command{"./autogen.pl"}, srcdir);

  econf::cmd::Command configure{"./configure"};
  configure.fmt("--prefix={}", env.install_dir().string());
  econf::cmd::check(configure, srcdir);

  econf::cmd::Command make{"make"};
  make.add("all");
  make.add("-j", std::to_string(env.args.jobs));
  econf::cmd::check(make, srcdir);

  econf::cmd::Command install{"make"};
  install.add("install");
  econf::cmd::check(install, srcdir);
}

ECONF_REGISTER_VARIANT("mpi", "openmpi", build_openmpi);
