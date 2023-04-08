#include "Driver.hpp"
#include "help.hpp"
#include "xo/exec.hpp"
#include <sstream>

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

static std::string load_file(std::filesystem::path const &p)
{
	std::error_code ec {};
	auto size = std::filesystem::file_size(p, ec);
	if (ec || size == 0) {
		return {};
	}
	auto buf = std::string(size, 0);
	auto ifs = std::ifstream(p);
	return ifs.read(buf.data(), buf.size()) ? buf : ""s;
}

// load project.mas as Mas::Value
static std::optional<Mas::Value> load_project_mas()
{
	auto buf = load_file(PROJECT_MAS);
	if (buf.size()) {
		return Mas::decode(buf);
	} else {
		std::cerr << "Error: read " << PROJECT_MAS << " failed\n";
		return {};
	}
}

static std::string files_option_author(XO::CommandLine const &cmdline, std::string def)
{
	def = cmdline.get_switch("author").value_or(def);
	if (def.empty()) {
		def = "<name of author>";
	}
	return def;
}

static std::string files_option_license(XO::CommandLine const &cmdline, std::string def)
{
	return cmdline.get_switch("license").value_or(def);
}

static std::string files_option_desc(XO::CommandLine const &cmdline, std::string def)
{
	def = cmdline.get_switch("desc").value_or(def);
	if (def.empty()) {
		def = "<one line to give the program's name and a brief idea of what it does.>";
	}
	return def;
}

static std::filesystem::path files_option_dir(XO::CommandLine const &cmdline, std::filesystem::path const &cwd)
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
	oss << "// Copyright (C) " << Date::Now::year() << "  " << author << '\n';
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
	return string_uppercase_all(oss.str());
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
	_cmd["help"] = [&]() { return help(); };
	_cmd["build"] = [&]() { return build(); };
	_cmd["install"] = [&]() { return install(); };
	_cmd["project"] = [&]() { return gen_project(); };
	_cmd["class"] = [&]()
	{
		setup_files_options();
		return gen_class(_cmdline.get_arguments());
	};
	_cmd["cpp"] = [&]()
	{
		setup_files_options();
		return gen_files(_cmdline.get_arguments(), "cpp", false, "");
	};
	_cmd["hpp"] = [&]()
	{
		setup_files_options();
		return gen_files(_cmdline.get_arguments(), "hpp", false, "");
	};
	_cmd["c"] = [&]()
	{
		setup_files_options();
		return gen_files(_cmdline.get_arguments(), "c", false, "");
	};
	_cmd["h"] = [&]()
	{
		setup_files_options();
		return gen_files(_cmdline.get_arguments(), "h", false, "");
	};
}

bool Driver::run()
{
	if (_argc == 1) {
		help();
		return true;
	}
	if (!_cmdline.is_valid()) {
		std::cerr << "Invalid option or argument, see Usage\n\n";
		help();
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

	auto mas = load_project_mas();
	if (!mas.has_value() || !mas.value().is_map()) {
		std::cerr << "Not a valid cgen project file\n";
		return false;
	}

	if (!_project.load(mas.value().as_map())) {
		std::cerr << "Failed to load cgen project file\n";
		return false;
	}
	_has_project = true;
	return true;
}

bool Driver::help()
{
	auto args = _cmdline.get_arguments();
	if (args.size()) {
		std::map<std::string, std::string> cmd_help = {
			{ "build", HELP_BUILD },
			{ "project", HELP_BUILD },
			{ "class", HELP_FILES },
			{ "cpp", HELP_FILES },
			{ "hpp", HELP_FILES },
			{ "c", HELP_FILES },
			{ "h", HELP_FILES },
		};
		auto it = cmd_help.find(args[0]);
		if (it != cmd_help.end()) {
			std::cerr << it->second;
		}
	}
	std::cerr << HELP_MSG;
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
	if (!_project.configure(opt_cdb)) {
		std::cerr << "Failed to configure project\n";
		return false;
	}
	auto targets_name = _cmdline.get_arguments();
	if (targets_name.empty()) {
		return _project.build_all_targets();
	}
	for (auto const &name : targets_name) {
		if (!_project.build_target(name)) {
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
	auto targets_name = _cmdline.get_arguments();
	if (targets_name.empty()) {
		return _project.install_all_targets(prefix);
	}
	for (auto const &name : targets_name) {
		if (!_project.install_target(prefix, name)) {
			std::cerr << "Aborting...\n";
			return false;
		}
	}
	return true;
}

bool Driver::gen_project()
{
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
		gen_files({name}, "hpp", false, code);
		code.clear();
		code += name + "::" + name + "()\n";
		code += "{\n";
		code += "}\n\n";
		code += name + "::~" + name + "()\n";
		code += "{\n";
		code += "}\n";
		gen_files({name}, "cpp", true, code);
	}
	return true;
}

bool Driver::gen_files(
	std::vector<std::string> const &names,
	std::string const &type,
	bool match_header,
	std::string const &code
)
{
	if (_file_dir.empty()) {
		return false;
	}

	bool is_header = type == "h" || type == "hpp";
	std::string dir_fence;
	if (is_header && _has_project) {
		dir_fence = header_fence_of_dir(_file_dir, _project.name());
	}

	for (auto &name : names) {
		auto file_name = name + "." + type;
		auto h_path = validate_file_path(_file_dir, file_name);
		if (h_path.empty()) {
			continue;
		}
		auto fence = dir_fence + string_uppercase_all(name + '_' + type);
		auto buf = _file_front;
		if (is_header) {
			buf += "#ifndef " + fence + '\n';
			buf += "#define " + fence + '\n';
			buf += code;
			buf += "\n#endif\n";
		} else if (match_header) {
			std::string include = "#include \"" + name + ".";
			if (type == "cpp") {
				include += "hpp\"\n\n";
			} else if (type == "c") {
				include += "h\"\n\n";
			} else {
				include = "";
			}
			buf += include;
			buf += code;
		}
		if (!write_file(_file_dir / file_name, buf)) {
			return false;
		}
	}
	return true;
}

void Driver::setup_data_dirs()
{
	if (_data_dirs.size()) {
		return;
	}
	std::filesystem::path home = getenv("HOME");
	if (!home.empty()) {
		_data_dirs.push_back(home / ".cgen");
		_data_dirs.push_back(home / ".local/share/cgen");
	}
	auto p = XO::exec_install_prefix();
	if (!p.empty()) {
		_data_dirs.push_back(p / "share/cgen");
	}
}

std::string Driver::load_license_header(std::string const &license_name)
{
	if (_data_dirs.empty()) {
		setup_data_dirs();
	}
	for (auto &dir : _data_dirs) {
		auto p = dir / LICENSES_FOLDER / (license_name + ".header");
		auto buf = load_file(p);
		if (buf.size()) {
			return buf;
		}
	}
	std::cerr << "Warning: license header not found: " << license_name << "\n";
	return {};
}

void Driver::setup_files_options()
{
	load_project();
	_author = files_option_author(_cmdline, _project.author());
	_desc = files_option_desc(_cmdline, _project.desc());
	_file_dir = files_option_dir(_cmdline, _old_cwd);
	auto project_name = _project.name();
	_file_front = files_comment(
		_author,
		project_name.size() ? project_name + " -- " + _desc : _desc,
		load_license_header(files_option_license(_cmdline, _project.license()))
	);
}
