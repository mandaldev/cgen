#include "Cdb.hpp"
#include "common.hpp"
#include <Com/string.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cassert>

static inline std::string quote_string(std::string const &str)
{
	return '"' + str + '"';
}

struct CompileCommand
{
	std::vector<std::string> arguments;
	std::string directory;
	std::string file;

	std::string encode() const
	{
		auto out = "{\"arguments\":["s;
		assert(arguments.size());
		for (auto &arg : arguments) {
			assert(arg.size());
			out += quote_string(arg) + ',';
		}
		out.pop_back(); // last ','
		out += "],\"directory\":";
		out += quote_string(directory) + ",\"file\":";
		out += quote_string(file);
		out += "}\n";
		return out;
	}
};

bool gen_cdb_file(Cdb const &cdb, std::filesystem::path const &dir)
{
	std::ofstream fs;
	std::string fname = dir / "compile_commands.json";
	fs.open(fname, std::ios_base::out | std::ios_base::trunc);
	if (fs.fail()) {
		std::cerr << "Failed to open file " << fname << "\n";
		return false;
	}

	fs << "[\n";
	auto first_cc = true;
	auto abs_dir = std::filesystem::absolute(dir).string();
	for (auto &item : cdb) {
		auto args = Com::split(item.flags, " ");
		for (auto s : item.files) {
			CompileCommand cc;
			cc.file = "../.." / s;
			cc.directory = abs_dir;
			cc.arguments.push_back(item.cmd);
			cc.arguments.insert(cc.arguments.end(), args.begin(), args.end());
			cc.arguments.push_back("-c");
			cc.arguments.push_back("-o");
			cc.arguments.push_back(s += ".o");
			cc.arguments.push_back(cc.file);
			if (first_cc) {
				first_cc = false;
			} else {
				fs << ',';
			}
			fs << cc.encode();
		}
	}
	fs << "]";
	fs.close();
	std::cerr << fname << " written\n";
	return true;
}
