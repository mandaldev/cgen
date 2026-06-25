#include "Target.hpp"
#include "common.hpp"
#include "ninja.hpp"
#include <Com/Mas/serialize.hpp>
#include <Com/file.hpp>
#include <Com/string.hpp>
#include <filesystem>
#include <iostream>
#include <string>
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
	for (auto &p : _tm.pkgconf) {
		if (!pkgconf.find(p)) {
			std::cerr << "Error: pkg-config for '" << p << "' failed: can't find pkg\n";
			return false;
		}
		_pkg_cflags += " " + pkgconf.cflags(p);
		_pkg_lflags += " " + pkgconf.libs(p);
	}
	return true;
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
	if (name.size()) {
		_tm.name = name;
	}
	if (_tm.type == "exe") {
		_type = TargetType::Executable;
	} else if (_tm.type == "slib") {
		_type = TargetType::StaticLibrary;
	} else if (_tm.type == "dlib") {
		_type = TargetType::DynamicLibrary;
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

std::vector<std::string> const &Target::dep() const
{
	return _dep;
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
	_dep = _tm.dep;
	for (auto &d : _tm.dep) {
		std::vector<std::string> indirects;
		auto it = all_targets.find(d);
		if (it == all_targets.end()) {
			if (auto dep = import_target(data_dirs, imports, pkgconf, d)) {
				indirects = dep->dep();
			} else {
				std::cerr << "Error: failed to resolve dep of '" << d << "'\n";
				return false;
			}
		} else if (it->second->resolve_deps(all_targets, data_dirs, imports, pkgconf)) {
			indirects = it->second->dep();
		} else {
			return false;
		}
		_dep.insert(_dep.end(), indirects.begin(), indirects.end());
	}
	if (!load_pkgconfs(pkgconf)) {
		return false;
	}
	return true;
}

bool Target::gen(
	std::string const &profile,
	std::map<std::string, std::shared_ptr<Target>> const &targets,
	std::map<std::string, std::shared_ptr<Target>> const &imports,
	std::vector<std::filesystem::path> const &import_dirs,
	std::optional<Cdb> &opt_cdb
) const
{
	switch (_type) {
	case TargetType::Executable:
	case TargetType::StaticLibrary:
	case TargetType::DynamicLibrary:
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
	cflags.push_back(external_cflags());
	lflags.push_back(external_lflags());

	/// 内部依赖项，只有内部的才会传入 ninja
	std::vector<std::pair<bool, std::string>> inner_deps;
	for (auto &d : _dep) {
		auto it = targets.find(d);
		if (it != targets.end()) {
			inner_deps.push_back({it->second->type() == TargetType::StaticLibrary, d});
		} else if (it = imports.find(d); it == imports.end()) {
			continue;
		}
		// 外部依赖项
		cflags.push_back(it->second->external_cflags());
		lflags.push_back("-l" + it->second->name() + " " + it->second->external_lflags());
	}
	for (auto &path : import_dirs) {
		auto dir = path.string();
		cflags.push_back("-I" + dir + "/include");
		lflags.push_back("-L" + dir + "/lib");
	}
	auto cflag = filter_cflags(cflags);
	if (opt_cdb) {
		CdbItem item;
		item.cmd = cc;
		item.flags = cflag;
		item.files = _tm.src;
		opt_cdb.value().push_back(item);
	}
	return gen_ninja_target_build(
		profile,
		cc,
		_type,
		_tm.name,
		_tm.src,
		inner_deps,
		cflag,
		lflags
	);
}

std::string Target::external_cflags() const
{
	return std::string(Com::trim(_pkg_cflags + " " + _tm.extra_cflags));
}

std::string Target::external_lflags() const
{
	return std::string(Com::trim(_pkg_lflags + " " + _tm.extra_lflags));
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

void Target::append(std::shared_ptr<Target> target)
{
	// append these fields
	_tm.dep.insert(_tm.dep.begin(), target->_tm.dep.begin(), target->_tm.dep.end());
	_tm.pkgconf.insert(_tm.pkgconf.begin(), target->_tm.pkgconf.begin(), target->_tm.pkgconf.end());
	_tm.extra_cflags = target->_tm.extra_cflags + _tm.extra_cflags;
	_tm.extra_lflags = target->_tm.extra_lflags + _tm.extra_lflags;
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
	std::vector<std::string> dep;
	for (auto &d : _tm.dep) {
		if (auto it = all_phonys.find(d); it != all_phonys.end()) {
			if (!it->second->apply_phonys(all_phonys)) {
				return false;
			}
			auto &indirect = it->second->_tm;
			dep.insert(dep.end(), indirect.dep.begin(), indirect.dep.end());
			_tm.pkgconf.insert(_tm.pkgconf.end(), indirect.pkgconf.begin(), indirect.pkgconf.end());
			_tm.extra_cflags = Com::join(" ", _tm.extra_cflags, indirect.extra_cflags);
			_tm.extra_lflags = Com::join(" ", _tm.extra_lflags, indirect.extra_lflags);
		} else {
			dep.push_back(d);
		}
	}
	_tm.dep = std::move(dep);
	return true;
}

static std::filesystem::path install_file(std::filesystem::path const &p, std::filesystem::path const &dir)
{
	std::error_code ec{};
	std::filesystem::create_directories(dir, ec);
	if (ec) {
		std::cerr << "Error: failed to create directory " << dir << ", error: " << ec << "\n";
		return {};
	}
	auto dst = dir / p.filename();
	std::filesystem::copy_file(p, dst, std::filesystem::copy_options::overwrite_existing, ec);
	if (ec) {
		std::cerr << "Error: failed to install file " << p << ", error: " << ec << "\n";
		return {};
	}
	std::cerr << "Succeed to install " << p << " to " << dir << "\n";
	return dst;
}

bool uninstall_list(std::filesystem::path const &p)
{
	auto opt = Com::load_mas_file(p);
	if (!opt) {
		std::cerr << "Error: [UNINSTALL] unable to load list file: " << p << "\n";
		return false;
	}
	std::vector<std::filesystem::path> paths;
	if (!Com::Mas::deserialize(opt.value(), paths)) {
		std::cerr << "Error: [UNINSTALL] not a valid list file: " << p << "\n";
		return false;
	}
	for (auto const &p : paths) {
		std::error_code ec;
		std::filesystem::remove(p, ec);
		if (ec) {
			std::cerr << "Error: [UNINSTALL] failed to remove file: " << p << "\n";
			return false;
		}
	}
	std::cerr << "[UNINSTALL] " << p << " uninstalled.\n";
	return true;
}

bool Target::install(std::filesystem::path const &prefix, std::string const &profile) const
{
	std::error_code ec;
	auto list_file_path = prefix / "share" / "cgen" / INSTALLED_FOLDER / (name() + ".list");
	// uninstall first
	if (std::filesystem::exists(list_file_path, ec)) {
		if (!uninstall_list(list_file_path)) {
			return false;
		}
	}

	std::vector<std::filesystem::path> installed;
	auto target_file = target_file_name(_tm.name, _type);
	if (!target_file.empty()) {
		auto dst = install_file("./build/" + profile + "/" + target_file, prefix / target_install_subdir(_type));
		if (dst.empty()) {
			return false;
		}
		installed.push_back(dst);
	}
	for (auto &item : _tm.install) {
		for (auto &file : item.files) {
			auto dst = install_file(file, prefix / item.dir);
			if (dst.empty()) {
				return false;
			}
			installed.push_back(dst);
		}
		for (auto &file : item.headers) {
			auto dst = install_file(file, prefix / "include" / item.dir);
			if (dst.empty()) {
				return false;
			}
			installed.push_back(dst);
		}
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
	if (_type == TargetType::StaticLibrary || _type == TargetType::DynamicLibrary) {
		mas = Com::Mas::serialize_kvs(
			COM_KV(tm, name),
			COM_KV(tm, language),
			COM_KV(tm, std),
			COM_KV(tm, type),
			COM_KV(tm, dep),
			COM_KV(tm, pkgconf),
			COM_KV(tm, extra_cflags),
			COM_KV(tm, extra_lflags)
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
