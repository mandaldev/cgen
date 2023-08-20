#include "Target.hpp"
#include "common.hpp"
#include "ninja.hpp"
#include "Project.hpp"
#include <Com/string.hpp>
#include <Com/Mas/serialize.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>

static const auto NAME = "name";

static const auto LANG = "language";
static const auto LANG_C = "c";
static const auto LANG_CPP = "c++";
static const std::set<std::string> LANG_C_SET = {"c", "C"};
static const std::set<std::string> LANG_CPP_SET = {"c++", "C++", "cpp", "CPP"};

static const auto STD = "std";
static const auto TYPE = "type";
static const auto SRC = "src";
static const auto DEP = "dep";
static const auto PKGCONF = "pkgconf";
static const auto EXTRA_CFLAGS = "extra_cflags";
static const auto EXTRA_LFLAGS = "extra_lflags";
static const auto TEMPLATE = "template";
static const auto INSTALL = "install";
static const auto INSTALL_DIR = "dir";
static const auto INSTALL_FILES = "files";

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

Target::Target(Project *project)
: _project(project)
{
}

bool Target::load_pkgconfs()
{
	for (auto &p : _tm.pkgconf) {
		if (!_project->pkgconf()->find(p)) {
			std::cerr << "Error: pkg-config for " << p << "failed: can't find pkg\n";
			return false;
		}
		_pkg_cflags += " " + _project->pkgconf()->cflags(p);
		_pkg_lflags += " " + _project->pkgconf()->libs(p);
	}
	return true;
}

void Target::load_installs()
{
	for (auto &item : _tm.install) {
		load_and_fix_paths(item.files, _basedir);
	}
}

bool Target::load(std::string const &name, ProjectModel::TargetModel &tm, std::filesystem::path const &basedir)
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
	tm = _tm;
	return load_pkgconfs();
}

std::string Target::name() const
{
	return _tm.name;
}

TargetType Target::type() const
{
	return _type;
}

std::vector<std::string> Target::dep() const
{
	return _dep;
}

bool Target::resolve_deps(std::map<std::string, std::shared_ptr<Target>> const &all_targets)
{
	if (_deps_resolved) {
		return true;
	}
	_deps_resolved = true; // in case of loop
	_dep = _tm.dep;
	for (auto &d : _tm.dep) {
		auto it = all_targets.find(d);
		if (it == all_targets.end()) {
			std::cerr << "Error: no target named " << d << " found\n";
			return false;
		}
		if (!it->second->resolve_deps(all_targets)) {
			return false;
		}
		auto ddep = it->second->dep();
		_dep.insert(_dep.end(), ddep.begin(), ddep.end());
	}
	return true;
}

bool Target::gen(std::string const &profile, std::optional<Cdb> &opt_cdb) const
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

	std::vector<std::pair<bool, std::string>> deps;
	for (auto &d : _dep) {
		auto it = _project->targets().find(d);
		if (it != _project->targets().end()) {
			if (it->second->type() != TargetType::Phony) {
				deps.push_back({it->second->type() == TargetType::StaticLibrary, d});
			}
			cflags.push_back(it->second->external_cflags());
			lflags.push_back(it->second->external_lflags());
		}
	}
	auto cflag = Com::join(" ", cflags);
	auto lflag = Com::join(" ", lflags);
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
		deps,
		Com::join(" ", cflags),
		Com::join(" ", lflags)
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
	const char *field_name,
	std::function<std::string &(ProjectModel::TargetModel &)> field
)
{
	auto temp = _tm.template_;
	while (field(_tm).empty()) {
		if (temp.empty()) {
			std::cerr << "Error: target's " << field_name << " field not specified, no templates applyed\n";
			return false;
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
	_pkg_cflags = target->_pkg_cflags + _pkg_cflags;
	_pkg_lflags = target->_pkg_lflags + _pkg_lflags;
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

	if (!apply_override(all_templates, LANG, &ProjectModel::TargetModel::language)) {
		return false;
	}
	if (!apply_override(all_templates, STD, &ProjectModel::TargetModel::std)) {
		return false;
	}

	if (!apply_append(all_templates)) {
		return false;
	}
	return true;
}

static bool install_file(std::filesystem::path const &p, std::filesystem::path const &dir)
{
	std::error_code ec{};
	std::filesystem::create_directories(dir, ec);
	if (ec) {
		std::cerr << "Error: failed to create directory " << dir << ", error: " << ec << "\n";
		return false;
	}
	std::filesystem::copy_file(p, dir / p.filename(), std::filesystem::copy_options::overwrite_existing, ec);
	if (ec) {
		std::cerr << "Error: failed to install file " << p << ", error: " << ec << "\n";
		return false;
	}
	std::cerr << "Succeed to install " << p << " to " << dir << "\n";
	return true;
}

bool Target::install(std::filesystem::path const &prefix, std::string const &profile) const
{
	auto target_file = target_file_name(_tm.name, _type);
	if (target_file.empty()) {
		return true;
	}
	if (!install_file("./build/" + profile + "/" + target_file, prefix / target_install_subdir(_type))) {
		return false;
	}
	for (auto &item : _tm.install) {
		for (auto &file : item.files) {
			if (!install_file(file, prefix / item.dir)) {
				return false;
			}
		}
	}
	return true;
}

Com::Mas::Value Target::exports() const
{
	return Com::Mas::serialize_kvs(
		COM_KV(_tm, name),
		COM_KV(_tm, language),
		COM_KV(_tm, std),
		COM_KV(_tm, type),
		COM_KV(_tm, dep),
		COM_KV(_tm, pkgconf)
	);
}
