#pragma once

#include "../econf.hh"

inline void build_system_blas(const econf::BuildEnv &) {
  // This package actually doesn't do anything. It acts as a BLAS package
  // variant so that blas dependencies of other packages are marked as
  // satisfied, so that their build recipes can just find the system-wide
  // installed BLAS installation.
}

ECONF_REGISTER_VARIANT("blas", "system-blas", build_system_blas);
