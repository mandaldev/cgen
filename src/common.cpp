// cgen - A C and C++ stuff generator
// Copyright (C) 2023  Mandal Dev
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#include "common.hpp"
#include <Com/file.hpp>
#include <Com/Xo/exec.hpp>
#include <mutex>
#include <iostream>

static std::once_flag g_data_dirs_of;
static std::vector<std::filesystem::path> g_data_dirs;

std::vector<std::filesystem::path> const &cgen_data_dirs()
{
	std::call_once(g_data_dirs_of, []()
	{
		std::filesystem::path home = getenv("HOME");
		if (!home.empty()) {
			g_data_dirs.push_back(home / ".cgen");
			g_data_dirs.push_back(home / ".local/share/cgen");
		}
		auto p = Com::exec_install_prefix();
		if (!p.empty()) {
			g_data_dirs.push_back(p / "share/cgen");
		}
		g_data_dirs.push_back("/usr/local/share/cgen");
		g_data_dirs.push_back("/usr/share/cgen");
	});
	return g_data_dirs;
}

std::string load_runtime_file(std::vector<std::filesystem::path> const &data_dirs, std::filesystem::path const & path)
{
	for (auto &dir : data_dirs) {
		auto p = dir / path;
		std::error_code ec{};
		if (!std::filesystem::exists(p, ec) || ec) {
			continue;
		}
		auto buf = Com::load_file(p);
		if (buf) {
			return buf.value();
		}
	}
	std::cerr << "Warning: file not found at any runtime folders: " << path << "\n";
	return {};
}

std::string load_runtime_file(std::filesystem::path const & path)
{
	auto &data_dirs = cgen_data_dirs();
	return load_runtime_file(data_dirs, path);
}
