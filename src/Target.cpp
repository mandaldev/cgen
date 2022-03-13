#include "Target.hpp"
#include "common.hpp"
#include "ninja.hpp"
#include "Project.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

static const auto NAME = "name";
static const auto LANG = "language";
static const auto STD = "std";
static const auto TYPE = "type";
static const auto SRC = "src";
static const auto DEP = "dep";
static const auto PKGCONF = "pkgconf";
static const auto EXTRA_CFLAGS = "extra_cflags";
static const auto EXTRA_LFLAGS = "extra_lflags";

static std::optional<std::string> load_string_set(Mas::Value const &mas, std::set<std::string> &set)
{
	if (!mas.is_array()) {
		return "Not an array value";
	}
	for (auto &s : mas.array_or({})) {
		if (!s.is_string()) {
			return "Not a string value";
		}
		auto &path = s.string_or({});
		if (path.empty()) {
			return "Empty string";
		}
		if (set.count(path)) {
			return "Not a unique string: " + path;
		}
		set.insert(path);
	}
	return {};
}

Target::Target(Project *project)
: _project(project)
{
}

bool Target::load(Mas::Map const &target_mas)
{
	std::string lang, type;
	Mas::get_string(target_mas, NAME, _name);
	Mas::get_string(target_mas, LANG, lang);
	Mas::get_string(target_mas, STD, _std);
	Mas::get_string(target_mas, TYPE, type);
	Mas::get_string(target_mas, EXTRA_CFLAGS, _extra_cflags);
	Mas::get_string(target_mas, EXTRA_LFLAGS, _extra_lflags);
	if (lang == "c") {
		_lang = Lang::C;
	} else if (lang == "c++") {
		_lang = Lang::CPlusPlus;
	} else {
		std::cerr
			<< "Unknown language: " << lang
			<< std::endl;
		return false;
	}
	if (type == "exe") {
		_type = TargetType::Executable;
	} else if (type == "slib") {
		_type = TargetType::StaticLibrary;
	} else if (type == "dlib") {
		_type = TargetType::DynamicLibrary;
	} else {
		std::cerr
			<< "Unknown target type: " << type
			<< std::endl;
		return false;
	}

	auto it = target_mas.find(SRC);
	if (it != target_mas.end()) {
		auto emsg = load_string_set(it->second, _src);
		if (emsg.has_value()) {
			std::cerr << "Error: invalid 'src' of target '" << _name << "': " << emsg.value() << std::endl;
			return false;
		}
	}
	it = target_mas.find(DEP);
	if (it != target_mas.end()) {
		std::set<std::string> dep;
		auto emsg = load_string_set(it->second, dep);
		if (emsg.has_value()) {
			std::cerr << "Error: invalid 'dep' of target '" << _name << "': " << emsg.value() << std::endl;
			return false;
		}
		_dep = { dep.begin(), dep.end() };
	}
	it = target_mas.find(PKGCONF);
	if (it != target_mas.end()) {
		std::set<std::string> pkg;
		auto emsg = load_string_set(it->second, pkg);
		if (emsg.has_value()) {
			std::cerr << "Error: invalid 'pkgconf' of target '" << _name << "': " << emsg.value() << std::endl;
			return false;
		}
		_pkg = { pkg.begin(), pkg.end() };
	}
	return true;
}

std::string Target::name() const
{
	return _name;
}

TargetType Target::type() const
{
	return _type;
}

std::vector<std::string> Target::dep() const
{
	return _dep;
}

bool Target::load_indirected(std::map<std::string, std::shared_ptr<Target>> const &all_targets)
{
	if (_indirected_loaded)
		return true;
	_indirected_loaded = true; // in case of loop
	auto dep = _dep;
	for (auto &d : _dep) {
		auto it = all_targets.find(d);
		if (it == all_targets.end()) {
			std::cerr << "Error: no target named " << d << " found" << std::endl;
			return false;
		}
		if (!it->second->load_indirected(all_targets)) {
			return false;
		}
		auto ddep = it->second->dep();
		dep.insert(dep.end(), ddep.begin(), ddep.end());
	}
	_dep = dep;
	return true;
}

bool Target::gen() const
{
	std::string cc;
	switch (_lang) {
	case Lang::C:
		cc = "gcc";
		break;
	case Lang::CPlusPlus:
		cc = "g++";
		break;
	}
	switch (_type) {
	case TargetType::Executable:
	case TargetType::StaticLibrary:
	case TargetType::DynamicLibrary:
		break;
	default:
		return false;
	}
	std::vector<std::pair<bool, std::string>> deps;
	for (auto &d : _dep) {
		auto it = _project->targets().find(d);
		if (it != _project->targets().end()) {
			deps.push_back({ it->second->type() == TargetType::StaticLibrary, d });
		}
	}
	std::string pkg_cflags, pkg_lflags;
	for (auto &p : _pkg) {
		if (!_project->pkgconf()->find(p)) {
			std::cerr << "Error: pkg-config for " << p << "failed: can't find pkg" << std::endl;
			return false;
		}
		std::string cflags, lflags;
		pkg_cflags += " " + _project->pkgconf()->cflags(p);
		pkg_lflags += " " + _project->pkgconf()->libs(p);
	}
	return gen_ninja_target_build(
		"debug",
		cc,
		_type,
		_name,
		_src,
		deps,
		"-std=" + _std + pkg_cflags + " " + _extra_cflags,
		pkg_lflags + " " + _extra_lflags
	);
}
