#include "Project.hpp"
#include "common.hpp"
#include "ninja.hpp"
#include <Com/file.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>

static const auto NAME = "name";
static const auto AUTHOR = "author";
static const auto EMAIL = "email";
static const auto LICENSE = "license";
static const auto DESC = "desc";
static const auto TARGET = "target";
static const auto PROFILE = "profile";
static const auto PROFILE_CFLAGS = "cflags";
static const auto PROFILE_LFLAGS = "lflags";
static const auto INCLUDE = "include";

bool Project::load_target(std::string const &target_name, Com::Mas::Map &m, std::filesystem::path const &basedir)
{
	auto target = std::make_shared<Target>(this);
	if (!target->load(target_name, m, basedir)) {
		std::cerr << "Load target failed\n";
		return false;
	}
	auto name = target->name();
	auto &dst = target->type() == TargetType::Template ? _templates : _targets;
	if (name.length()) {
		if (!dst.count(name)) {
			dst.insert({target->name(), target});
			return true;
		} else {
			std::cerr << "Target must have unique name: " << name << "\n";
		}
	} else {
		std::cerr << "Target name must be non-empty string\n";
	}
	return false;
}

bool Project::load_target(std::string const &target_name, Com::Mas::Value &m)
{
	if (m.is_string()) {
		if (m.as_string().empty()) {
			std::cerr << "member of 'target' must be empty string\n";
			return false;
		}
		return load_include(target_name, m);
	}
	if (!m.is_map()) {
		std::cerr << "member of 'target' must be map or string value\n";
		return false;
	}
	if (!load_target(target_name, m.as_map())) {
		return false;
	}
	return true;
}

bool Project::load_targets(Com::Mas::Value &targets_mas)
{
	if (targets_mas.is_string()) {
		std::cerr << "Target must be array or map value\n";
		return false;
	}
	if (targets_mas.is_array()) {
		for (auto &m : targets_mas.as_array()) {
			if (!load_target("", m)) {
				return false;
			}
		}
	} else {
		for (auto &m : targets_mas.as_map()) {
			if (!load_target(m.first, m.second)) {
				return false;
			}
		}
	}
	return true;
}

bool Project::load_include(std::string const &target_name, Com::Mas::Value &target_mas)
{
	std::filesystem::path path = target_mas.as_string();
	auto opt_mas = Com::load_mas_file(path);
	if (!opt_mas) {
		std::cerr << "Error: not a valid mas file: " << path << '\n';
		return false;
	}
	auto &mas = opt_mas.value();
	if (!mas.is_map()) {
		std::cerr << "Error: must be a map: " << path << '\n';
		return false;
	}
	target_mas = std::move(mas);
	return load_target(target_name, target_mas.as_map(), path.parent_path());
}

std::map<std::string, std::shared_ptr<Target>> const &Project::targets() const
{
	return _targets;
}

std::optional<Profile> Project::get_profile(std::string const &name) const
{
	auto it = _profiles.find(name);
	if (it != _profiles.end()) {
		return it->second;
	}
	return std::optional<Profile>();
}

Pkgconf *Project::pkgconf()
{
	return &_pkgconf;
}

bool Project::load(Com::Mas::Map const &project_mas)
{
	_project_mas = project_mas;
	_project_cached = Com::Mas::encode(_project_mas);
	Com::Mas::get_string(_project_mas, NAME, _name);
	Com::Mas::get_string(_project_mas, AUTHOR, _author);
	Com::Mas::get_string(_project_mas, EMAIL, _email);
	Com::Mas::get_string(_project_mas, LICENSE, _license);
	Com::Mas::get_string(_project_mas, DESC, _desc);

	setup_default_profiles();
	if (auto it = _project_mas.find(PROFILE); it != _project_mas.end()) {
		if (it->second.is_map()) {
			if (!load_profiles(it->second.map_or({}))) {
				return false;
			}
		}
	}

	if (auto it = _project_mas.find(TARGET); it != _project_mas.end()) {
		if (!load_targets(it->second)) {
			return false;
		}
	}
	for (auto &&[name, target] : _targets) {
		if (!target->apply_templates(_templates)) {
			return false;
		}
	}
	return true;
}

bool Project::build_all_targets(std::string const &profile)
{
	return 0 == std::system(("ninja -C ./build/" + profile).c_str());
}

bool Project::build_target(std::string const &profile, std::string const &target_name)
{
	auto it = _targets.find(target_name);
	if (it != _targets.end()) {
		return 0 == std::system(("ninja -C ./build/" + profile + ' ' + target_file_name(target_name, it->second->type())).c_str());
	}
	return false;
}

bool Project::install_all_targets(std::string const &profile, std::filesystem::path const &prefix)
{
	if (!build_all_targets(profile)) {
		return false;
	}
	for (auto &t : _targets) {
		if (!t.second->install(prefix, profile)) {
			return false;
		}
	}
	return true;
}

bool Project::install_target(std::string const &profile, std::filesystem::path const &prefix, std::string const &target_name)
{
	if (!build_target(profile, target_name)) {
		return false;
	}
	return _targets[target_name]->install(prefix, profile);
}

bool Project::configure(std::string const &profile, std::optional<Cdb> &opt_cdb) const
{
	auto changed = true;
	auto build_dir = std::filesystem::path("build") / profile;
	auto ninja_build_path = build_dir / NINJA_BUILD;
	auto cache_path = build_dir / "project.cached";
	if (auto opt_cached_project = Com::load_file(cache_path)) {
		if (opt_cached_project.value() == _project_cached) {
			changed = false;
		}
	}
	if (!changed) {
		return true;
	}
	std::error_code ec;
	std::filesystem::create_directories(ninja_build_path.parent_path(), ec);
	if (ec) {
		// error
		return false;
	}
	if (gen(profile, opt_cdb)) {
		Com::save_file(cache_path, _project_cached);
		return true;
	}
	return false;
}

bool Project::gen_with_dep(std::string const &profile, std::shared_ptr<Target> target, std::set<std::string> &generated, std::optional<Cdb> &opt_cdb) const
{
	if (generated.count(target->name())) { // loop dep?
		return true;
	}
	if (target->type() == TargetType::Phony) {
		return true;
	}
	generated.insert(target->name());
	for (auto &d : target->dep()) {
		auto it = _targets.find(d);
		if (it == _targets.end()) {
			std::cerr << "Error: no target named " << d << " found\n";
			return false;
		}
		if (!gen_with_dep(profile, it->second, generated, opt_cdb)) {
			return false;
		}
	}
	return target->gen(profile, opt_cdb);
}

bool Project::load_profiles(Com::Mas::Map const &mas)
{
	for (auto &p : mas) {
		if (!p.second.is_map()) {
			std::cerr << "Error: profile must be a map\n";
			return false;
		}
		auto &profile = _profiles[p.first];
		auto profile_map = p.second.map_or({});
		Com::Mas::get_string(profile_map, PROFILE_CFLAGS, profile.cflags);
		Com::Mas::get_string(profile_map, PROFILE_LFLAGS, profile.lflags);
	}
	return true;
}

void Project::setup_default_profiles()
{
	_profiles["debug"] = { "-Wall -g -DDEBUG", "", };
	_profiles["release"] = { "-Wall -O2 -DNDEBUG", "", };
}

std::string const &Project::name() const
{
	return _name;
}

std::string const &Project::author() const
{
	return _author;
}

std::string const &Project::email() const
{
	return _email;
}

std::string const &Project::license() const
{
	return _license;
}

std::string const &Project::desc() const
{
	return _desc;
}

bool Project::gen(std::string const &profile_name, std::optional<Cdb> &opt_cdb) const
{
	auto opt_profile = get_profile(profile_name);
	if (!opt_profile.has_value()) {
		std::cerr << "Error: no such profile: " << profile_name << "\n";
		return false;
	}
	auto &profile = opt_profile.value();
	std::set<std::string> generated;
	for (auto &&[name, target] : _targets) {
		if (!target->resolve_deps(_targets)) {
			return false;
		}
	}
	for (auto &&[name, target] : _targets) {
		if (!gen_with_dep(profile_name, target, generated, opt_cdb)) { // generate each target first
			return false;
		}
	}
	if (opt_cdb) {
		for (auto &item : opt_cdb.value()) {
			// add profile (global) cflags
			if (profile.cflags.size()) {
				item.flags = profile.cflags + ' ' + item.flags;
			}
		}
	}
	auto script = gen_ninja_build(profile.cflags, generated);

	std::ofstream fs;
	std::string fname = "./build/" + profile_name + '/' + NINJA_BUILD;
	fs.open(fname, std::ios_base::out | std::ios_base::trunc);
	if (fs.fail()) {
		std::cerr << "Failed to open file " << fname << "\n";
		return false;
	}
	fs << script;
	fs.close();
	std::cerr << fname << " written\n";

	if (opt_cdb) {
		return gen_cdb_file(opt_cdb.value(), "build/" + profile_name);
	}
	return true;
}
