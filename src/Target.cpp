#include "Target.hpp"
#include "common.hpp"
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
	load_string(target_mas, NAME, _name);
	load_string(target_mas, LANG, lang);
	load_string(target_mas, STD, _std);
	load_string(target_mas, TYPE, type);
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
		_type = Type::Executable;
	} else if (type == "slib") {
		_type = Type::StaticLibrary;
	} else if (type == "dlib") {
		_type = Type::DynamicLibrary;
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
	//if (_src.empty()) {
	//	std::cerr
	//		<< "Target '" << _name << "' has no sources, skip building"
	//		<< std::endl;
	//	return true;
	//}
	switch (_type) {
	case Type::Executable:
		return gen_exe();
	case Type::StaticLibrary:
		return gen_slib();
	case Type::DynamicLibrary:
		return gen_dlib();
	default:
		return false;
	}
}

bool Target::gen_exe() const
{
	std::string fname = "./build/debug/" + _name + NINJA_BUILD_EXT;
	std::ofstream fs;
	fs.open(fname, std::ios_base::out | std::ios_base::trunc);
	if (fs.fail()) {
		std::cerr
			<< "Failed to open file " << fname
			<< std::endl;
		return false;
	}
	fs
		<< "cflags = $g_cflags -std=" << _std << std::endl
		<< std::endl
		<< "rule compile\n"
		<< "    deps = gcc\n"
		<< "    depfile = $out.d\n"
		<< "    command = g++ $cflags -MD -MF $out.d -c -o $out $in\n"
		<< std::endl
		<< "rule link\n"
		<< "    command = g++ -o $out $in\n"
		<< std::endl
		;
	std::string obj_list;
	for (auto const &s : _src) {
		std::filesystem::path p{ s };
		if (p.has_parent_path()) {
			std::error_code ec;
			std::filesystem::create_directories(p.parent_path(), ec);
			if (ec) {
				// error
			}
		}
		fs << "build " << s << ".o" << ": compile ../../" << s << std::endl;
		obj_list += s + ".o ";
	}
	fs
		<< std::endl
		<< "build " << _name << ": link " << obj_list << std::endl
		<< std::endl
		<< "default " << _name
		<< std::endl
		;
	std::cerr << fname << " written\n";
	return true;
}

bool Target::gen_slib() const
{
	return true;
}

bool Target::gen_dlib() const
{
	return true;
}

