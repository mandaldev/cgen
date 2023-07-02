#ifndef MAS_SRC_COMMON_HPP
#define MAS_SRC_COMMON_HPP
#include <string>
#include <cctype>
#include <algorithm>
#include <ctime>
#include <locale>

using namespace std::literals;

enum class TargetType {
	None,
	Phony,
	Template,
	Executable,
	StaticLibrary,
	DynamicLibrary,
}; // enum class Type

struct Profile
{
	std::string cflags;
	std::string lflags;
};

static std::string const PROJECT_MAS = "project.mas";

static const auto LICENSES_FOLDER = "licenses";

#endif
