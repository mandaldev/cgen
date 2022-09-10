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
static const auto TARGET = "target";

static bool load_target(Project *project, Mas::Map const &m, std::map<std::string, std::shared_ptr<Target>> &targets)
{
	auto target = std::make_shared<Target>(project);
	if (!target->load(m)) {
		std::cerr
			<< "Load target failed"
			<< std::endl
		;
		return false;
	}
	auto name = target->name();
	if (name.length()) {
		if (!targets.count(name)) {
			targets.insert({ target->name(), target });
			return true;
		} else {
			std::cerr
				<< "Target must have unique name: " << name
				<< std::endl
			;
		}
	} else {
		std::cerr
			<< "Target name must be non-empty string"
			<< std::endl
		;
	}
	return false;
}

std::map<std::string, std::shared_ptr<Target>> const &Project::targets() const
{
	return _targets;
}

Pkgconf *Project::pkgconf()
{
	return &_pkgconf;
}

bool Project::load(Mas::Map const &project_mas)
{
	Mas::get_string(project_mas, NAME, _name);
	Mas::get_string(project_mas, AUTHOR, _author);
	Mas::get_string(project_mas, EMAIL, _email);
	Mas::get_string(project_mas, LICENSE, _license);

	auto it = project_mas.find(TARGET);
	if (it != project_mas.end()) {
		if (!it->second.is_array()) {
			std::cerr
				<< "Target must be array value"
				<< std::endl;
			return false;
		}
		for (auto &m : it->second.array_or({})) {
			if (!m.is_map()) {
				std::cerr
					<< "member of 'target' must be map value"
					<< std::endl;
				return false;
			}
			if (!load_target(this, m.map_or({}), _targets)) {
				return false;
			}
		}
	}
	return true;
}

bool Project::build_all_targets()
{
	return 0 == std::system("ninja -C ./build/debug");
}

bool Project::build_target(std::string const &target_name)
{
	auto it = _targets.find(target_name);
	if (it != _targets.end()) {
		return 0 == std::system(("ninja -C ./build/debug " + target_name).c_str());
	}
	return false;
}

bool Project::configure(std::optional<Cdb> &opt_cdb) const
{
	auto ninja_build = "./build/debug/"s + NINJA_BUILD;
	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(ninja_build).parent_path(), ec);
	if (ec) {
		// error
		return false;
	}
	auto in = std::filesystem::last_write_time(PROJECT_MAS, ec);
	auto out = std::filesystem::last_write_time(ninja_build, ec);
	if (ec || in > out) {
		return gen(opt_cdb);
	}
	return true;
}

bool Project::gen_with_dep(std::shared_ptr<Target> target, std::set<std::string> &generated, std::optional<Cdb> &opt_cdb) const
{
	if (generated.count(target->name())) { // loop dep?
		return true;
	}
	generated.insert(target->name());
	for (auto &d : target->dep()) {
		auto it = _targets.find(d);
		if (it == _targets.end()) {
			std::cerr << "Error: no target named " << d << " found" << std::endl;
			return false;
		}
		if (!gen_with_dep(it->second, generated, opt_cdb)) {
			return false;
		}
	}
	return target->gen(opt_cdb);
}

bool Project::gen(std::optional<Cdb> &opt_cdb) const
{
	std::set<std::string> generated;
	for (auto const &t : _targets) {
		if (!t.second->resolve_deps(_targets)) {
			return false;
		}
	}
	for (auto const &t : _targets) {
		if (!gen_with_dep(t.second, generated, opt_cdb)) { // generate each target first
			return false;
		}
	}
	if (opt_cdb) {
		for (auto &item : opt_cdb.value()) {
			// global cfalgs
			item.flags = "-Wall -g " + item.flags;
		}
	}
	auto script = gen_ninja_build("-Wall -g", generated);

	std::ofstream fs;
	std::string fname = "./build/debug/"s + NINJA_BUILD;
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
