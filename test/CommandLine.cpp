#include "../src/CommandLine.hpp"
#include <iostream>

int main(int argc, const char *argv[])
{
	XO::CommandLine cmdline{ argc, argv };
	if (!cmdline.is_valid()) {
		std::cerr << "Command line is invalid" << std::endl;
		return 1;
	}

	std::cerr << "Switchs:" << std::endl;
	auto switchs = cmdline.get_switchs();
	for (auto const &s : switchs) {
		std::cerr << "\t" << s.first << " = " << s.second << std::endl;
	}
	std::cerr << "Arguments:" << std::endl;
	auto args = cmdline.get_arguments();
	for (auto const &a : args) {
		std::cerr << "\t" << a << std::endl;
	}
	return 0;
}
