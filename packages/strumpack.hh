#pragma once

#include "../econf.hh"

inline void build_strumpack(const econf::BuildEnv &env) {
  constexpr econf::git::Source strumpack{
      .url = "https://github.com/pghysels/STRUMPACK.git",
      .tag = "v7.2.0",
  };

  const auto builddir = env.build_dir / "strumpack";

  econf::git::checkout(env, strumpack, builddir);
  econf::build::cmake_make_install(env,
                                   {
                                       "-DBUILD_SHARED_LIBS=On",
                                   },
                                   builddir);
}

ECONF_REGISTER("strumpack", build_strumpack, "metis", "mpi", "blas");
