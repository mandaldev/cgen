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
#include "ProjectModel.hpp"
#include <string>
#include <variant>


std::vector<ProjectModel::DepDesc> ProjectModel::flatten_dep(std::vector<Dep> const &deps)
{
	std::vector<ProjectModel::DepDesc> ret;
	for (auto &d : deps) {
		if (auto p = std::get_if<std::string>(&d)) {
			DepDesc &desc = ret.emplace_back();
			desc.name = *p;
		} else if (auto p = std::get_if<DepDesc>(&d)) {
			ret.push_back(*p);
		}
	}
	return ret;
}

std::vector<std::string> ProjectModel::flatten_flags(Flags const &flags)
{
	std::vector<std::string> ret;
	if (auto p = std::get_if<std::string>(&flags)) {
		ret.push_back(*p);
	} else if (auto p = std::get_if<std::vector<std::string>>(&flags)) {
		ret = *p;
	}
	return ret;
}

namespace Com::Mas {

template <>
Value serialize(ProjectModel::TargetModel::Install const &v)
{
	return serialize_kvs(COM_KV(v, dir), COM_KV(v, files), COM_KV(v, headers));
}

template <>
bool deserialize(Value const &mas, ProjectModel::TargetModel::Install &v)
{
	return deserialize_kvs(mas, COM_KV(v, dir), COM_KV(v, files), COM_KV(v, headers));
}

template <>
Value serialize(ProjectModel::Profile const &v)
{
	return serialize_kvs(
		COM_KV(v, inherit),
		COM_KV(v, cflags),
		COM_KV(v, lflags),
		COM_KV(v, cflags_private),
		COM_KV(v, lflags_private),
		COM_KV(v, shared)
	);
}

template <>
bool deserialize(Value const &mas, ProjectModel::Profile &v)
{
	return deserialize_kvs(
		mas,
		COM_KV(v, inherit),
		COM_KV(v, cflags),
		COM_KV(v, lflags),
		COM_KV(v, cflags_private),
		COM_KV(v, lflags_private),
		COM_KV(v, shared)
	);
}

template <>
Value serialize(ProjectModel::DepDesc const &v)
{
	return serialize_kvs(
		COM_KV(v, name),
		"static", v.static_,
		"private", v.private_
	);
}

template <>
bool deserialize(Value const &mas, ProjectModel::DepDesc &v)
{
	return deserialize_kvs(
		mas,
		COM_KV(v, name),
		"static", v.static_,
		"private", v.private_
	);
}

template <>
Value serialize(ProjectModel::TargetModel const &v)
{
	return serialize_kvs(
		COM_KV(v, name),
		COM_KV(v, language),
		COM_KV(v, std),
		COM_KV(v, type),
		"template", v.template_,
		COM_KV(v, src),
		COM_KV(v, dep),
		COM_KV(v, pkgconf),
		COM_KV(v, cflags),
		COM_KV(v, lflags),
		COM_KV(v, cflags_static),
		COM_KV(v, lflags_static),
		COM_KV(v, cflags_shared),
		COM_KV(v, lflags_shared),
		COM_KV(v, cflags_private),
		COM_KV(v, lflags_private),
		COM_KV(v, install),
		COM_KV(v, explicit_install)
	);
}

template <>
bool deserialize(Value const &mas, ProjectModel::TargetModel &v)
{
	return deserialize_kvs(
		mas,
		COM_KV(v, name),
		COM_KV(v, language),
		COM_KV(v, std),
		COM_KV(v, type),
		"template", v.template_,
		COM_KV(v, src),
		COM_KV(v, dep),
		COM_KV(v, pkgconf),
		COM_KV(v, cflags),
		COM_KV(v, lflags),
		COM_KV(v, cflags_static),
		COM_KV(v, lflags_static),
		COM_KV(v, cflags_shared),
		COM_KV(v, lflags_shared),
		COM_KV(v, cflags_private),
		COM_KV(v, lflags_private),
		COM_KV(v, install),
		COM_KV(v, explicit_install)
	);
}

template <>
Value serialize(ProjectModel const &v)
{
	return serialize_kvs(
		COM_KV(v, name),
		COM_KV(v, author),
		COM_KV(v, email),
		COM_KV(v, desc),
		COM_KV(v, license),
		COM_KV(v, imports),
		COM_KV(v, profile),
		COM_KV(v, target)
	);
}

template <>
bool deserialize(Value const &mas, ProjectModel &v)
{
	return deserialize_kvs(
		mas,
		COM_KV(v, name),
		COM_KV(v, author),
		COM_KV(v, email),
		COM_KV(v, desc),
		COM_KV(v, license),
		COM_KV(v, imports),
		COM_KV(v, profile),
		COM_KV(v, target)
	);
}

}
