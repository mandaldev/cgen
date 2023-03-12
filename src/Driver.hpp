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

	bool _has_project = false;
	std::filesystem::path _old_cwd;
	Project _project;

	std::vector<std::filesystem::path> _data_dirs;

 	std::string _author;
 	std::string _desc;
 	std::string _license;
	std::filesystem::path _file_dir;

 	std::string _file_front;

	int load_project();

	int help();

	int build();

	int install();

	int gen_project();

	int gen_class(std::vector<std::string> const &names);

	void setup_data_dirs();

	std::string load_license_header(std::string const &license_name);

	void setup_files_options();

	int gen_files(
		std::vector<std::string> const &names,
		std::string const &type,
		bool match_header,
		std::string const &code
	);
public:
	Driver(int argc, char const *const *argv);

	int run();

}; // class Driver

#endif
