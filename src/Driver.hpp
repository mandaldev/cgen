#ifndef MAS_SRC_DRIVER_HPP
#define MAS_SRC_DRIVER_HPP
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <functional>
#include "Value.hpp"
#include "CommandLine.hpp"
#include "Project.hpp"

// Command driver
class Driver
{
public:

	Driver(int argc, char const *const *argv);

	int run();

private:

	int help();

	int build();

	int dot();

private:

	int _argc = 0;
	char const *const *_argv = nullptr;
	XO::CommandLine _cmdline;
	std::map<std::string, std::function<int()>> _cmd;

}; // class Driver

#endif
