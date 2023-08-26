#ifndef MAS_SRC_COMMON_HPP
#define MAS_SRC_COMMON_HPP
#include <string>
#include <cctype>
#include <algorithm>
#include <ctime>
#include <locale>
#include <filesystem>
#include <Com/Xo/exec.hpp>

using namespace std::literals;

enum class TargetType {
	None,
	Phony,
	Template,
	Executable,
	StaticLibrary,
	DynamicLibrary,
}; // enum class Type

static std::string const PROJECT_MAS = "project.mas";

static const std::filesystem::path LICENSES_FOLDER = "licenses";
static const std::filesystem::path INSTALLED_FOLDER = "installed";

/// "~/.cgen/cgen"
/// "~/.local/share/cgen"
/// "<prefix>/share/cgen"
/// system folders at last
std::vector<std::filesystem::path> const &cgen_data_dirs();

/// load cgen runtime file at relative `path`
std::string load_runtime_file(std::vector<std::filesystem::path> const &data_dirs, std::filesystem::path const & path);

/// load cgen runtime file at relative `path`
std::string load_runtime_file(std::filesystem::path const & path);

#endif
