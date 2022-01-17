#ifndef MAS_SRC_NINJA_H
#define MAS_SRC_NINJA_H
#include <string>
#include <vector>
#include <set>
#include "common.hpp"

// common things about 'build':
//
// - 'cflags': compile time flags, from common to specific
//   - profile common: like '-g'
//   - target common: like '-std=c++17'
//   - target specific:
//     - include flags: '-I'
//     - macro define: '-D'
//     - from 'pkg-config --cflags'
//     - others
// - 'lflags': link time flags, for 'exe' and dlib'
//   - library: '-l'
//   - library path: '-L'
//   - from 'pkg-config --libs'
//   - others
//
// |	step	|	exe	|	slib	|	dlib	|
// |	compile	|	X	|	X	|	X	|
// |	link	|	X	|		|	X	|
// |	archive	|		|	X	|		|

// global binja veriables:
//   - g_cflags: in build.ninja file,  for compile flags
//   - g_lflags: in build.ninja file,  for link flags
//
// ninja layout:
//   `build.ninja` is top level (entry script) build script,
//   along with target specific `<target>.binja` build script for each target.
//   `build.ninja` will include them all.
//
//   `build.ninja` will also define rules to build the whole project.

// ninja scripts extension
static std::string const NINJA_BUILD_EXT = ".ninja";
// ninja build entry script
static std::string const NINJA_BUILD = "build.ninja";

static std::string const NINJA_GLOBAL_CFLAGS = "g_cflags";
static std::string const NINJA_GLOBAL_LFLAGS = "g_lflags";
static std::string const NINJA_RULE_COMPILE = "compile";
static std::string const NINJA_RULE_LINK_EXE = "link_exe";
static std::string const NINJA_RULE_LINK_SLIB = "link_slib";
static std::string const NINJA_RULE_LINK_DLIB = "link_dlib";

// generate ninja build entry script
// `cflags`: global cflags
// `gcc_cmd`: gcc cmd to use, gcc or g++
// `include_names`: target names
std::string gen_ninja_build(
	std::string const &gcc_cmd,
	std::string const &cflags,
	std::set<std::string> const &subninja_names
);

bool gen_ninja_target_build(
	std::string const &profile, // debug or release
	TargetType type,
	std::string const &name,
	std::set<std::string> const &src,
	std::vector<std::string> const &dep_implicit,
	std::vector<std::string> const &dep_order_only,
	std::string const &extra_cflags,
	std::string const &extra_lflags
);

#endif
