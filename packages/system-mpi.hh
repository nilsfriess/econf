#pragma once

#include "../econf.hh"

inline void build_system_mpi(const econf::BuildEnv &) {
  // This package actually doesn't do anything. It acts as an MPI package
  // variant so that mpi dependencies of other packages are marked as satisfied,
  // so that their build recipes can just find the system-wide installed MPI
  // installation (which is what one often wants on HPC clusters).

  // TODO: We could have some kind of try_compile mechanism to try and compile a
  // simple MPI program to actually check for a working MPI installation here.
}

ECONF_REGISTER_VARIANT("mpi", "system-mpi", build_system_mpi);
