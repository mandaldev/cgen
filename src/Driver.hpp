#ifndef MAS_SRC_DRIVER_HPP
#define MAS_SRC_DRIVER_HPP
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <functional>
#include "mas/Value.hpp"
#include "xo/CommandLine.hpp"
#include "Project.hpp"

/// Command driver
class Driver
{
	int _argc = 0;
	char const *const *_argv = nullptr;
	XO::CommandLine _cmdline;
	std::map<std::string, std::function<int()>> _cmd;

	int help();

	int build();

	int dot();

public:

	Driver(int argc, char const *const *argv);

	int run();

}; // class Driver

#endif
