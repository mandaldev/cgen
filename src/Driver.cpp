#include "Driver.hpp"
#include "common.hpp"

static bool chroot()
{
	auto cur = std::filesystem::current_path();
	while (!std::filesystem::exists(cur / PROJECT_MAS)) {
		if (!cur.has_relative_path())
			return false;
		cur = cur.parent_path();
	}
	std::filesystem::current_path(cur); // chroot
	std::cerr << "chroot to " << cur << std::endl;
	return true;
}

// load project.mas as Mas::Value
static std::optional<Mas::Value> load_project_mas()
{
	try {
		auto buf = std::string(std::filesystem::file_size(PROJECT_MAS), 0);
		std::ifstream(PROJECT_MAS).read(buf.data(), buf.size());
		return Mas::decode(buf);
	} catch (...) {
		std::cerr << "read " << PROJECT_MAS << " failed" << std::endl;
		return {};
	}
}

Driver::Driver(int argc, char const *const *argv)
: _argc(argc)
, _argv(argv)
, _cmdline(argc - 1, argv + 1)
{
	_cmd["help"] = [&]() { return help(); };
	_cmd["build"] = [&]() { return build(); };
}

int Driver::run()
{
	if (_argc == 1) {
		help();
		return 0;
	}
	if (!_cmdline.is_valid()) {
		std::cerr << "Invalid option or argument, see Usage\n\n";
		help();
		return 1;
	}
	auto cmd = _argv[1];
	auto it = _cmd.find(cmd);
	if (it != _cmd.end()) {
		return it->second();
	}
	std::cerr << "Unknown commond: " << cmd << std::endl;
	return 1;
}

int Driver::help()
{
	std::cerr
		<< "Usage:" << std::endl
		<< "	cgen <command> [<args>]" << std::endl
		<< std::endl
		<< "Commands:" << std::endl
		<< "	build [options] [<target>]	build target" << std::endl
		<< "	help [<command>]		print help info [for specific command]" << std::endl
		<< std::endl
		<< "'build' options:" << std::endl
		<< "	-cdb			generate compilation databse file: compile_commands.json" << std::endl
		<< std::endl
	;
	return 0;
}

int Driver::build()
{
	if (!chroot()) {
		std::cerr << "Not a cgen project path" << std::endl;
		return 1;
	}

	auto mas = load_project_mas();
	if (!mas.has_value() || !mas.value().is_map()) {
		std::cerr
			<< "Not a valid cgen project file"
			<< std::endl;
		return 1;
	}

	Project project;
	if (!project.load(mas.value().as_map())) {
		std::cerr
			<< "Failed to load cgen project file"
			<< std::endl;
		return 1;
	}

	std::optional<Cdb> opt_cdb;
	if (_cmdline.has_switch("cdb")) {
		opt_cdb = Cdb();
	}
	if (!project.configure(opt_cdb)) {
		std::cerr
			<< "Failed to configure project"
			<< std::endl;
		return 1;
	}
	if (opt_cdb) {
		gen_cdb(opt_cdb.value());
	}
	auto targets_name = _cmdline.get_arguments();
	if (targets_name.empty()) {
		return project.build_all_targets() ? 0 : 1;
	}
	for (auto const &name : targets_name) {
		if (!project.build_target(name)) {
			std::cerr
				<< "Aborting..."
				<< std::endl;
			return 1;
		}
	}
	return 0;
}

int Driver::gen_cdb(Cdb const &cdb) const
{
	std::ofstream fs;
	std::string fname = "./build/debug/compile_commands.json";
	fs.open(fname, std::ios_base::out | std::ios_base::trunc);
	if (fs.fail()) {
		std::cerr
			<< "Failed to open file " << fname
			<< std::endl;
		return 1;
	}
	auto quote = '"';
	bool first = true;
	fs << "[\n";
	for (auto &item : cdb) {
		auto args = split_string(item.flags, " ");
		for (auto &s : item.files) {
			std::string obj;
			if (first) {
				first = false;
			} else {
				obj += ",";
			}
			obj += "{\"arguments\": [";
			obj += quote + item.cmd + quote;
			for (auto &&arg : args) {
				obj += ",\"" + arg + quote;
			}
			obj += ",\"-o\",";
			obj += quote + s + ".o" + quote;
			obj += ",\"../../" + s + quote;
			obj += "],\"directory\":";
			obj += quote + std::filesystem::absolute("build/debug").string() + quote;
			obj += ",\"file\":\"../../" + s + "\"}\n";
			fs << obj;
		}
	}
	fs << "]";
	fs.close();
	std::cerr << fname << " written\n";
	return true;
}
