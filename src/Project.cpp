#include "Project.hpp"
#include "ProjectModel.hpp"
#include "Target.hpp"
#include "common.hpp"
#include "ninja.hpp"
#include <Com/file.hpp>
#include <Com/string.hpp>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

bool Project::load_target(std::string const &target_name, ProjectModel::TargetModel const &tm, std::filesystem::path const &basedir)
{
	auto target = std::make_shared<Target>();
	if (!target->load(target_name, tm, basedir)) {
		std::cerr << "[Error] load target failed\n";
		return false;
	}
	auto name = target->name();
	auto &dst = target->type() == TargetType::Template ? _templates : (target->type() == TargetType::Phony ? _phonys : _targets);
	if (name.length()) {
		if (!dst.count(name)) {
			dst.insert({target->name(), target});
			return true;
		} else {
			std::cerr << "[Error] target must have unique name: " << name << "\n";
		}
	} else {
		std::cerr << "[Error] target name must be non-empty string\n";
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
		std::cerr << "[Error] path of target file is not pecified: " << target_name << "\n";
		return false;
	}
	std::filesystem::path path = include;
	auto opt_mas = Com::load_mas_file(path);
	if (!opt_mas) {
		std::cerr << "[Error] not a valid mas file: " << path << '\n';
		return false;
	}
	ProjectModel::TargetModel tm;
	auto &mas = opt_mas.value();
	if (!Com::Mas::deserialize(mas, tm)) {
		std::cerr << "[Error] must be a map: " << path << '\n';
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
			[this](auto &tm) { return load_target("", tm); },
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

std::optional<Profile> Project::get_profile(std::string const &name) const
{
	auto it = _profiles.find(name);
	if (it != _profiles.end()) {
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
	_data_dirs = _pm.imports;
	for (auto &p : _data_dirs) {
		p /= "share";
		p /= "cgen";
	}
	{
		auto dirs = cgen_data_dirs();
		_data_dirs.insert(_data_dirs.end(), dirs.begin(), dirs.end());
	}

	setup_profiles();

	if (!std::visit(
		[this](auto &targets)
		{
			return load_targets(targets);
		},
		_pm.target
	)) {
		return false;
	}

	for (auto &[name, target] : _targets) {
		if (!target->apply_templates(_templates)) {
			return false;
		}
	}
	for (auto &[name, target] : _targets) {
		if (!target->apply_phonys(_phonys)) {
			return false;
		}
	}
	{
		ProjectModel::TargetArray tms;
		tms.reserve(_targets.size());
		for (auto &[name, target] : _targets) {
			tms.push_back(target->model());
		}
		_pm.target = std::move(tms);
	}
	_project_cached = Com::Mas::encode(Com::Mas::serialize(_pm));

	for (auto &[name, target] : _targets) {
		if (!target->resolve_deps(_targets, _data_dirs, _imports, _pkgconf)) {
			return false;
		}
	}
	return true;
}

bool Project::build_all_targets(std::string const &profile)
{
	return 0 == std::system(("ninja -C ./build/" + profile).c_str());
}

bool Project::build_targets(std::string const &profile_name, std::vector<std::string> const &targets)
{
	auto const &profile = _profiles[profile_name];
	std::string names;
	for (auto &name : targets) {
		auto it = _targets.find(name);
		if (it == _targets.end()) {
			std::cerr << "[Error] no such target: " << name << "\n";
			return false;
		}
		if (it->second->type() == TargetType::Executable) {
			Com::join(" ", names, target_file_name(name, it->second->type(), false));
		} else if (it->second->type() == TargetType::Library) {
			if (profile.build_shared) {
				Com::join(" ", names, target_file_name(name, it->second->type(), true));
			}
			if (profile.build_static) {
				Com::join(" ", names, target_file_name(name, it->second->type(), false));
			}
		} else {
			std::cerr << "[Error] no such target type: " << name
				<< " of " << (int)it->second->type() << "\n";
			return false;
		}
	}
	return 0 == std::system(("ninja -C ./build/" + profile_name + ' ' + names).c_str());
}

bool Project::build_target(std::string const &profile_name, std::string const &target_name)
{
	return build_targets(profile_name, {target_name});
}

static const auto SHARED_INSTALL_MSG = "shared library installed, you may need run 'ldconfig' to update ld cache.";

bool Project::install_all_targets(std::string const &profile_name, std::filesystem::path const &prefix)
{
	// if (!build_all_targets(profile_name)) {
	// 	return false;
	// }
	bool has_shared = false;
	auto const &profile = _profiles[profile_name];
	for (auto &t : _targets) {
		if (t.second->model().explicit_install) {
			continue;
		}
		if (!t.second->install(prefix, profile_name, profile)) {
			return false;
		}
		if (t.second->type() == TargetType::Library && profile.build_shared) {
			has_shared = true;
		}
	}

	if (has_shared) {
		std::cerr << SHARED_INSTALL_MSG << std::endl;
	}
	return true;
}

bool Project::install_target(std::string const &profile_name, std::filesystem::path const &prefix, std::string const &target_name)
{
	// if (!build_target(profile_name, target_name)) {
	// 	return false;
	// }
	bool has_shared = false;
	auto target = _targets[target_name];
	auto const &profile = _profiles[profile_name];
	if (target->type() != TargetType::Executable) {
		std::set<std::shared_ptr<Target>> installed;
		for (auto p : {
			profile.build_static ? &target->dep_for_build(false) : nullptr,
			profile.build_shared ? &target->dep_for_build(true) : nullptr
		}) {
			if (!p) {
				continue;
			}
			for (auto const &d : *p) {
				auto it = _targets.find(d.name);
				if (it != _targets.end()) {
					installed.insert(it->second);
				}
			}
		}
		for (auto const &t : installed) {
			t->install(prefix, profile_name, profile);
			if (profile.build_shared && t->type() == TargetType::Library) {
				has_shared = true;
			}
		}
	}
	auto res = target->install(prefix, profile_name, profile);
	if (profile.build_shared && target->type() == TargetType::Library) {
		has_shared = true;
	}
	if (has_shared) {
		std::cerr << SHARED_INSTALL_MSG << std::endl;
	}
	return res;
}

bool Project::configure(std::string const &profile, std::optional<Cdb> &opt_cdb)
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

bool Project::gen_with_dep(
	std::string const &profile_name,
	std::shared_ptr<Target> target,
	std::set<std::string> &generated_libs,
	std::set<std::string> &generated_exes,
	std::optional<Cdb> &opt_cdb
)
{
	auto name = target->name();
	if (generated_libs.count(name) || generated_exes.count(name)) { // loop dep?
		return true;
	}
	if (target->type() == TargetType::Phony) {
		return true;
	}

	auto const &profile = _profiles[profile_name];

	auto gen_deps = [&](bool shared)
	{
		for (auto &d : target->dep_for_build(shared)) {
			auto it = _targets.find(d.name);
			if (it == _targets.end()) {
				if (_imports.count(d.name)) {
					continue;
				}
				std::cerr << "[Error] no target named " << d.name << " found\n";
				return false;
			}
			if (!gen_with_dep(profile_name, it->second, generated_libs, generated_exes, opt_cdb)) {
				return false;
			}
		}
		return true;
	};

	if (target->type() == TargetType::Executable) {
		generated_exes.insert(name);
		auto res = gen_deps(profile.build_shared);
		if (!res) {
			return false;
		}
	} else {
		generated_libs.insert(name);
		if (profile.build_static) {
			auto res = gen_deps(false);
			if (!res) {
				return false;
			}
		}
		if (profile.build_shared) {
			auto res = gen_deps(true);
			if (!res) {
				return false;
			}
		}
	}


	return target->gen(profile_name, profile, _targets, _imports, _pm.imports, opt_cdb);
}

static const auto DEBUG_PROFILE = Profile{
	{}, {"-Wall -g -DDEBUG"},
	{}, {},
};

static const auto RELEASE_PROFILE = Profile{
	{}, {"-Wall -O2 -DNDEBUG"},
	{}, {},
};

void Project::setup_profiles()
{
	_profiles["debug"] = DEBUG_PROFILE;
	_profiles["release"] = RELEASE_PROFILE;
	{
		auto &tmp = _profiles["debug_only_shared"] = DEBUG_PROFILE;
		tmp.build_shared = true;
		tmp.build_static = false;
	}
	{
		auto &tmp = _profiles["debug_with_shared"] = DEBUG_PROFILE;
		tmp.build_shared = true;
		tmp.build_static = true;
	}
	{
		auto &tmp = _profiles["release_only_shared"] = RELEASE_PROFILE;
		tmp.build_shared = true;
		tmp.build_static = false;
	}
	{
		auto &tmp = _profiles["release_with_shared"] = RELEASE_PROFILE;
		tmp.build_shared = true;
		tmp.build_static = true;
	}
	for (auto const &[name, p] : _pm.profile) {
		auto &tmp = _profiles[name];
		if (!p.inherit.empty() && p.inherit != name) {
			if (auto it = _profiles.find(p.inherit); it != _profiles.end()) {
				tmp = it->second;
			} else {
				std::cerr << "[Warning] there is no buildin profile named: " << p.inherit << "\n";
			}
		}
		sequence_append(tmp.cflags, _pm.flatten_flags(p.cflags));
		sequence_append(tmp.lflags, _pm.flatten_flags(p.lflags));
		sequence_append(tmp.cflags_private, _pm.flatten_flags(p.cflags_private));
		sequence_append(tmp.lflags_private, _pm.flatten_flags(p.lflags_private));
		auto build_shared = p.build_shared();
		auto build_static = p.build_static();
		if (tmp.build_shared != build_shared) {
			tmp.build_shared = build_shared;
		}
		if (tmp.build_static != build_static) {
			tmp.build_static = build_static;
		}
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
	return _pm.imports;
}

bool Project::gen(std::string const &profile_name, std::optional<Cdb> &opt_cdb)
{
	auto &profile = _profiles[profile_name];
	std::set<std::string> libs, exes;
	for (auto &[name, target] : _targets) {
		if (!gen_with_dep(profile_name, target, libs, exes, opt_cdb)) { // generate each target first
			return false;
		}
	}

	auto cflags = Com::join(
		" ",
		Com::join(" ", profile.cflags),
		Com::join(" ", profile.cflags_private)
	);
	for (auto &item : opt_cdb.value_or(Cdb())) {
		// add profile (global) cflags
		item.flags = Com::join(" ", cflags, item.flags);
	}
	auto script = gen_ninja_build(profile, libs, exes);

	std::ofstream fs;
	std::string fname = "./build/" + profile_name + '/' + NINJA_BUILD;
	fs.open(fname, std::ios_base::out | std::ios_base::trunc);
	if (fs.fail()) {
		std::cerr << "[Error] failed to open file " << fname << "\n";
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
