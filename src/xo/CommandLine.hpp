#ifndef MAS_SRC_COMMANDLINE_HPP
#define MAS_SRC_COMMANDLINE_HPP
#include <map>
#include <vector>
#include <string>
#include <optional>

namespace XO {

/// A command line like this:
///
///   exec-name -switch ... --switch=value ... arg1 arg2 ...
///
/// No matter of orderness of switchs and arguments.
class CommandLine
{
	int _argc = 0;
	char const *const *_argv = nullptr;

	bool _valid_syntax = false;
	std::vector<std::string> _args;
	std::map<std::string, std::string> _switchs;

	void parse();

public:

	CommandLine(int argc, char const *const *argv);

	bool is_valid();

	bool has_switch(std::string const &name) const;

	std::optional<std::string> get_switch(std::string const &name) const;

	std::map<std::string, std::string> get_switchs() const;

	std::vector<std::string> get_arguments() const;

}; // class CommandLine

} // namespace XO

#endif
