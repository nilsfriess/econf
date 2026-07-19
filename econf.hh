#pragma once

// Umbrella header: pulls in every econf component. Downstream code (econf.cc
// and packages/*.hh) includes only this file. Each header under include/ is
// self-sufficient and guarded, so the order below is for readability, not
// correctness; it follows the dependency direction (leaves first).
#include "include/error.hh"
#include "include/log.hh"
#include "include/env.hh"
#include "include/cmd.hh"
#include "include/build.hh"
#include "include/git.hh"
#include "include/registry.hh"
#include "include/rebuild.hh"
#include "include/cli.hh"
