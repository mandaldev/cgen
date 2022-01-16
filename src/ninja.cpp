#include "ninja.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

static std::string gen_ninja_rules(std::string const &gcc_cmd)
{
	std::string rules;
	rules += "rule " + NINJA_RULE_COMPILE + "\n"
		 "    deps = gcc\n"
		 "    depfile = $out.d\n"
		 "    command = " + gcc_cmd + " -c -o $out $in $cflags -MD -MF $out.d\n"
		 "\n"

		 "rule " + NINJA_RULE_LINK_EXE + "\n"
		 "    command = " + gcc_cmd + " -o $out $in $lflags\n"
		 "\n"

		 "rule " + NINJA_RULE_LINK_SLIB + "\n"
		 "    command = ar rcs $out $in $lflags\n"
		 "\n"

		 "rule " + NINJA_RULE_LINK_DLIB + "\n"
		 "    command = " + gcc_cmd + " -shared -o $out $in $lflags\n"
		 "\n"
		 ;
	return rules;
}

std::string gen_ninja_build(
	std::string const &gcc_cmd,
	std::string const &cflags,
	std::set<std::string> const &subninja_names
)
{
	std::string build;
	build += NINJA_GLOBAL_CFLAGS + " = " + cflags + "\n\n";
	build += gen_ninja_rules(gcc_cmd);
	for (auto &name : subninja_names) {
		build += "subninja " + name + NINJA_BUILD_EXT + "\n";
	}
	return build;
}

bool gen_ninja_target_build(
	std::string const &profile, // debug or release
	TargetType type,
	std::string const &name,
	std::set<std::string> const &src,
	std::set<std::string> const &dep,
	std::string const &extra_cflags,
	std::string const &extra_lflags
)
{
	std::string cflags = extra_cflags;
	std::string lflags = extra_lflags;
	std::string deps;
	std::string script;
	if (dep.size()) {
		deps = " |";
		for (auto &d : dep) {
			lflags += " -l" + d;
			deps += " _phony_" + d;
		}
		lflags += " -L.";
	}
	std::string out_name, link_rule;
	switch (type) {
	case TargetType::Executable:
		out_name = name;
		link_rule = NINJA_RULE_LINK_EXE;
		break;
	case TargetType::StaticLibrary:
		out_name = "lib" + name + ".a";
		link_rule = NINJA_RULE_LINK_SLIB;
		break;
	case TargetType::DynamicLibrary:
		cflags += " -fPIC";
		out_name = "lib" + name + ".so";
		link_rule = NINJA_RULE_LINK_DLIB;
		break;
	}
	script += "cflags = $" + NINJA_GLOBAL_CFLAGS + " " + cflags + "\n";
	script += "lflags = $" + NINJA_GLOBAL_LFLAGS + " " + lflags + "\n";
	script += "\n";

	std::string obj_list;
	for (auto const &s : src) {
		std::filesystem::path p{ s };
		if (p.has_parent_path()) { // make sure directories exist
			std::error_code ec;
			std::filesystem::create_directories(p.parent_path(), ec);
			if (ec) {
				// error
			}
		}
		script += "build " + s + ".o: " + NINJA_RULE_COMPILE + " ../../" + s + "\n";
		obj_list += s + ".o ";
	}
	script += "build " + out_name + ": " + link_rule + " " + obj_list + deps + "\n";
	script += "build _phony_" + name + ": phony " + out_name + "\n";
	script += "default _phony_" + name + "\n";

	std::string fname = "./build/" + profile + "/" + name + NINJA_BUILD_EXT;
	std::ofstream fs;
	fs.open(fname, std::ios_base::out | std::ios_base::trunc);
	if (fs.fail()) {
		std::cerr
			<< "Failed to open file " << fname
			<< std::endl;
		return false;
	}
	fs << script;
	fs.close();
	std::cerr << fname << " written\n";
	return true;
}
