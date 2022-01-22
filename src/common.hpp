#ifndef MAS_SRC_COMMON_HPP
#define MAS_SRC_COMMON_HPP
#include <string>

enum class TargetType {
	Executable,
	StaticLibrary,
	DynamicLibrary,
}; // enum class Type

static std::string const PROJECT_MAS = "project.mas";

#endif
