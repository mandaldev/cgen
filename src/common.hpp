#ifndef MAS_SRC_COMMON_HPP
#define MAS_SRC_COMMON_HPP
#include <string>
#include <cctype>
#include <algorithm>

using namespace std::literals;

enum class TargetType {
	Executable,
	StaticLibrary,
	DynamicLibrary,
}; // enum class Type

static std::string const PROJECT_MAS = "project.mas";

static inline std::string trim_string_left(std::string const &str)
{
	auto pos = std::find_if_not(str.begin(), str.end(), [](auto c) { return std::isspace(c); });
	return std::string{ pos, str.end() };
}

static inline std::string trim_string_right(std::string const &str)
{
	auto pos = std::find_if_not(str.rbegin(), str.rend(), [](auto c) { return std::isspace(c); }) - str.rbegin();
	return std::string{ str.begin(), str.end() - pos };
}

static inline std::string trim_string(std::string const &str)
{
	return trim_string_left(trim_string_right(str));
}

/// join strings with `delimter`
template <typename ItorFirst, typename ItorLast, typename Delimter>
static inline std::string join_strings(ItorFirst begin, ItorLast end, Delimter const &delimter)
{
	if (begin == end) {
		return "";
	}
	auto ret = *begin;
	while (++begin != end) {
		ret += delimter;
		ret += *begin;
	}
	return ret;
}

/// join strings with `delimter` (default as one space)
static inline std::string join_strings(std::vector<std::string> const &strs, std::string const &delimter = " "s)
{
	return join_strings(strs.begin(), strs.end(), delimter);
}

#endif
