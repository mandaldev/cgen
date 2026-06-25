#ifndef MAS_SRC_DRIVER_HPP
#define MAS_SRC_DRIVER_HPP
#include "Project.hpp"
#include <Com/Xo/CommandLine.hpp>
#include <cassert>
#include <filesystem>
#include <functional>

/// Command driver
class Driver
{
	int _argc = 0;
	char const *const *_argv = nullptr;
	Com::CommandLine _cmdline;
	std::map<std::string, std::function<bool()>> _cmd;

	bool _has_project = false;
	std::filesystem::path _old_cwd;
	Project _project;

 	std::string _author;
 	std::string _desc;
 	std::string _license;
	std::filesystem::path _file_dir;

 	std::string _file_front;

	bool load_project();

	bool help(std::string const &cmd);

	bool build();

	bool install();

	bool gen_project(std::string const &name);

	bool gen_class(std::vector<std::string> const &names);

	bool uninstall();

	bool list();

	std::string load_license(std::string const &license_name);

	std::string load_license_header(std::string const &license_name);

	void setup_files_options(bool preload_project);

	bool gen_files(
		std::filesystem::path const &file_dir,
		std::vector<std::string> const &names,
		std::string const &type,
		bool match_header,
		std::string const &code
	);
public:
	Driver(int argc, char const *const *argv);

	bool run();

}; // class Driver

#endif
