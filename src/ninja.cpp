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
	rules += "rule "s + NINJA_RULE_SCOMPILE + "\n"
		 "    deps = gcc\n"
		 "    depfile = $out.d\n"
		 "    command = $CC -c -o $out $in $cflags -MD -MF $out.d\n"
		 "\n"

	         "rule " + NINJA_RULE_DCOMPILE + "\n"
		 "    deps = gcc\n"
		 "    depfile = $out.shared.d\n"
		 "    command = $CC -c -o $out $in $cflags -fPIC -MD -MF $out.shared.d\n"
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
	Profile const &profile,
	std::set<std::string> const &libs,
	std::set<std::string> const &exes
)
{
	std::string build;
	auto cflags = Com::join(" ", Com::join(" ", profile.cflags), Com::join(" ", profile.cflags_private));
	auto lflags = Com::join(" ", Com::join(" ", profile.lflags), Com::join(" ", profile.lflags_private));
	build += NINJA_GLOBAL_CFLAGS + " = "s + cflags + "\n";
	build += NINJA_GLOBAL_LFLAGS + " = "s + lflags + "\n\n";
	build += gen_ninja_rules();
	for (auto &name : libs) {
		if (profile.build_static) {
			build += "subninja " + name + NINJA_BUILD_EXT + "\n";
		}
		if (profile.build_shared) {
			build += "subninja "s + SHAERD_TARGET_NINJA_BUILD_PREFIX + name + NINJA_BUILD_EXT + "\n";
		}
	}
	for (auto &name : exes) {
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

bool gen_one_ninja_target_build(
	std::string const &profile_name,
	bool shared,
	bool build_shared,
	std::string const &cc,
	TargetType type,
	std::string const &name,
	std::set<std::filesystem::path> const &src,
	std::vector<std::string> const &inner_deps,
	std::vector<std::string> const &extra_cflags,
	std::vector<std::string> const &extra_lflags
)
{
	std::vector<std::string> cflags;
	std::vector<std::string> lflags;
	std::string deps_str;
	std::string const phony_name_prefix = shared ? "_phony_so_" : "_phony_";
	std::string dep_ext = ".so";
	std::string dep_phony_name_prefix = "_phony_so_";
	if (type == TargetType::Executable) {
		if (!build_shared) {
			dep_ext = ".a";
			dep_phony_name_prefix = "_phony_";
		}
	} else if (!shared) {
		dep_ext = ".a";
		dep_phony_name_prefix = "_phony_";
	}
	if (inner_deps.size()) {
		lflags.push_back("-L.");
		deps_str = build_shared ? " ||" : "|";
		for (auto &name : inner_deps) {
			deps_str += " " + dep_phony_name_prefix + name;
			lflags.push_back("-l:lib" + name + dep_ext);
		}
	}
	std::string out_name = target_file_name(name, type, shared);
	std::string link_rule;
	switch (type) {
	case TargetType::Executable:
		link_rule = NINJA_RULE_LINK_EXE;
		break;
	case TargetType::Library:
		if (shared) {
			link_rule = NINJA_RULE_LINK_DLIB;
		} else {
			link_rule = NINJA_RULE_LINK_SLIB;
		}
		break;
	default:
		return false;
	}
	sequence_append(cflags, extra_cflags);
	sequence_append(lflags, extra_lflags);

	std::string script;
	script += Com::join(" ", "cflags = $"s + NINJA_GLOBAL_CFLAGS, filter_cflags(cflags)) + "\n";
	script += Com::join(" ", "lflags = $"s + NINJA_GLOBAL_LFLAGS, filter_lflags(lflags)) + "\n";
	script += "CC = " + cc + "\n";
	script += "\n";

	std::string obj_list;
	std::string const obj_ext = shared ? ".PIC.o" : ".o";
	for (auto const &p : src) {
		if (p.has_parent_path()) { // make sure directories exist
			std::error_code ec;
			std::filesystem::create_directories(p.parent_path(), ec);
			if (ec) {
				// error
			}
		}
		auto out = p.string() + obj_ext;
		auto abs_path = std::filesystem::current_path() / "build" / profile_name / "../../" / p;
		auto in = abs_path.string();
		script += "build " + out + ": " + (shared ? NINJA_RULE_DCOMPILE : NINJA_RULE_SCOMPILE) + " " + in + "\n";
		obj_list += out + " ";
	}
	script += "build " + out_name + ": " + link_rule + " " + obj_list;
	if (!deps_str.empty()) {
		script += deps_str;
	}
	script += "\n";

	auto phony_name = phony_name_prefix + name;
	script += "build " + phony_name + ": phony " + out_name + "\n";
	script += "default " + phony_name + "\n";

	std::string fname = "./build/" + profile_name + "/";
	if (shared) {
		fname += SHAERD_TARGET_NINJA_BUILD_PREFIX;
	}
	fname += name + NINJA_BUILD_EXT;
	std::ofstream fs;
	fs.open(fname, std::ios_base::out | std::ios_base::trunc);
	if (fs.fail()) {
		std::cerr << "[Error] failed to open file " << fname << "\n";
		return false;
	}
	fs << script;
	fs.close();
	std::cerr << fname << " written\n";
	return true;
}

std::string target_file_name(std::string const &target_name, TargetType type, bool shared)
{
	switch (type) {
	case TargetType::Library:
		if (shared) {
			return "lib" + target_name + ".so";
		}
		return "lib" + target_name + ".a";
	case TargetType::Executable:
		return target_name;
	default:
		return "";
	}
}

std::string target_install_subdir(TargetType type)
{
	switch (type) {
	case TargetType::Library:
		return "lib";
	case TargetType::Executable:
		return "bin";
	default:
		return "";
	}
}
