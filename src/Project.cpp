#include "Project.hpp"
#include "common.hpp"
#include "ninja.hpp"
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

static bool load_target(
	Project *project,
	std::string const &target_name,
	Com::Mas::Map const &m,
	std::map<std::string, std::shared_ptr<Target>> &targets,
	std::map<std::string, std::shared_ptr<Target>> &templates
)
{
	auto target = std::make_shared<Target>(project);
	if (!target->load(target_name, m)) {
		std::cerr << "Load target failed\n";
		return false;
	}
	auto name = target->name();
	auto &dst = target->type() == TargetType::Template ? templates : targets;
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
	Com::Mas::get_string(project_mas, NAME, _name);
	Com::Mas::get_string(project_mas, AUTHOR, _author);
	Com::Mas::get_string(project_mas, EMAIL, _email);
	Com::Mas::get_string(project_mas, LICENSE, _license);
	Com::Mas::get_string(project_mas, DESC, _desc);

	setup_default_profiles();
	if (auto it = project_mas.find(PROFILE); it != project_mas.end()) {
		if (it->second.is_map()) {
			if (!load_profiles(it->second.map_or({}))) {
				return false;
			}
		}
	}

	auto it = project_mas.find(TARGET);
	if (it != project_mas.end()) {
		if (it->second.is_string()) {
			std::cerr << "Target must be array or map value\n";
			return false;
		}
		for (auto &m : it->second.array_or({})) {
			if (!m.is_map()) {
				std::cerr << "member of 'target' must be map value\n";
				return false;
			}
			if (!load_target(this, {}, m.map_or({}), _targets, _templates)) {
				return false;
			}
		}
		for (auto &m : it->second.map_or({})) {
			if (!m.second.is_map()) {
				std::cerr << "member of 'target' must be map value\n";
				return false;
			}
			if (!load_target(this, m.first, m.second.map_or({}), _targets, _templates)) {
				return false;
			}
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
	auto ninja_build = "./build/" + profile + '/' + NINJA_BUILD;
	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(ninja_build).parent_path(), ec);
	if (ec) {
		// error
		return false;
	}
	auto in = std::filesystem::last_write_time(PROJECT_MAS, ec);
	auto out = std::filesystem::last_write_time(ninja_build, ec);
	if (ec || in > out) {
		return gen(profile, opt_cdb);
	}
	return true;
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
