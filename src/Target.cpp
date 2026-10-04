#include "Target.hpp"
#include "ProjectModel.hpp"
#include "common.hpp"
#include "ninja.hpp"
#include <Com/Mas/serialize.hpp>
#include <Com/file.hpp>
#include <Com/log.hpp>
#include <Com/string.hpp>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

static const auto LANG_C = "c";
static const auto LANG_CPP = "c++";
static const std::set<std::string> LANG_C_SET = {"c", "C"};
static const std::set<std::string> LANG_CPP_SET = {"c++", "C++", "cpp", "CPP"};

static void load_and_fix_paths(std::set<std::filesystem::path> &paths, std::filesystem::path const &base)
{
	if (!base.empty()) {
		std::set<std::filesystem::path> tmp;
		for (auto &p : paths) {
			tmp.insert(base / p);
		}
		paths = tmp;
	}
}

bool Target::load_pkgconfs(Pkgconf &pkgconf)
{
	for (auto &p : _tm_pkgconf) {
		if (!pkgconf.find(p.name)) {
			std::cerr << "Error: pkg-config for '" << p.name << "' failed: can't find pkg\n";
			return false;
		}
		auto cflags = pkgconf.cflags(p.name, p.static_);
		auto lflags = pkgconf.libs(p.name, p.static_);
		_static.pkg.cflags.push_back(cflags);
		_static.pkg.lflags.push_back(lflags);
		_build_shared.pkg.cflags.push_back(cflags);
		_build_shared.pkg.lflags.push_back(lflags);
		if (!p.private_) {
			_export_shared.pkg.cflags.push_back(cflags);
			_export_shared.pkg.lflags.push_back(lflags);
		}
	}
	return true;
}

bool Target::load_dep_flags(
	std::map<std::string, std::shared_ptr<Target>> const &targets,
	std::map<std::string, std::shared_ptr<Target>> const &imports,
	DepDesc const &for_inner,
	DepDesc const &for_flags,
	std::vector<std::string> &inner_deps,
	std::vector<std::string> &cflags,
	std::vector<std::string> &lflags
)
{
	auto is_static = &for_flags == &_static;
	sequence_append(cflags, for_flags.user.cflags);
	sequence_append(lflags, for_flags.user.lflags);
	sequence_append(cflags, for_flags.pkg.cflags);
	sequence_append(lflags, for_flags.pkg.lflags);

	/// 内部依赖项，只有内部的才会传入 ninja
	for (auto &d : for_inner.dep) {
		auto it = targets.find(d.name);
		if (it != targets.end()) {
			inner_deps.push_back(d.name);
		}
	}
	for (auto &d : for_flags.dep) {
		auto it = targets.find(d.name);
		if (it != targets.end()) {
			if (is_static) {
				sequence_append(cflags, it->second->_static.user.cflags);
				sequence_append(lflags, it->second->_static.user.lflags);
				sequence_append(cflags, it->second->_static.pkg.cflags);
				sequence_append(lflags, it->second->_static.pkg.lflags);
			} else {
				sequence_append(cflags, it->second->_build_shared.user.cflags);
				sequence_append(lflags, it->second->_build_shared.user.lflags);
				sequence_append(cflags, it->second->_build_shared.pkg.cflags);
				sequence_append(lflags, it->second->_build_shared.pkg.lflags);
			}
		} else if (it = imports.find(d.name); it != imports.end()) {
			if (d.static_) {
				sequence_append(cflags, it->second->_static.user.cflags);
				sequence_append(lflags, it->second->_static.user.lflags);
				sequence_append(cflags, it->second->_static.pkg.cflags);
				sequence_append(lflags, it->second->_static.pkg.lflags);
				lflags.push_back("-l:lib" + it->second->name() + ".a");
			} else {
				sequence_append(cflags, it->second->_export_shared.user.cflags);
				sequence_append(lflags, it->second->_export_shared.user.lflags);
				sequence_append(cflags, it->second->_export_shared.pkg.cflags);
				sequence_append(lflags, it->second->_export_shared.pkg.lflags);
				lflags.push_back("-l" + it->second->name());
			}
		} else {
			LOG_ERROR << "Failed to import target: " << d.name;
			return false;
		}
	}
	return true;
}

bool Target::gen_exe(
	std::string const &profile_name,
	Profile const &profile,
	std::map<std::string, std::shared_ptr<Target>> const &targets,
	std::map<std::string, std::shared_ptr<Target>> const &imports,
	std::string const &cc,
	std::vector<std::string> cflags,
	std::vector<std::string> lflags,
	std::optional<Cdb> &opt_cdb
)
{
	auto is_static = !profile.build_shared;
	auto &dep = is_static ? _static : _build_shared;
	/// 内部依赖项，只有内部的才会传入 ninja
	std::vector<std::string> inner_deps;
	if (!load_dep_flags(targets, imports, _static, dep, inner_deps, cflags, lflags)) {
		return false;
	}

	if (opt_cdb) {
		CdbItem item;
		item.cmd = cc;
		item.flags = filter_cflags(cflags);
		item.files = _tm.src;
		opt_cdb.value().push_back(item);
	}
	return gen_one_ninja_target_build(
		profile_name, false, profile.build_shared,
		cc, TargetType::Executable,
		_tm.name, _tm.src,
		inner_deps, cflags, lflags
	);
}

bool Target::gen_lib(
	std::string const &profile_name,
	Profile const &profile,
	std::map<std::string, std::shared_ptr<Target>> const &targets,
	std::map<std::string, std::shared_ptr<Target>> const &imports,
	std::string const &cc,
	DepDesc const &dep,
	std::vector<std::string> cflags,
	std::vector<std::string> lflags,
	std::optional<Cdb> &opt_cdb
)
{
	auto is_static = &dep == &_static;
	/// 内部依赖项，只有内部的才会传入 ninja
	std::vector<std::string> inner_deps;
	if (!load_dep_flags(targets, imports, dep, dep, inner_deps, cflags, lflags)) {
		return false;
	}

	if (opt_cdb) {
		CdbItem item;
		item.cmd = cc;
		item.flags = filter_cflags(cflags);
		item.files = _tm.src;
		opt_cdb.value().push_back(item);
	}
	return gen_one_ninja_target_build(
		profile_name, !is_static, profile.build_shared,
		cc, TargetType::Library,
		_tm.name, _tm.src,
		inner_deps, cflags, lflags
	);
}

void Target::load_installs()
{
	for (auto &item : _tm.install) {
		load_and_fix_paths(item.files, _basedir);
		load_and_fix_paths(item.headers, _basedir);
	}
}

bool Target::load(std::string const &name, ProjectModel::TargetModel const &tm, std::filesystem::path const &basedir)
{
	_tm = tm;
	_tm_dep = ProjectModel::flatten_dep(tm.dep);
	_tm_pkgconf = ProjectModel::flatten_dep(tm.pkgconf);

	_tm_cflags = ProjectModel::flatten_flags(tm.cflags);
	_tm_lflags = ProjectModel::flatten_flags(tm.lflags);
	_tm_cflags_static = ProjectModel::flatten_flags(tm.cflags_static);
	_tm_lflags_static = ProjectModel::flatten_flags(tm.lflags_static);
	_tm_cflags_shared = ProjectModel::flatten_flags(tm.cflags_shared);
	_tm_lflags_shared = ProjectModel::flatten_flags(tm.lflags_shared);
	_tm_cflags_private = ProjectModel::flatten_flags(tm.cflags_private);
	_tm_lflags_private = ProjectModel::flatten_flags(tm.lflags_private);

	_static.user.cflags = _tm_cflags;
	sequence_append(_static.user.cflags, _tm_cflags_static);
	_static.user.lflags = _tm_lflags;
	sequence_append(_static.user.lflags, _tm_lflags_static);

	_build_shared.user.cflags = _tm_cflags;
	sequence_append(_build_shared.user.cflags, _tm_cflags_shared);
	_build_shared.user.lflags = _tm_lflags;
	sequence_append(_build_shared.user.lflags, _tm_lflags_shared);

	_export_shared.user.cflags = _build_shared.user.cflags;
	_export_shared.user.lflags = _build_shared.user.lflags;

	sequence_append(_build_shared.user.cflags, _tm_cflags_private);
	sequence_append(_build_shared.user.lflags, _tm_lflags_private);

	if (name.size()) {
		_tm.name = name;
	}
	if (_tm.type == "exe") {
		_type = TargetType::Executable;
	} else if (_tm.type == "lib") {
		_type = TargetType::Library;
	} else if (_tm.type == "phony") {
		_type = TargetType::Phony;
	} else if (_tm.type == "template") {
		_type = TargetType::Template;
	} else {
		std::cerr << "Unknown target type: " << _tm.type << "\n";
		return false;
	}
	if (LANG_C_SET.count(_tm.language)) {
		_tm.language = LANG_C;
	} else if (LANG_CPP_SET.count(_tm.language)) {
		_tm.language = LANG_CPP;
	} else if (_type != TargetType::Phony && _type != TargetType::Template && _tm.template_.empty()) {
		std::cerr << "Unknown language: " << _tm.language << "\n";
		return false;
	}

	_basedir = basedir;
	load_and_fix_paths(_tm.src, _basedir);

	load_installs();
	return true;
}

std::string Target::name() const
{
	return _tm.name;
}

TargetType Target::type() const
{
	return _type;
}

ProjectModel::TargetModel const &Target::model() const
{
	return _tm;
}

std::vector<ProjectModel::DepDesc> const &Target::dep_for_build(bool shared) const
{
	return shared ? _build_shared.dep : _static.dep;
}

std::vector<ProjectModel::DepDesc> const &Target::dep_for_export(bool shared) const
{
	return shared ? _export_shared.dep : _static.dep;
}

bool Target::resolve_deps(
	std::map<std::string, std::shared_ptr<Target>> const &all_targets,
	std::vector<std::filesystem::path> const &data_dirs,
	std::map<std::string, std::shared_ptr<Target>> &imports,
	Pkgconf &pkgconf
)
{
	if (_deps_resolved) {
		return true;
	}
	_deps_resolved = true; // in case of loop
	for (auto &d : _tm_dep) {
		bool out = false;
		std::vector<ProjectModel::DepDesc> for_static, for_shared;
		auto it = all_targets.find(d.name);
		if (it == all_targets.end()) {
			if (auto dep = import_target(data_dirs, imports, pkgconf, d.name)) {
				out = true;
				for_static = dep->dep_for_export(false);
				for_shared = dep->dep_for_export(true);
			} else {
				std::cerr << "Error: failed to resolve dep of '" << d.name << "'\n";
				return false;
			}
		} else if (it->second->resolve_deps(all_targets, data_dirs, imports, pkgconf)) {
			for_static = it->second->dep_for_export(false);
			for_shared = it->second->dep_for_export(true);
		} else {
			return false;
		}

		_static.dep.push_back(d);
		_build_shared.dep.push_back(d);
		if (!d.private_) {
			_export_shared.dep.push_back(d);
		}
		// 间接依赖
		if (out) {
			if (d.static_) {
				sequence_append(_static.dep, for_static);
				sequence_append(_build_shared.dep, for_static);
				if (!d.private_) {
					sequence_append(_export_shared.dep, for_static);
				}
			} else {
				sequence_append(_static.dep, for_shared);
				sequence_append(_build_shared.dep, for_shared);
				if (!d.private_) {
					sequence_append(_export_shared.dep, for_shared);
				}
			}
		} else {
			sequence_append(_static.dep, for_static);
			sequence_append(_build_shared.dep, for_shared);
			if (!d.private_) {
				sequence_append(_export_shared.dep, for_shared);
			}
		}
	}

	return load_pkgconfs(pkgconf);
}

bool Target::gen(
	std::string const &profile_name,
	Profile const &profile,
	std::map<std::string, std::shared_ptr<Target>> const &targets,
	std::map<std::string, std::shared_ptr<Target>> const &imports,
	std::vector<std::filesystem::path> const &import_dirs,
	std::optional<Cdb> &opt_cdb
)
{
	bool is_lib = false;
	switch (_type) {
	case TargetType::Executable:
		break;
	case TargetType::Library:
		is_lib = true;
		break;
	case TargetType::Phony:
		return true;
	default:
		return false;
	}
	std::string cc;
	if (_tm.language == LANG_C) {
		cc = "gcc";
	} else if (_tm.language == LANG_CPP) {
		cc = "g++";
	} else {
		return false;
	}

	std::vector<std::string> cflags;
	std::vector<std::string> lflags;

	cflags.push_back("-std=" + _tm.std);

	for (auto &path : import_dirs) {
		auto dir = path.string();
		cflags.push_back("-I" + dir + "/include");
		lflags.push_back("-L" + dir + "/lib");
	}

	if (!is_lib) {
		return gen_exe(profile_name, profile, targets, imports, cc, cflags, lflags, opt_cdb);
	}

	if (profile.build_static) {
		if (!gen_lib(profile_name, profile, targets, imports, cc, _static, cflags, lflags, opt_cdb)) {
			return false;
		}
	}
	if (profile.build_shared) {
		auto &&cdb = profile.build_static ? std::optional<Cdb>() : opt_cdb;
		return gen_lib(profile_name, profile, targets, imports, cc, _build_shared, cflags, lflags, cdb);
	}
	return true;
}

bool Target::apply_override(
	std::map<std::string, std::shared_ptr<Target>> const &all_templates,
	std::function<std::string &(ProjectModel::TargetModel &)> field
)
{
	auto temp = _tm.template_;
	while (field(_tm).empty()) {
		if (temp.empty()) {
			break;
		}
		auto it = all_templates.find(temp);
		if (it == all_templates.end()) {
			std::cerr << "Error: Template not found: " << _tm.template_ << "\n";
			return false;
		}
		field(_tm) = field(it->second->_tm);
		temp = it->second->_tm.template_;
	}
	return true;
}

bool Target::apply_append(std::map<std::string, std::shared_ptr<Target>> const &all_templates)
{
	auto temp = _tm.template_;
	while (temp.size()) {
		auto it = all_templates.find(temp);
		if (it == all_templates.end()) {
			std::cerr << "Error: Template not found: " << _tm.template_ << "\n";
			return false;
		}
		append(it->second);
		temp = it->second->_tm.template_;
	}
	return true;
}

void Target::append(std::shared_ptr<Target> target, bool include_deps)
{
	// append these fields
	if (include_deps) {
		sequence_append(_tm_dep, target->_tm_dep);
	}
	sequence_append(_tm_pkgconf, target->_tm_pkgconf);
	sequence_append(_tm_cflags, target->_tm_cflags);
	sequence_append(_tm_lflags, target->_tm_lflags);
	sequence_append(_tm_cflags_static, target->_tm_cflags_static);
	sequence_append(_tm_lflags_static, target->_tm_lflags_static);
	sequence_append(_tm_cflags_shared, target->_tm_cflags_shared);
	sequence_append(_tm_lflags_shared, target->_tm_lflags_shared);
	sequence_append(_tm_cflags_private, target->_tm_cflags_private);
	sequence_append(_tm_lflags_private, target->_tm_lflags_private);
}

std::shared_ptr<Target> Target::import_target(
	std::vector<std::filesystem::path> const &data_dirs,
	std::map<std::string, std::shared_ptr<Target>> &imports,
	Pkgconf &pkgconf,
	std::string const &name
)
{
	auto it = imports.find(name);
	if (it != imports.end()) {
		return it->second;
	}
	auto text = load_runtime_file(data_dirs, INSTALLED_FOLDER / (name + ".mas"));
	if (text.empty()) {
		std::cerr << "Error: external dep not found: " << name << '\n';
		return {};
	}
	ProjectModel::TargetModel tm;
	if (!Com::Mas::deserialize(Com::Mas::decode(text).value_or(""), tm)) {
		std::cerr << "Error: failed to load external dep: " << name << '\n';
		return {};
	}
	auto target = std::make_shared<Target>();
	if (!target->load("", tm)) {
		std::cerr << "Error: failed to load external dep: " << name << '\n';
		return {};
	}
	if (!target->resolve_deps({}, data_dirs, imports, pkgconf)) {
		return {};
	}
	imports[target->name()] = target;
	return target;
}


bool Target::Target::apply_templates(std::map<std::string, std::shared_ptr<Target>> const &all_templates)
{
	if (_tm.template_.empty()) {
		return true;
	}
	auto it = all_templates.find(_tm.template_);
	if (it == all_templates.end()) {
		std::cerr << "Error: Template not found: " << _tm.template_ << "\n";
		return false;
	}

	if (!apply_override(all_templates, &ProjectModel::TargetModel::language)) {
		return false;
	}
	if (_tm.language.empty()) {
		std::cerr << "Error: target's language field not specified, no templates applyed\n";
		return false;
	}
	if (!apply_override(all_templates, &ProjectModel::TargetModel::std)) {
		return false;
	}
	if (_tm.std.empty()) {
		std::cerr << "Error: target's std field not specified, no templates applyed\n";
		return false;
	}

	if (!apply_append(all_templates)) {
		return false;
	}
	return true;
}

bool Target::apply_phonys(std::map<std::string, std::shared_ptr<Target>> const &all_phonys)
{
	if (_phony_applyed) {
		return true;
	}
	_phony_applyed = true;
	std::vector<ProjectModel::DepDesc> dep;
	for (auto const &d : _tm_dep) {
		if (auto it = all_phonys.find(d.name); it != all_phonys.end()) {
			if (!it->second->apply_phonys(all_phonys)) {
				return false;
			}
			sequence_append(dep, it->second->_tm_dep);
			append(it->second, false);
		} else {
			dep.push_back(d);
		}
	}
	_tm_dep = std::move(dep);
	return true;
}

static std::filesystem::path install_file(
	std::filesystem::path const &p,
	std::filesystem::path const &dir,
	std::set<std::filesystem::path> &installed,
	bool &ignored
)
{
	ignored = false;
	std::error_code ec{};
	std::filesystem::create_directories(dir, ec);
	if (ec) {
		std::cerr << "Error: failed to create directory " << dir << ", error: " << ec << "\n";
		return {};
	}
	auto dst = dir / p.filename();
	installed.erase(dst);
	if (std::filesystem::is_symlink(p, ec)) {
		if (std::filesystem::is_symlink(dst, ec)
		&& std::filesystem::read_symlink(p, ec) == std::filesystem::read_symlink(dst, ec)
		&& !ec) {
			ignored = true;
			return dst;
		}
		if (std::filesystem::exists(dst, ec)) {
			std::filesystem::remove(dst, ec);
		}
		std::filesystem::copy_symlink(p, dst, ec);
		if (ec) {
			std::cerr << "Error: failed to install symlink " << p << ", error: " << ec << "\n";
			return {};
		}
		std::cerr << "Succeed to install symlink " << p << " to " << dir << "\n";
		return dst;
	}
	auto res = std::filesystem::copy_file(p, dst, std::filesystem::copy_options::update_existing, ec);
	if (ec) {
		std::cerr << "Error: failed to install file " << p << ", error: " << ec << "\n";
		return {};
	} else if (!res) {
		ignored = true;
		// std::cerr << "Info: file is not updated, skip: " << p << "\n";
	} else {
		std::cerr << "Succeed to install " << p << " to " << dir << "\n";
	}
	return dst;
}

static std::set<std::filesystem::path> load_uninstall_list(std::filesystem::path const &p)
{
	auto opt = Com::load_mas_file(p);
	if (!opt) {
		std::cerr << "Error: [UNINSTALL] unable to load list file: " << p << "\n";
		return {};
	}
	std::set<std::filesystem::path> paths;
	if (!Com::Mas::deserialize(opt.value(), paths)) {
		std::cerr << "Error: [UNINSTALL] not a valid list file: " << p << "\n";
		return {};
	}
	return paths;
}

bool uninstall_list(std::set<std::filesystem::path> const &paths)
{
	for (auto const &p : paths) {
		std::error_code ec;
		std::filesystem::remove(p, ec);
		if (ec) {
			std::cerr << "Error: [UNINSTALL] failed to remove file: " << p << "\n";
			return false;
		}
	}
	return true;
}

bool uninstall_list(std::filesystem::path const &p)
{
	auto paths = load_uninstall_list(p);
	if (!uninstall_list(paths)) {
		return false;
	}
	std::cerr << "[UNINSTALL] " << p << " uninstalled.\n";
	return true;
}

static void list_dir_deep(std::filesystem::path const &dir, std::vector<std::filesystem::path> &files)
{
	std::error_code ec;
	auto di = std::filesystem::recursive_directory_iterator(dir, ec);
	if (ec) {
		return;
	}
	for (auto const &e : di) {
		if (e.is_symlink(ec) || e.is_regular_file(ec)) {
			files.push_back(e.path());
		}
	}
}

bool Target::install(
	std::filesystem::path const &prefix,
	std::string const &profile_name,
	Profile const &profile
) const
{
	std::error_code ec;
	auto list_file_path = prefix / "share" / "cgen" / INSTALLED_FOLDER / (name() + ".list");
	auto old = load_uninstall_list(list_file_path);

	bool is_exe = _type == TargetType::Executable;
	bool is_lib = _type == TargetType::Library;
	if (!is_exe && !is_lib) {
		return false;
	}

	bool ignored = false;
	size_t ignored_count = 0;
	std::vector<std::filesystem::path> installed;
	auto install_target_file = [&](std::string const &target_file)
	{
		auto dst = install_file(
			"./build/" + profile_name + "/" + target_file,
			prefix / target_install_subdir(_type),
			old, ignored
		);
		if (dst.empty()) {
			return false;
		}
		if (ignored) {
			ignored_count += 1;
		}
		installed.push_back(dst);
		return true;
	};
	if (is_exe) {
		auto res = install_target_file(target_file_name(_tm.name, _type, false));
		if (!res) {
			return false;
		}
	} else {
		if (profile.build_static) {
			auto res = install_target_file(target_file_name(_tm.name, _type, false));
			if (!res) {
				return false;
			}
		}
		if (profile.build_shared) {
			auto res = install_target_file(target_file_name(_tm.name, _type, true));
			if (!res) {
				return false;
			}
		}
	}
	for (auto &item : _tm.install) {
		for (auto &file : item.files) {
			std::vector<std::filesystem::path> files;
			auto stat = std::filesystem::symlink_status(file, ec);
			if (!ec && std::filesystem::is_directory(stat)) {
				list_dir_deep(file, files);
			} else {
				files.push_back(file);
			}
			for (auto const &p : files) {
				auto rel = p.lexically_relative(file.parent_path());
				std::filesystem::path dir = item.dir;
				if (rel.has_parent_path()) {
					dir /= rel.parent_path();
				}
				auto dst = install_file(p, prefix / dir, old, ignored);
				if (dst.empty()) {
					return false;
				}
				if (ignored) {
					ignored_count += 1;
				}
				installed.push_back(dst);
			}
		}
		for (auto &file : item.headers) {
			auto dst = install_file(file, prefix / "include" / item.dir, old, ignored);
			if (dst.empty()) {
				return false;
			}
			if (ignored) {
				ignored_count += 1;
			}
			installed.push_back(dst);
		}
	}

	if (!old.empty()) {
		if (uninstall_list(old)) {
			std::cerr << "[UNINSTALL] files left in list: " << list_file_path << "\n";
		} else {
			return false;
		}
	}

	if (ignored_count) {
		std::cerr << "Info: " << ignored_count << " not updated files are skipped\n";
	}
	auto dst = exports(prefix);
	if (dst.empty()) {
		return false;
	}
	installed.push_back(dst);
	installed.push_back(list_file_path);
	if (!Com::save_pretty_mas_file(installed.back(), Com::Mas::serialize(installed))) {
		return false;
	}
	return true;
}

std::filesystem::path Target::exports(std::filesystem::path const &prefix) const
{
	auto tm = _tm;
	Com::Mas::Value mas;
	if (_type == TargetType::Library) {
		mas = Com::Mas::serialize_kvs(
			COM_KV(tm, name),
			COM_KV(tm, language),
			COM_KV(tm, std),
			COM_KV(tm, type),
			"dep", _tm_dep,
			"pkgconf", _tm_pkgconf,
			COM_KV(tm, cflags),
			COM_KV(tm, lflags),
			COM_KV(tm, cflags_static),
			COM_KV(tm, lflags_static),
			COM_KV(tm, cflags_shared),
			COM_KV(tm, lflags_shared)
		);
	} else {
		mas = Com::Mas::serialize_kvs(
			COM_KV(tm, name),
			COM_KV(tm, language),
			COM_KV(tm, std),
			COM_KV(tm, type)
		);
	}
	auto export_file = prefix / "share" / "cgen" / INSTALLED_FOLDER / (name() + ".mas");
	if (!Com::save_pretty_mas_file(export_file, mas)) {
		return {};
	}
	return export_file;
}
