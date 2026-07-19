# econf - easy configuration of reproducible build environments
`econf` is a tool written in C++20 that makes it easy to create reproducible build environments mainly aimed at HPC/ scientific computing contexts.

## Quick start
Compile the econf binary
```bash
> g++ -std=c++20 -o econf econf.cc
```
and then run it to download and compile one or several of the available packages:
```bash
> ./econf --download strumpack --download openblas --download system-mpi
```
This downloads and builds the [STRUMPACK](https://portal.nersc.gov/project/sparse/strumpack/) solver package and the [OpenBLAS](https://github.com/OpenMathLib/OpenBLAS) BLAS library, as well as necessary dependencies such as the [METIS](https://github.com/KarypisLab/METIS) graph partitioner that STRUMPACK uses. The option `--download system-mpi` tells `econf` to try to use the system's MPI installation instead of trying to download one. All libraries are installed to a local folder (`./dist-econf-release` by default) which can either be added to `$PATH` to make the libraries available for othe programs or added to search paths such as `CMAKE_PREFIX_PATH` for other libraries.

Run the `econf` program with the option `--help` to list all options and available packages.

## Adding new packages
Build recipes for packages are written as C++ header files located in the folder `packages`. The build recipe for the STRUMPACK library located in `packages/strumpack.hh` looks like this

```cpp
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
```

The `econf` library provides a variety of utility functions that facilitate writing new build recipes. In this case, the source code is cloned via `git` from a pinned tag to ensure reproducibility and then built via `cmake`. The `ECONF_REGISTER` macro makes the package known to a registry which registers the `--download strumpack` command. The first argument to the macro is the package name, the next is the function that builds and installs it, and the following variadic argument list can contain dependencies. `econf` automatically builds a dependency graph that ensures METIS is built before STRUMPACK (even when the user didn't specify `--download metis`). Dependencies can also be so-called _variant packages_ such as `"mpi"` or `"blas"` in this case: STRUMPACK does not need a specific MPI or BLAS implementation, it just needs _some_ MPI and _some_ BLAS implementation. For variant packages, the user must specifiy a specific implementation via the command line (in the example above we use `--download openblas --download system-mpi` to set the BLAS installation to OpenBLAS and let STRUMPACK use the system's MPI installation; alternatively we could pass `--download mpich` or `--download openmpi`).
