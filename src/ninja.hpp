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
static constexpr auto SHAERD_TARGET_NINJA_BUILD_PREFIX = "so_";
/// ninja build entry script
static constexpr auto NINJA_BUILD = "build.ninja";

static constexpr auto NINJA_GLOBAL_CFLAGS = "g_cflags";
static constexpr auto NINJA_GLOBAL_LFLAGS = "g_lflags";
static constexpr auto NINJA_RULE_SCOMPILE = "scompile";
static constexpr auto NINJA_RULE_DCOMPILE = "dcompile";
static constexpr auto NINJA_RULE_LINK_EXE = "link_exe";
static constexpr auto NINJA_RULE_LINK_SLIB = "link_slib";
static constexpr auto NINJA_RULE_LINK_DLIB = "link_dlib";

/// generate ninja build entry script
/// `cflags`: global cflags
/// `subninja_names`: target names
std::string gen_ninja_build(
	Profile const &profile,
	std::set<std::string> const &libs,
	std::set<std::string> const &exes
);

std::string filter_cflags(std::vector<std::string> const &flags);

bool gen_one_ninja_target_build(
	std::string const &profile_name,
	bool shared,
	bool build_shared,
	std::string const &cc,
	TargetType type,
	std::string const &name,
	std::set<std::filesystem::path> const &src,
	std::vector<std::string> const &inner_deps,
	std::vector<std::string> const &extra_cflags,
	std::vector<std::string> const &extra_lflags
);

std::string target_file_name(std::string const &target_name, TargetType type, bool shared);

std::string target_install_subdir(TargetType type);

#endif
