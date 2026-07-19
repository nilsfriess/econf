#include "econf.hh"
#include "packages.hh"

using namespace econf;

int main(int argc, char **argv) {
  rebuild_if_necessary(argc, argv);

  // Default parameters (can be overwritten from the command line)
  econf::BuildEnv env = {
      .cc = "/usr/bin/clang",
      .cxx = "/usr/bin/clang++",
  };

  try {
    econf::run(env, argc, argv);
  } catch (const econf::Error &e) {
    econf::log::error("{}", e.what());
  }
}
