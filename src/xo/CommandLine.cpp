#include "CommandLine.hpp"
#include <algorithm>

namespace XO {

CommandLine::CommandLine(int argc, char const *const *argv)
: _argc(argc)
, _argv(argv)
{
	parse();
}

bool CommandLine::is_valid() const
{
	return _valid_syntax;
}

bool CommandLine::has_switch(std::string const &name) const
{
	return _switchs.count(name);
}

std::optional<std::string> CommandLine::get_switch(std::string const &name) const
{
	auto it = _switchs.find(name);
	if (it != _switchs.end())
	{
		return it->second;
	}
	return {};
}

std::map<std::string, std::string> CommandLine::get_switchs() const
{
	return _switchs;
}

std::vector<std::string> CommandLine::get_arguments() const
{
	return _args;
}

void CommandLine::parse()
{
	if (_argc < 1) {
		_valid_syntax = false;
		return;
	}
	for (int i = 1; i < _argc; i++) {
		std::string arg{ _argv[i] };
		auto len = arg.length();
		if (len > 2 && arg[0] == '-' && arg[1] == '-') { // --switch=value
			auto switch_pos = arg.begin() + 2;
			auto assign_pos = std::find(arg.begin() + 2, arg.end(), '=');
			if (assign_pos == arg.end()) {
				_valid_syntax = false;
				return;
			}
			std::string s{ switch_pos, assign_pos };
			std::string v{ assign_pos + 1, arg.end() };
			_switchs.insert({ s, v });
		} else if (len > 1 && arg[0] == '-') { // -switch
			std::string s{ arg.begin() + 1, arg.end() };
			_switchs.insert({ s, "" });
		} else {
			_args.push_back(arg);
		}
	}
	_valid_syntax = true;
}

} // namespace XO
