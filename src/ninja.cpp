#include "ninja.hpp"
#include "common.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

static std::string gen_ninja_rules()
{
	std::string rules;
	rules += "rule "s + NINJA_RULE_COMPILE + "\n"
		 "    deps = gcc\n"
		 "    depfile = $out.d\n"
		 "    command = $CC -c -o $out $in $cflags -MD -MF $out.d\n"
		 "\n"

		 "rule " + NINJA_RULE_LINK_EXE + "\n"
		 "    command = $CC -o $out $in $lflags\n"
		 "\n"

		 "rule " + NINJA_RULE_LINK_SLIB + "\n"
		 "    command = ar cs $out $in\n"
		 "\n"

		 "rule " + NINJA_RULE_LINK_DLIB + "\n"
		 "    command = $CC -shared -o $out $in $lflags\n"
		 "\n"
		 ;
	return rules;
}

std::string gen_ninja_build(
	std::string const &cflags,
	std::set<std::string> const &subninja_names
)
{
	std::string build;
	build += NINJA_GLOBAL_CFLAGS + " = "s + cflags + "\n\n";
	build += gen_ninja_rules();
	for (auto &name : subninja_names) {
		build += "subninja " + name + NINJA_BUILD_EXT + "\n";
	}
	return build;
}

bool gen_ninja_target_build(
	std::string const &profile, // debug or release
	std::string const &cc,
	TargetType type,
	std::string const &name,
	std::set<std::string> const &src,
	std::vector<std::pair<bool, std::string>> const &dep, /// [implicit or not, dep target name]
	std::string const &extra_cflags,
	std::string const &extra_lflags
)
{
	std::vector<std::string> cflags;
	std::vector<std::string> lflags;
	std::string deps_implicit, deps_order_only;
	bool has_implicit = false, has_order_only = false;
	if (dep.size()) {
		deps_implicit = "|";
		deps_order_only = " ||";
		lflags.push_back("-L.");
		for (auto &&[implicit, name] : dep) {
			if (implicit) {
				has_implicit = true;
				deps_implicit += " _phony_" + name;
			} else {
				has_order_only = true;
				deps_order_only += " _phony_" + name;
			}
			lflags.push_back("-l" + name);
		}
	}
	std::string out_name = target_file_name(name, type);
	std::string link_rule;
	switch (type) {
	case TargetType::Executable:
		link_rule = NINJA_RULE_LINK_EXE;
		break;
	case TargetType::StaticLibrary:
		link_rule = NINJA_RULE_LINK_SLIB;
		break;
	case TargetType::DynamicLibrary:
		cflags.push_back("-fPIC");
		link_rule = NINJA_RULE_LINK_DLIB;
		break;
	default:
		return false;
	}
	cflags.push_back(extra_cflags);
	lflags.push_back(extra_lflags);

	std::string script;
	script += join_two_items("cflags = $"s + NINJA_GLOBAL_CFLAGS, join_strings(cflags, " "), " ") + "\n";
	script += join_two_items("lflags = $"s + NINJA_GLOBAL_LFLAGS, join_strings(lflags, " "), " ") + "\n";
	script += "CC = " + cc + "\n";
	script += "\n";

	std::string obj_list;
	for (auto const &s : src) {
		std::filesystem::path p{s};
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
	script += "build " + out_name + ": " + link_rule + " " + obj_list;
	if (has_implicit) {
		script += deps_implicit;
	}
	if (has_order_only) {
		script += deps_order_only;
	}
	script += "\n";

	script += "build _phony_" + name + ": phony " + out_name + "\n";
	script += "default _phony_" + name + "\n";

	std::string fname = "./build/" + profile + "/" + name + NINJA_BUILD_EXT;
	std::ofstream fs;
	fs.open(fname, std::ios_base::out | std::ios_base::trunc);
	if (fs.fail()) {
		std::cerr << "Failed to open file " << fname << "\n";
		return false;
	}
	fs << script;
	fs.close();
	std::cerr << fname << " written\n";
	return true;
}

std::string target_file_name(std::string const &target_name, TargetType type)
{
	switch (type) {
	case TargetType::StaticLibrary:
		return "lib" + target_name + ".a";
	case TargetType::DynamicLibrary:
		return "lib" + target_name + ".so";
	case TargetType::Executable:
		return target_name;
	default:
		return "";
	}
}

std::string target_install_subdir(TargetType type)
{
	switch (type) {
	case TargetType::StaticLibrary:
	case TargetType::DynamicLibrary:
		return "lib";
	case TargetType::Executable:
		return "bin";
	default:
		return "";
	}
}
