#include "exec.hpp"
#include <iterator>
#include <fstream>
#include <filesystem>
#include <cassert>
using namespace std::literals;

static inline std::string get_tmpnam(std::string const &tmpname)
{
	int n = 0;
	auto name = tmpname + "_a";
	while (n++ < 20) {
		std::error_code ec{};
		if (!std::filesystem::exists(name, ec) && !ec) { // no one is using it
			return name;
		}
		name.back() += 1;
	}
	return {};
}

namespace XO {

int exec(std::string const &cmd, std::string const &tmpname, std::string &stdout_content, std::string &stderr_content)
{
	std::string cmdline = cmd;
	std::string out_name = get_tmpnam(tmpname + "_stdout");
	std::string err_name = get_tmpnam(tmpname + "_stderr");
	if (out_name.empty() || err_name.empty()) {
		stderr_content = "no more temp files";
		return 1;
	}
	cmdline += " &> " + err_name + " > " + out_name;
	auto ret = std::system(cmdline.c_str());
	if (!ret) {
		try {
			std::ifstream out{ out_name };
			std::ifstream err{ err_name };
			stdout_content = { std::istream_iterator<char>(out), std::istream_iterator<char>() };
			stderr_content = { std::istream_iterator<char>(err), std::istream_iterator<char>() };
		} catch (std::exception const &e) {
			stderr_content = "Failed to read output: ";
			stderr_content += e.what();
			ret = 1;
		}
	}
	std::system(("rm -f " + err_name + " " + out_name).c_str());
	return ret;
}

} // namespace XO
