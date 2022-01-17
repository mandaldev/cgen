#include "Target.hpp"
#include "common.hpp"
#include "ninja.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

static const auto NAME = "name";
static const auto LANG = "language";
static const auto STD = "std";
static const auto TYPE = "type";
static const auto SRC = "src";
static const auto DEP = "dep";

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

bool Target::load(Mas::Map const &target_mas)
{
	std::string lang, type;
	Mas::get_string(target_mas, NAME, _name);
	Mas::get_string(target_mas, LANG, lang);
	Mas::get_string(target_mas, STD, _std);
	Mas::get_string(target_mas, TYPE, type);
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
		auto emsg = load_string_set(it->second, _dep);
		if (emsg.has_value()) {
			std::cerr << "Error: invalid 'dep' of target '" << _name << "': " << emsg.value() << std::endl;
			return false;
		}
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

std::set<std::string> Target::dep() const
{
	return _dep;
}

bool Target::gen(std::map<std::string, TargetType> const &all_dep_type) const
{
	switch (_type) {
	case TargetType::Executable:
	case TargetType::StaticLibrary:
	case TargetType::DynamicLibrary:
		break;
	default:
		return false;
	}
	std::vector<std::string> dep_implicit, dep_order_only;
	for (auto &d : _dep) {
		auto it = all_dep_type.find(d);
		if (it != all_dep_type.end()) {
			switch (it->second) {
			case TargetType::StaticLibrary:
				dep_implicit.push_back(d);
				break;
			case TargetType::Executable:
			case TargetType::DynamicLibrary:
			default:
				dep_order_only.push_back(d);
				break;
			}
		}
	}
	return gen_ninja_target_build(
		"debug",
		_type,
		_name,
		_src,
		dep_implicit,
		dep_order_only,
		"-std=" + _std,
		""
	);
}
