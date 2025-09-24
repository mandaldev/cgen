#ifndef CGEN_SRC_HELP_HPP
#define CGEN_SRC_HELP_HPP

static const auto HELP_MSG = R"(cgen - a C and C++ stuff generator

usage:
  cgen <command> [<args>]

commands:
  help      help message
  build     build target
  install   install target
  project   gen new C/C++ project
  class     gen new C++ class source files (CLASS.cpp and CLASS.hpp)
  cpp       gen new C++ source file
  hpp       gen new C++ header file
  c         gen new C source file
  h         gen new C header file

use 'help COMMAND' see detials about 'COMMAND'

cgen  Copyright (C) 2023  Mandal Dev
This program comes with ABSOLUTELY NO WARRANTY.
This is free software, and you are welcome to redistribute it
under certain conditions.
)";

static const auto HELP_BUILD = R"(cgen build - build targets

cgen build [target]... [OPTION]...

options:
  -cdb    generate compilation database file: compile_commands.json
  --profile=<PROFILE>      build profile to use, default is debug
)";

static const auto HELP_INSTALL = R"(cgen install - install targets

cgen install [target]... [OPTION]...

options:
  --prefix=PREFIX    instal prefix, default '/usr/local'
)";

static const auto HELP_PROJECT = R"(cgen project - gen new project

cgen project NAME [OPTION]...

options:
  --author=AUTHOR          author of new project
  --license=LICENSE        which license your new project is licensed with
  --desc=DESC              description about project
  --lang=c|c++
  --std=c11|c++17|...
  --type=exe|slib|dlib   add a specific target type in new project
)";

static const auto HELP_FILES = R"(cgen class|cpp|hpp|c|h - gen new C/C++ class, sources files...

cgen class|cpp|hpp|c|h NAME... [OPTION]

meta info options(if not provided, cgen use info from current project):
  --author=AUTHOR
  --license=LICENSE
  --desc=DESC

other options:
  --dir=DIR      put files at DIR (relative to current working directory), default '.'
)";

#endif
