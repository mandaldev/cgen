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
static const auto TEMPLATE = "template";
static const auto INSTALL = "install";
static const auto INSTALL_DIR = "dir";
static const auto INSTALL_FILES = "files";

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

bool Target::load_pkgconfs(Mas::Map const &target_mas)
{
	auto it = target_mas.find(PKGCONF);
	if (it == target_mas.end()) {
		return true;
	}
	std::set<std::string> pkg;
	auto emsg = load_string_set(it->second, pkg);
	if (emsg.has_value()) {
		std::cerr << "Error: invalid 'pkgconf' of target '" << _name << "': " << emsg.value() << std::endl;
		return false;
	}
	_pkg = { pkg.begin(), pkg.end() };

	for (auto &p : _pkg) {
		if (!_project->pkgconf()->find(p)) {
			std::cerr << "Error: pkg-config for " << p << "failed: can't find pkg" << std::endl;
			return false;
		}
		_pkg_cflags += " " + _project->pkgconf()->cflags(p);
		_pkg_lflags += " " + _project->pkgconf()->libs(p);
	}
	return true;
}

bool Target::load_installs(Mas::Map const &target_mas)
{
	auto it = target_mas.find(INSTALL);
	if (it == target_mas.end()) {
		return true;
	}
	if (!it->second.is_array()) {
		std::cerr << "Error: invalid 'install' of target '" << _name << "'\n";
		return false;
	}
	auto &install_mas = it->second.array_or({});
	for (auto &item_mas : install_mas) {
		if (!item_mas.is_map()) {
			std::cerr << "Error: 'install' list element must be a map!\n";
			return false;
		}
		InstallItem item;
		auto &item_map = item_mas.map_or({});
		auto dir = item_map.find(INSTALL_DIR);
		if (dir != item_map.end()) {
			item.dir = dir->second.string_or(item.dir);
		}
		auto files = item_map.find(INSTALL_FILES);
		if (files != item_map.end()) {
			load_string_set(files->second, item.files);
		}
		if (item.files.size()) {
			_installItems.push_back(item);
		}
	}
	return true;
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
	Mas::get_string(target_mas, TEMPLATE, _template);
	if (type == "exe") {
		_type = TargetType::Executable;
	} else if (type == "slib") {
		_type = TargetType::StaticLibrary;
	} else if (type == "dlib") {
		_type = TargetType::DynamicLibrary;
	} else if (type == "phony") {
		_type = TargetType::Phony;
	} else if (type == "template") {
		_type = TargetType::Template;
	} else {
		std::cerr
			<< "Unknown target type: " << type
			<< std::endl;
		return false;
	}
	if (lang == "c") {
		_lang = Lang::C;
	} else if (lang == "c++") {
		_lang = Lang::CPlusPlus;
	} else if (
		_type != TargetType::Phony
		&& _type != TargetType::Template
		&& _template.empty()
	) {
		std::cerr
			<< "Unknown language: " << lang
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
	return load_pkgconfs(target_mas);
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

bool Target::resolve_deps(std::map<std::string, std::shared_ptr<Target>> const &all_targets)
{
	if (_deps_resolved)
		return true;
	_deps_resolved = true; // in case of loop
	auto dep = _dep;
	for (auto &d : _dep) {
		auto it = all_targets.find(d);
		if (it == all_targets.end()) {
			std::cerr << "Error: no target named " << d << " found" << std::endl;
			return false;
		}
		if (!it->second->resolve_deps(all_targets)) {
			return false;
		}
		auto ddep = it->second->dep();
		dep.insert(dep.end(), ddep.begin(), ddep.end());
	}
	_dep = dep;
	return true;
}

bool Target::gen(std::optional<Cdb> &opt_cdb) const
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
	switch (_lang) {
	case Lang::C:
		cc = "gcc";
		break;
	case Lang::CPlusPlus:
		cc = "g++";
		break;
	default:
		return false;
	}

	std::vector<std::string> cflags;
	std::vector<std::string> lflags;
	cflags.push_back("-std=" + _std);
	cflags.push_back(external_cflags());
	lflags.push_back(external_lflags());

	std::vector<std::pair<bool, std::string>> deps;
	for (auto &d : _dep) {
		auto it = _project->targets().find(d);
		if (it != _project->targets().end()) {
			if (it->second->type() != TargetType::Phony) {
				deps.push_back({ it->second->type() == TargetType::StaticLibrary, d });
			}
			cflags.push_back(it->second->external_cflags());
			lflags.push_back(it->second->external_lflags());
		}
	}
	auto cflag = join_strings(cflags);
	auto lflag = join_strings(lflags);
	if (opt_cdb) {
		CdbItem item;
		item.cmd = cc;
		item.flags = cflag;
		item.files = _src;
		opt_cdb.value().push_back(item);
	}
	return gen_ninja_target_build(
		"debug",
		cc,
		_type,
		_name,
		_src,
		deps,
		join_strings(cflags),
		join_strings(lflags)
	);
}

std::string Target::external_cflags() const
{
	return trim_string(_pkg_cflags + " " + _extra_cflags);
}

std::string Target::external_lflags() const
{
	return trim_string(_pkg_lflags + " " + _extra_lflags);
}

bool Target::Target::apply_templates(std::map<std::string, std::shared_ptr<Target>> const &all_templates)
{
	if (_template.empty()) {
		return true;
	}
	auto it = all_templates.find(_template);
	if (it == all_templates.end()) {
		std::cerr << "Error: Template not found: " << _template << std::endl;
		return false;
	}

	auto apply_override = [this, &all_templates](auto field_name, auto op_need_apply, auto op_apply)
	{
		auto temp = _template;
		while (op_need_apply()) {
			if (temp.empty()) {
				std::cerr << "Error: target's " << field_name << " field not specified, no templates applyed\n";
				return false;
			}
			auto it = all_templates.find(temp);
			if (it == all_templates.end()) {
				std::cerr << "Error: Template not found: " << _template << std::endl;
				return false;
			}
			op_apply(it->second);
			temp = it->second->_template;
		}
		return true;
	};
	if (!apply_override(LANG, [this]() { return _lang == Lang::None; }, [this](auto target) { _lang = target->_lang; })) {
		return false;
	}
	if (!apply_override(STD, [this]() { return _std.empty(); }, [this](auto target) { _std = target->_std; })) {
		return false;
	}

	auto apply_append = [this, &all_templates](auto op_apply)
	{
		auto temp = _template;
		while (temp.size()) {
			auto it = all_templates.find(temp);
			if (it == all_templates.end()) {
				std::cerr << "Error: Template not found: " << _template << std::endl;
				return false;
			}
			op_apply(it->second);
			temp = it->second->_template;
		}
		return true;
	};
	if (!apply_append([this](auto target)
	{
		// append these fields
		_dep.insert(_dep.begin(), target->_dep.begin(), target->_dep.end());
		_pkg.insert(_pkg.begin(), target->_pkg.begin(), target->_pkg.end());
		_extra_cflags = target->_extra_cflags + _extra_cflags;
		_extra_lflags = target->_extra_lflags + _extra_lflags;
		_pkg_cflags = target->_pkg_cflags + _pkg_cflags;
		_pkg_lflags = target->_pkg_lflags + _pkg_lflags;
	})) {
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
	return true;
}

bool Target::install(std::filesystem::path const &prefix, std::string const &profile) const
{
	auto target_file = target_file_name(_name, _type);
	if (target_file.empty()) {
		return true;
	}
	if (!install_file("./build/" + profile + "/" + target_file, prefix / target_install_subdir(_type))) {
		return false;
	}
	for (auto &item : _installItems) {
		for (auto &file : item.files) {
			if (!install_file(file, prefix / item.dir)) {
				return false;
			}
		}
	}
	return true;
}
