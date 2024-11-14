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
#ifndef _CGEN_SRC_PROJECTMODEL_HPP_
#define _CGEN_SRC_PROJECTMODEL_HPP_
#include <string>
#include <map>
#include <set>
#include <vector>
#include <Com/Mas/serialize.hpp>

struct ProjectModel
{
	std::string name;
	std::string author;
	std::string email;
	std::string desc;
	std::string license;

	/// dirs to look for imported targets
	/// like "/usr/local"
	std::vector<std::filesystem::path> imports;

	struct Profile
	{
		std::string cflags;
		std::string lflags;
	};
	/// <name, profile>
	std::map<std::string, Profile> profile;

	struct TargetModel
	{
		std::string name;
		std::string language;
		std::string std;
		std::string type;
		std::string template_;
		std::set<std::filesystem::path> src;
		std::vector<std::string> dep;
		std::vector<std::string> pkgconf;
		std::string extra_cflags;
		std::string extra_lflags;

		struct Install
		{
			std::string dir;
			std::set<std::filesystem::path> files;
			std::set<std::filesystem::path> headers;
		};
		std::vector<Install> install;
	};
	using Target = std::variant<TargetModel, std::string>;
	/// <name, target/include>
	using TargetMap = std::map<std::string, Target>;
	/// <target/include>
	using TargetArray = std::vector<Target>;

	using Targets = std::variant<TargetArray, TargetMap>;
	/// array or map
	Targets target;
};

namespace Com::Mas {

template <>
Value serialize(ProjectModel::TargetModel::Install const &v);

template <>
bool deserialize(Value const &mas, ProjectModel::TargetModel::Install &v);

template <>
Value serialize(ProjectModel::Profile const &v);

template <>
bool deserialize(Value const &mas, ProjectModel::Profile &v);

template <>
Value serialize(ProjectModel::TargetModel const &v);

template <>
bool deserialize(Value const &mas, ProjectModel::TargetModel &v);

template <>
Value serialize(ProjectModel const &v);

template <>
bool deserialize(Value const &mas, ProjectModel &v);

}
#endif
