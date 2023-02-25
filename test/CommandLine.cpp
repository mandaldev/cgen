#include "../src/CommandLine.hpp"
#include <iostream>

int main(int argc, const char *argv[])
{
	XO::CommandLine cmdline{argc, argv};
	if (!cmdline.is_valid()) {
		std::cerr << "Command line is invalid\n";
		return 1;
	}

	std::cerr << "Switchs:\n";
	auto switchs = cmdline.get_switchs();
	for (auto const &s : switchs) {
		std::cerr << "\t" << s.first << " = " << s.second << "\n";
	}
	std::cerr << "Arguments:\n";
	auto args = cmdline.get_arguments();
	for (auto const &a : args) {
		std::cerr << "\t" << a << "\n";
	}
	return 0;
}
