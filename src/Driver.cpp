#include "Driver.hpp"
#include "help.hpp"
#include "common.hpp"
#include <Com/Xo/exec.hpp>
#include <Com/file.hpp>
#include <Com/Date.hpp>
#include <sstream>
#include <fstream>
#include <iostream>

static std::string to_valid_fence_string(std::string const &str)
{
	auto fence = str;
	for (auto &c : fence) {
		c = std::toupper(c);
		switch (c) {
		case 'A' ... 'Z':
		case '0' ... '9':
		case '_':
			continue;
		default:
			c = '_';
		}
	}
	return fence;
}

// return:
//   first: chroot succeed or not
//   second: old cwd
static std::pair<bool, std::filesystem::path> chroot()
{
	auto cur = std::filesystem::current_path();
	auto old_cwd = cur;
	while (!std::filesystem::exists(cur / PROJECT_MAS)) {
		if (!cur.has_relative_path()) {
			return {false, old_cwd};
		}
		cur = cur.parent_path();
	}
	std::filesystem::current_path(cur); // chroot
	std::cerr << "chroot to " << cur << "\n";
	return {true, old_cwd};
}

static std::string files_option_author(Com::CommandLine const &cmdline, std::string def)
{
	def = cmdline.get_switch("author").value_or(def);
	if (def.empty()) {
		def = "<name of author>";
	}
	return def;
}

static std::string files_option_license(Com::CommandLine const &cmdline, std::string def)
{
	return cmdline.get_switch("license").value_or(def.empty() ? "GPLv3" : def);
}

static std::string files_option_desc(Com::CommandLine const &cmdline, std::string def)
{
	def = cmdline.get_switch("desc").value_or(def);
	if (def.empty()) {
		def = "<one line to give the program's name and a brief idea of what it does.>";
	}
	return def;
}

static std::filesystem::path files_option_dir(Com::CommandLine const &cmdline, std::filesystem::path const &cwd)
{
	auto dir = cmdline.get_switch("dir").value_or(".");
	auto h_dir = std::filesystem::path(dir.empty() ? "." : dir);
	std::error_code ec{};
	h_dir = std::filesystem::canonical(h_dir.is_relative() ? cwd / h_dir : h_dir, ec);
	if (ec) {
		std::cerr << "Error: invalid dir path: " << dir << '\n';
		return {};
	}
	return h_dir;
}

static std::string files_comment(std::string const &author, std::string const &desc, std::string const &license_header)
{
	std::ostringstream oss;
	oss << "// " << desc << "\n";
	oss << "// Copyright (C) " << Com::Date::Now::year() << "  " << author << '\n';
	if (license_header.size()) {
		oss << "//\n";
		oss << license_header;
		if (license_header.back() != '\n') {
			oss << '\n';
		}
	}
	return oss.str();
}

static std::string header_fence_of_dir(std::filesystem::path const &file_dir, std::string const &project_name)
{
	auto root = std::filesystem::current_path();
	auto rel_path = file_dir.lexically_relative(root);
	std::ostringstream oss;
	if (project_name.size()) {
		oss << project_name << '_';
	}
	for (auto &node : rel_path) {
		if (!node.empty()) {
			oss << node.string() << '_';
		}
	}
	return oss.str();
}

static std::filesystem::path validate_file_path(std::filesystem::path const &dir, std::string const &file_name)
{
	auto file_path = dir / file_name;
	std::error_code ec{};
	if (std::filesystem::exists(file_path, ec) || ec) {
		std::cerr << "Warning: ignore generating " << file_path << ": file already exists!\n";
		return {};
	}
	return file_path;
}

static bool write_file(std::filesystem::path const &path, std::string const &buf)
{
	std::ofstream ofs(path);
	if (!ofs.write(buf.data(), buf.size())) {
		std::cerr << "Error: failed to write file: " << path << '\n';
		return false;
	}
	std::cerr << "Succeed: file " << path << " generated\n";
	return true;
}

Driver::Driver(int argc, char const *const *argv)
: _argc(argc)
, _argv(argv)
, _cmdline(argc - 1, argv + 1)
{
	_cmd["help"] = [&]() { return help(""); };
	_cmd["build"] = [&]() { return build(); };
	_cmd["install"] = [&]() { return install(); };
	_cmd["project"] = [&]()
	{
		auto args = _cmdline.get_arguments();
		if (args.size() != 1) {
			help("project");
			return false;
		}
		setup_files_options(false);
		return gen_project(args[0]);
	};
	_cmd["class"] = [&]()
	{
		setup_files_options(true);
		return gen_class(_cmdline.get_arguments());
	};
	_cmd["cpp"] = [&]()
	{
		setup_files_options(true);
		return gen_files(_file_dir, _cmdline.get_arguments(), "cpp", false, "");
	};
	_cmd["hpp"] = [&]()
	{
		setup_files_options(true);
		return gen_files(_file_dir, _cmdline.get_arguments(), "hpp", false, "");
	};
	_cmd["c"] = [&]()
	{
		setup_files_options(true);
		return gen_files(_file_dir, _cmdline.get_arguments(), "c", false, "");
	};
	_cmd["h"] = [&]()
	{
		setup_files_options(true);
		return gen_files(_file_dir, _cmdline.get_arguments(), "h", false, "");
	};
}

bool Driver::run()
{
	if (_argc == 1) {
		help("");
		return true;
	}
	if (!_cmdline.is_valid()) {
		std::cerr << "Invalid option or argument, see Usage\n\n";
		help("");
		return false;
	}
	auto cmd = _argv[1];
	auto it = _cmd.find(cmd);
	if (it != _cmd.end()) {
		return it->second();
	}
	std::cerr << "Unknown commond: " << cmd << "\n";
	return false;
}

bool Driver::load_project()
{
	auto res = chroot();
	if (!res.first) {
		std::cerr << "Not a cgen project path\n";
		return false;
	}
	_old_cwd = res.second;

	auto opt_mas = Com::load_mas_file(PROJECT_MAS);
	if (!opt_mas.has_value() || !opt_mas.value().is_map()) {
		std::cerr << "Not a valid cgen project file\n";
		return false;
	}
	auto &mas = opt_mas.value();
	ProjectModel pm;
	if (!Com::Mas::deserialize(mas, pm)) {
		std::cerr << "Failed to load cgen project file\n";
		return false;
	}

	if (!_project.load(pm)) {
		std::cerr << "Failed to load cgen project file\n";
		return false;
	}
	_has_project = true;
	return true;
}

bool Driver::help(std::string const &cmd)
{
	auto args = _cmdline.get_arguments();
	std::map<std::string, std::string> cmd_help = {
		{ "build", HELP_BUILD },
		{ "project", HELP_PROJECT },
		{ "class", HELP_FILES },
		{ "cpp", HELP_FILES },
		{ "hpp", HELP_FILES },
		{ "c", HELP_FILES },
		{ "h", HELP_FILES },
	};
	auto it = cmd_help.find(cmd.size() || args.empty() ? cmd : args[0]);
	if (it != cmd_help.end()) {
		std::cerr << it->second;
	} else {
		std::cerr << HELP_MSG;
	}
	return true;
}

bool Driver::build()
{
	if (!load_project()) {
		return false;
	}
	std::optional<Cdb> opt_cdb;
	if (_cmdline.has_switch("cdb")) {
		opt_cdb = Cdb();
	}
	std::string profile = _cmdline.get_switch("profile").value_or("debug");
	if (!_project.get_profile(profile).has_value()) {
		std::cerr << "Error: no such profile: " << profile << "\n";
		return false;
	}
	if (!_project.configure(profile, opt_cdb)) {
		std::cerr << "Failed to configure project\n";
		return false;
	}
	auto targets_name = _cmdline.get_arguments();
	if (targets_name.empty()) {
		return _project.build_all_targets(profile);
	}
	for (auto const &name : targets_name) {
		if (!_project.build_target(profile, name)) {
			std::cerr << "Aborting...\n";
			return false;
		}
	}
	return true;
}

bool Driver::install()
{
	if (!load_project()) {
		return false;
	}
	std::filesystem::path prefix = _cmdline.get_switch("prefix").value_or("/usr/local");
	std::string profile = _cmdline.get_switch("profile").value_or("debug");
	if (!_project.get_profile(profile).has_value()) {
		std::cerr << "Error: no such profile: " << profile << "\n";
		return false;
	}
	auto targets_name = _cmdline.get_arguments();
	if (targets_name.empty()) {
		return _project.install_all_targets(profile, prefix);
	}
	for (auto const &name : targets_name) {
		if (!_project.install_target(profile, prefix, name)) {
			std::cerr << "Aborting...\n";
			return false;
		}
	}
	return true;
}

static const auto MAIN_CODE = R"(
int main(int argc, const char *argv[])
{
	return 0;
}
)";

static const auto LIB_HEAD_CODE = R"(
void do_something();
)";

static const auto LIB_DECL_CODE = R"(
void do_something()
{
}
)";

bool Driver::gen_project(std::string const &name)
{
	auto lang = _cmdline.get_switch("lang").value_or("c++");
	if (lang != "c++" && lang != "c") {
		std::cerr << "Error: wrong 'lang' type, only 'c' or 'c++' supported!\n";
		return false;
	}
	auto std = _cmdline.get_switch("std").value_or("c++17");
	auto type = _cmdline.get_switch("type").value_or("exe");
	if (type != "exe" && type != "slib" && type != "dlib") {
		std::cerr << "Error: wrong 'target' type, only 'bin', 'slib' or 'dlib' supported!\n";
		return false;
	}
	struct Files
	{
		std::string name;
		std::string type;
		bool match_header;
		std::string code;
	};
	std::vector<Files> files;
	if (type == "exe") {
		files.push_back(Files{"main", lang == "c" ? "c" : "cpp", false, MAIN_CODE});
	} else if (type == "slib" || type == "dlib") {
		files.push_back(Files{name, lang == "c" ? "c" : "cpp", true, LIB_DECL_CODE});
		files.push_back(Files{name, lang == "c" ? "h" : "hpp", true, LIB_HEAD_CODE});
	}

	std::string cgen;
	cgen += "{\n";
	cgen += "  name \"" + name + "\"\n";
	cgen += "  author \"" + _author + "\"\n";
	cgen += "  desc \"" + _desc + "\"\n";
	cgen += "  license \"" + _license + "\"\n";
	cgen += "\n";
	cgen += "  target {\n";
	cgen += "    " + name + " {\n";
	cgen += "      language \"" + lang + "\"\n";
	cgen += "      std \"" + std + "\"\n";
	cgen += "      type \"" + type + "\"\n";
	cgen += "\n";
	cgen += "      src [\n";
	cgen += "        \"src/" + files[0].name + "." + files[0].type + "\"\n";
	cgen += "      ]\n";
	cgen += "    }\n";
	cgen += "  }\n";
	cgen += "}\n";

	if (_file_dir.empty()) {
		_file_dir = ".";
	}
	_file_dir /= name;
	std::error_code ec{};
	if (!std::filesystem::create_directory(_file_dir, ec) || ec) {
		std::cerr << "Error: failed to create project directory: " << ec << "\n";
		return false;
	}

	std::filesystem::current_path(_file_dir, ec);
	if (ec) {
		std::cerr << "Error: failed cd to project directory: " << ec << "\n";
		return false;
	}
	if (!write_file(_file_dir / "project.mas", cgen)) {
		return false;
	}
	if (!load_project()) {
		return false;
	}

	auto license_txt = load_license(_license);
	if (license_txt.size()) {
		if (!write_file(_file_dir / "LICENSE", license_txt)) {
			return false;
		}
	}

	auto src = _file_dir / "src";
	if (!std::filesystem::create_directory(src, ec) || ec) {
		std::cerr << "Error: failed to create src directory: " << ec << "\n";
		return false;
	}

	for (auto &file : files) {
		if (!gen_files(src, {file.name}, file.type, file.match_header, file.code)) {
			return false;
		}
	}

	return true;
}

bool Driver::gen_class(std::vector<std::string> const &names)
{
	for (auto &name : names) {
		std::string code;
		code += "\nclass " + name + '\n';
		code += "{\n";
		code += "public:\n";
		code += "\t" + name + "();\n";
		code += "\t~" + name + "();\n";
		code += "private:\n";
		code += "};\n";
		gen_files(_file_dir, {name}, "hpp", false, code);
		code.clear();
		code += "\n" + name + "::" + name + "()\n";
		code += "{\n";
		code += "}\n\n";
		code += name + "::~" + name + "()\n";
		code += "{\n";
		code += "}\n";
		gen_files(_file_dir, {name}, "cpp", true, code);
	}
	return true;
}

bool Driver::gen_files(
	std::filesystem::path const &file_dir,
	std::vector<std::string> const &names,
	std::string const &type,
	bool match_header,
	std::string const &code
)
{
	if (file_dir.empty()) {
		return false;
	}

	bool is_header = type == "h" || type == "hpp";
	std::string dir_fence;
	if (is_header && _has_project) {
		dir_fence = header_fence_of_dir(file_dir, _project.name());
	}

	for (auto &name : names) {
		auto file_name = name + "." + type;
		auto h_path = validate_file_path(file_dir, file_name);
		if (h_path.empty()) {
			continue;
		}
		auto fence = to_valid_fence_string(dir_fence + name + '_' + type);
		auto buf = _file_front;
		if (is_header) {
			buf += "#ifndef " + fence + '\n';
			buf += "#define " + fence + '\n';
			buf += code;
			buf += "\n#endif\n";
		} else {
			if (match_header) {
				std::string include = "#include \"" + name + ".";
				if (type == "cpp") {
					include += "hpp\"\n";
				} else if (type == "c") {
					include += "h\"\n";
				} else {
					include = "";
				}
				buf += include;
			}
			buf += code;
		}
		if (!write_file(file_dir / file_name, buf)) {
			return false;
		}
	}
	return true;
}

std::string Driver::load_license(std::string const &license_name)
{
	return load_runtime_file(LICENSES_FOLDER / license_name);
}

std::string Driver::load_license_header(std::string const &license_name)
{
	return load_runtime_file(LICENSES_FOLDER / (license_name + ".header"));
}

void Driver::setup_files_options(bool preload_project)
{
	if (preload_project) {
		load_project();
	}
	_author = files_option_author(_cmdline, _project.author());
	_desc = files_option_desc(_cmdline, _project.desc());
	_file_dir = files_option_dir(_cmdline, _old_cwd);
	_license = files_option_license(_cmdline, _project.license());
	auto project_name = _project.name();
	_file_front = files_comment(
		_author,
		project_name.size() ? project_name + " - " + _desc : _desc,
		load_license_header(_license)
	);
}
