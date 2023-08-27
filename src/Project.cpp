#include "Project.hpp"
#include "common.hpp"
#include "ninja.hpp"
#include <Com/file.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>

bool Project::load_target(std::string const &target_name, ProjectModel::TargetModel const &tm, std::filesystem::path const &basedir)
{
	auto target = std::make_shared<Target>();
	if (!target->load(target_name, tm, basedir)) {
		std::cerr << "Load target failed\n";
		return false;
	}
	auto name = target->name();
	auto &dst = target->type() == TargetType::Template ? _templates : (target->type() == TargetType::Phony ? _phonys : _targets);
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

bool Project::load_target(std::string const &target_name, ProjectModel::TargetModel const &tm)
{
	return load_target(target_name, tm, "");
}

bool Project::load_target(std::string const &target_name, std::string const &include)
{
	if (include.empty()) {
		std::cerr << "member of 'target' must be empty string\n";
		return false;
	}
	std::filesystem::path path = include;
	auto opt_mas = Com::load_mas_file(path);
	if (!opt_mas) {
		std::cerr << "Error: not a valid mas file: " << path << '\n';
		return false;
	}
	ProjectModel::TargetModel tm;
	auto &mas = opt_mas.value();
	if (!Com::Mas::deserialize(mas, tm)) {
		std::cerr << "Error: must be a map: " << path << '\n';
		return false;
	}
	return load_target(target_name, tm, path.parent_path());
}

bool Project::load_targets(ProjectModel::TargetMap const &targets)
{
	for (auto &p : targets) {
		auto ret = std::visit(
			[this, &p](auto &tm)
			{
				return load_target(p.first, tm);
			},
			p.second
		);
		if (!ret) {
			return false;
		}
	}
	return true;
}

bool Project::load_targets(ProjectModel::TargetArray const &targets)
{
	for (auto &target : targets) {
		auto ret = std::visit(
			[this, &target](auto &tm)
			{
				return load_target("", tm);
			},
			target
		);
		if (!ret) {
			return false;
		}
	}
	return true;
}

std::map<std::string, std::shared_ptr<Target>> const &Project::targets() const
{
	return _targets;
}

std::optional<ProjectModel::Profile> Project::get_profile(std::string const &name) const
{
	auto it = _profile.find(name);
	if (it != _profile.end()) {
		return it->second;
	}
	return {};
}

Pkgconf *Project::pkgconf()
{
	return &_pkgconf;
}

bool Project::load(ProjectModel const &project)
{
	_pm = project;
	_import_dirs = _pm.imports;
	for (auto &p : _import_dirs) {
		p /= "share";
		p /= "cgen";
	}
	{
		auto dirs = cgen_data_dirs();
		_import_dirs.insert(_import_dirs.end(), dirs.begin(), dirs.end());
	}

	setup_default_profiles();

	if (!std::visit(
		[this](auto &targets)
		{
			return load_targets(targets);
		},
		_pm.target
	)) {
		return false;
	}

	for (auto &&[name, target] : _targets) {
		if (!target->apply_templates(_templates)) {
			return false;
		}
	}
	for (auto &&[name, target] : _targets) {
		if (!target->apply_phonys(_phonys)) {
			return false;
		}
	}
	{
		ProjectModel::TargetArray tms;
		tms.reserve(_targets.size());
		for (auto &&[name, target] : _targets) {
			tms.push_back(target->model());
		}
		_pm.target = std::move(tms);
	}
	_project_cached = Com::Mas::encode(Com::Mas::serialize(_pm));

	for (auto &&[name, target] : _targets) {
		if (!target->load_pkgconfs(_pkgconf)) {
			return false;
		}
		if (!target->resolve_deps(_targets, _import_dirs, _imports)) {
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
	auto target = _targets[target_name];
	if (target->type() != TargetType::Executable) {
		for (auto d : target->dep()) {
			auto it = _targets.find(d);
			if (it != _targets.end()) {
				it->second->install(prefix, profile);
			}
		}
	}
	return target->install(prefix, profile);
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
			if (_imports.count(d)) {
				continue;
			}
			std::cerr << "Error: no target named " << d << " found\n";
			return false;
		}
		if (!gen_with_dep(profile, it->second, generated, opt_cdb)) {
			return false;
		}
	}
	return target->gen(profile, _targets, _imports, opt_cdb);
}

void Project::setup_default_profiles()
{
	_profile = _pm.profile;
	if (!_profile.count("debug")) {
		_profile["debug"] = { "-Wall -g -DDEBUG", "", };
	}
	if (!_profile.count("release")) {
		_profile["release"] = { "-Wall -O2 -DNDEBUG", "", };
	}
}

std::string const &Project::name() const
{
	return _pm.name;
}

std::string const &Project::author() const
{
	return _pm.author;
}

std::string const &Project::email() const
{
	return _pm.email;
}

std::string const &Project::license() const
{
	return _pm.license;
}

std::string const &Project::desc() const
{
	return _pm.desc;
}

std::vector<std::filesystem::path> const &Project::import_dirs() const
{
	return _import_dirs;
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
