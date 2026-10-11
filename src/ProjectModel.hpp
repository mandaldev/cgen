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
#ifndef CGEN_SRC_PROJECTMODEL_HPP_
#define CGEN_SRC_PROJECTMODEL_HPP_
#include <Com/Mas/serialize.hpp>
#include <map>
#include <set>
#include <string>
#include <variant>
#include <vector>

struct ProjectModel
{
	/// 依赖项描述
	struct DepDesc
	{
		/// 名字
		std::string name;
		/// 是否链接静态库，默认动态库，如果依赖项是当前项目的目标，则忽略此设置。
		bool static_ = false;
		/// 是否需要公开
		bool private_ = false;
	};
	/// 只是字符串的话相当于只指定 DepDesc 的 name，其他保留默认值
	using Dep = std::variant<std::string, DepDesc>;
	static std::vector<DepDesc> flatten_dep(std::vector<Dep> const &deps);

	using Flags = std::variant<std::string, std::vector<std::string>>;
	static std::vector<std::string> flatten_flags(Flags const &flags);

	std::string name;
	std::string author;
	std::string email;
	std::string desc;
	std::string license;

	/// dirs to look for imported targets
	/// like "/usr/local"
	std::vector<std::filesystem::path> imports;

	/// [cl]flags 参数会传递给所有的目标，且会被导出，如果不希望导出参数，可以使用 private 参数版本
	struct Profile
	{
		/// 要继承的内建 profile，使用内建 profile 的名字也可以直接继承
		std::string inherit;

		Flags cflags, cflags_private;
		Flags lflags, lflags_private;

		/// "with", "only", or empty string
		std::string shared;

		bool build_shared() const { return shared == "only" || shared == "with"; }
		bool build_static() const { return shared != "only"; }
	};
	/// <name, profile>
	std::map<std::string, Profile> profile;

	struct TargetModel
	{
		std::string name;
		std::string language;
		std::string std;
		/// exe, lib, phony, template
		std::string type;
		std::string template_;
		std::set<std::filesystem::path> src;
		std::vector<Dep> dep;
		std::vector<Dep> pkgconf;
		/// private 参数只在编译动态库时有效，且不会被导出的
		Flags cflags, cflags_static, cflags_shared, cflags_private;
		/// private 参数只在编译动态库时有效，且不会被导出的
		Flags lflags, lflags_static, lflags_shared, lflags_private;

		/// - 对于 headers，安装路径为 `$PREFIX/include/<dir>/<header>`
		/// - 对于 files，安装路径为 `$PREFIX/<dir>/<file>` ，file 可以是目录
		struct Install
		{
			/// 必须为相对路径
			std::string dir;
			std::set<std::filesystem::path> files;
			std::set<std::filesystem::path> headers;
		};
		std::vector<Install> install;
		/// 是否必须手动安装
		bool explicit_install = false;
	};
	/// target model or mas file path
	using Target = std::variant<TargetModel, std::string>;
	/// <name, target>
	using TargetMap = std::map<std::string, Target>;
	/// <target>
	using TargetArray = std::vector<Target>;
	/// array or map
	using Targets = std::variant<TargetArray, TargetMap>;

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
Value serialize(ProjectModel::DepDesc const &v);

template <>
bool deserialize(Value const &mas, ProjectModel::DepDesc &v);

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
