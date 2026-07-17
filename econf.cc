#include "econf.hh"
#include "packages.hh"

using namespace econf;

int main(int argc, char **argv) {
  rebuild_if_necessary(argc, argv);

  log::info("--------------- econf.log ---------------");
  log::info("econf called with args:");
  for (int i = 0; i < argc; ++i)
    log::info("  {}", argv[i]);

  econf::BuildEnv env = {
      .cc = "clang",
      .cxx = "clang++",
  };

  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (arg.starts_with("--download-")) {
      auto name = arg.substr(11);
      if (auto *e = package::Registry::instance().find(name)) {
        for (auto &dep : e->deps) /* resolve/build dep first */
          ;
        e->build(env);
      } else {
        // unknown package, list ctx names for the user
      }
    }
  }
}
