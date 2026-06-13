#include "ninja.hpp"
#include "common.hpp"
#include <Com/string.hpp>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using namespace std::literals;

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
		 "    command = rm -f $out; ar -crs $out $in\n"
		 "\n"

		 "rule " + NINJA_RULE_LINK_DLIB + "\n"
		 "    command = $CC -shared -o $out $in $lflags\n"
		 "\n"
		 ;
	return rules;
}

std::string gen_ninja_build(
	std::string const &cflags,
	std::string const &lflags,
	std::set<std::string> const &subninja_names
)
{
	std::string build;
	build += NINJA_GLOBAL_CFLAGS + " = "s + cflags + "\n";
	build += NINJA_GLOBAL_LFLAGS + " = "s + lflags + "\n\n";
	build += gen_ninja_rules();
	for (auto &name : subninja_names) {
		build += "subninja " + name + NINJA_BUILD_EXT + "\n";
	}
	return build;
}

std::string filter_cflags(std::vector<std::string> const &flags)
{
	auto flag_str = Com::join(" ", flags);
	std::set<std::string_view> includes;
	std::vector<std::string_view> others;
	for (auto flag : Com::split(std::string_view(flag_str), " ")) {
		if (flag.size() > 2 && flag.substr(0, 2) == "-I") {
			includes.insert(flag);
		} else {
			others.push_back(flag);
		}
	}
	return Com::join(" ", Com::join(" ", others), Com::join(" ", includes));
}

static std::string filter_lflags(std::vector<std::string> const &flags)
{
	auto flag_str = Com::join(" ", flags);
	std::map<std::string_view, size_t> libs_indices;
	std::set<std::string_view> dirs;
	std::vector<std::string_view> libs, others;
	size_t i = 0;
	for (auto flag : Com::split(std::string_view(flag_str), " ")) {
		if (flag.size() > 2 && flag.substr(0, 2) == "-l") {
			libs_indices[flag] = i;
		} else if (flag.size() > 2 && flag.substr(0, 2) == "-L") {
			dirs.insert(flag);
		} else {
			others.push_back(flag);
		}
		i += 1;
	}
	std::map<size_t, std::string_view> indiced_libs;
	for (auto const &[lib, i] : libs_indices) {
		indiced_libs[i] = lib;
	}
	for (auto const &[i, lib] : indiced_libs) {
		libs.push_back(lib);
	}
	return Com::join(
		" ",
		Com::join(" ", others),
		Com::join(
			" ",
			Com::join(" ", libs),
			Com::join(" ", dirs)
		)
	);
}

bool gen_ninja_target_build(
	std::string const &profile, // debug or release
	std::string const &cc,
	TargetType type,
	std::string const &name,
	std::set<std::filesystem::path> const &src,
	std::vector<std::pair<bool, std::string>> const &dep, /// [implicit or not, dep target name]
	std::string const &extra_cflags,
	std::vector<std::string> const &extra_lflags
)
{
	std::vector<std::string> cflags;
	std::vector<std::string> lflags;
	std::string deps_implicit, deps_order_only;
	bool has_implicit = false, has_order_only = false;
	if (dep.size()) {
		deps_implicit = "|";
		deps_order_only = " ||";
		std::set<std::string> implicits;
		std::set<std::string> order_onlys;
		lflags.push_back("-L.");
		for (auto &&[implicit, name] : dep) {
			if (implicit) {
				has_implicit = true;
				if (!implicits.count(name)) {
					implicits.insert(name);
					deps_implicit += " _phony_" + name;
				}
			} else {
				has_order_only = true;
				if (!order_onlys.count(name)) {
					order_onlys.insert(name);
					deps_order_only += " _phony_" + name;
				}
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
	lflags.insert(lflags.end(), extra_lflags.begin(), extra_lflags.end());

	std::string script;
	script += Com::join(" ", "cflags = $"s + NINJA_GLOBAL_CFLAGS, filter_cflags(cflags)) + "\n";
	script += Com::join(" ", "lflags = $"s + NINJA_GLOBAL_LFLAGS, filter_lflags(lflags)) + "\n";
	script += "CC = " + cc + "\n";
	script += "\n";

	std::string obj_list;
	for (auto const &p : src) {
		if (p.has_parent_path()) { // make sure directories exist
			std::error_code ec;
			std::filesystem::create_directories(p.parent_path(), ec);
			if (ec) {
				// error
			}
		}
		auto out = p.string();
		auto abs_path = std::filesystem::current_path() / "build" / profile / "../../" / p;
		auto in = abs_path.string();
		script += "build " + out + ".o: " + NINJA_RULE_COMPILE + " " + in + "\n";
		obj_list += out + ".o ";
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
