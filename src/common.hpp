#ifndef MAS_SRC_COMMON_HPP
#define MAS_SRC_COMMON_HPP
#include <filesystem>
#include <string>
#include <vector>

enum class TargetType {
	None,
	Phony,
	Template,
	Executable,
	Library,
}; // enum class Type

struct Profile {
	std::vector<std::string> cflags, cflags_private;
	std::vector<std::string> lflags, lflags_private;

	bool build_static = true;
	bool build_shared = false;
};

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

template <typename Seq>
void sequence_append(Seq &dst, Seq const &src)
{
	dst.insert(dst.end(), src.begin(), src.end());
}

#endif
