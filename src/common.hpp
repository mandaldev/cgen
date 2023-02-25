#ifndef MAS_SRC_COMMON_HPP
#define MAS_SRC_COMMON_HPP
#include <string>
#include <cctype>
#include <algorithm>

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

static inline std::string trim_string_left(std::string const &str)
{
	auto pos = std::find_if_not(str.begin(), str.end(), [](auto c) { return std::isspace(c); });
	return std::string{pos, str.end()};
}

static inline std::string trim_string_right(std::string const &str)
{
	auto pos = std::find_if_not(str.rbegin(), str.rend(), [](auto c) { return std::isspace(c); }) - str.rbegin();
	return std::string{str.begin(), str.end() - pos};
}

static inline std::string trim_string(std::string const &str)
{
	return trim_string_left(trim_string_right(str));
}

/// join two items with delimter
/// if one item is empty then delimter will not be inserted
template <typename Item, typename Delimter>
static inline Item join_two_items(Item const &lhs, Item const &rhs, Delimter const &delimter)
{
	if (lhs.empty() || rhs.empty()) {
		return lhs + rhs;
	}
	return lhs + delimter + rhs;
}

/// join items with `delimter`
/// if one item is empty then delimter will not be inserted
template <typename ItorFirst, typename ItorLast, typename Delimter>
static inline auto join_items(ItorFirst begin, ItorLast end, Delimter const &delimter)
{
	if (begin == end) {
		return decltype(*begin + *begin)();
	}
	auto ret = *begin;
	while (++begin != end) {
		ret = join_two_items(ret, *begin, delimter);
	}
	return ret;
}

/// join strings with `delimter`
/// if one item is empty then delimter will not be inserted
static inline std::string join_strings(std::vector<std::string> const &strs, std::string const &delimter)
{
	return join_items(strs.begin(), strs.end(), delimter);
}

static inline auto split_string(std::string const &str, std::string const &delims)
{
	std::vector<std::string> strs;
	size_t e = 0;
	while (e != str.npos) {
		auto b = str.find_first_not_of(delims, e);
		if (b == str.npos) {
			break;
		}
		e = str.find_first_of(delims, b);
		strs.push_back(std::string(str.data() + b, str.data() + (e == str.npos ? str.size() : e)));
	}
	return strs;
}

#endif
