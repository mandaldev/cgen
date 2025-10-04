#ifndef MAS_SRC_NINJA_H
#define MAS_SRC_NINJA_H
#include "common.hpp"
#include <filesystem>
#include <set>
#include <string>
#include <vector>

/// common things about 'build':
///
/// - 'cflags': compile time flags, from common to specific
///   - profile common: like '-g'
///   - target common: like '-std=c++17'
///   - target specific:
///     - include flags: '-I'
///     - macro define: '-D'
///     - from 'pkg-config --cflags'
///     - others
/// - 'lflags': link time flags, for 'exe' and dlib'
///   - library: '-l'
///   - library path: '-L'
///   - from 'pkg-config --libs'
///   - others
///
/// |	step	|	exe	|	slib	|	dlib	|
/// |	compile	|	X	|	X	|	X	|
/// |	link	|	X	|		|	X	|
/// |	archive	|		|	X	|		|

/// global binja veriables:
///   - g_cflags: in build.ninja file,  for compile flags
///   - g_lflags: in build.ninja file,  for link flags
///
/// ninja layout:
///   `build.ninja` is top level (entry script) build script,
///   along with target specific `<target>.binja` build script for each target.
///   `build.ninja` will include them all.
///
///   `build.ninja` will also define rules to build the whole project.

/// ninja scripts extension
static constexpr auto NINJA_BUILD_EXT = ".ninja";
/// ninja build entry script
static constexpr auto NINJA_BUILD = "build.ninja";

static constexpr auto NINJA_GLOBAL_CFLAGS = "g_cflags";
static constexpr auto NINJA_GLOBAL_LFLAGS = "g_lflags";
static constexpr auto NINJA_RULE_COMPILE = "compile";
static constexpr auto NINJA_RULE_LINK_EXE = "link_exe";
static constexpr auto NINJA_RULE_LINK_SLIB = "link_slib";
static constexpr auto NINJA_RULE_LINK_DLIB = "link_dlib";

/// generate ninja build entry script
/// `cflags`: global cflags
/// `subninja_names`: target names
std::string gen_ninja_build(
	std::string const &cflags,
	std::string const &lflags,
	std::set<std::string> const &subninja_names
);

/// generate build script for one target
bool gen_ninja_target_build(
	std::string const &profile, /// debug or release
	std::string const &cc,
	TargetType type,
	std::string const &name,
	std::set<std::filesystem::path> const &src,
	std::vector<std::pair<bool, std::string>> const &dep, /// [implicit or not, dep target name]
	std::string const &extra_cflags,
	std::string const &extra_lflags
);

std::string target_file_name(std::string const &target_name, TargetType type);

std::string target_install_subdir(TargetType type);

#endif
