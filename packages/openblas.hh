#pragma once

#include "../econf.hh"

inline void build_openblas(const econf::BuildEnv &env) {
  econf::todo("build_openblas");
}

ECONF_REGISTER_VARIANT("blas", "openblas", build_openblas);
