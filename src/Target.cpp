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
		if (!it->second.is_array()) {
			std::cerr
				<< "Target's 'src' key must be array value"
				<< std::endl;
			return false;
		}
		for (auto &s : it->second.array_or({})) {
			if (!s.is_string()) {
				std::cerr
					<< "Path of 'src' must be string value"
					<< std::endl;
				return false;
			}
			auto &path = s.string_or({});
			if (path.empty()) {
				std::cerr
					<< "Path in 'src' must not be empty"
					<< std::endl;
				return false;
			}
			if (_src.count(path)) {
				std::cerr
					<< "Every path in 'src' must be unique"
					<< std::endl;
				return false;
			}
			_src.insert(path);
		}
	}
	return true;
}

bool Target::gen() const
{
	switch (_type) {
	case TargetType::Executable:
	case TargetType::StaticLibrary:
	case TargetType::DynamicLibrary:
		return gen_ninja_target_build("debug", _type, _name, _src, "", "");
	default:
		return false;
	}
}
