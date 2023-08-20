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
	return serialize_kvs(COM_KV(v, cflags), COM_KV(v, lflags));
}

template <>
bool deserialize(Value const &mas, ProjectModel::Profile &v)
{
	return deserialize_kvs(mas, COM_KV(v, cflags), COM_KV(v, lflags));
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
		COM_KV(v, extra_cflags),
		COM_KV(v, extra_lflags),
		COM_KV(v, install)
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
		COM_KV(v, extra_cflags),
		COM_KV(v, extra_lflags),
		COM_KV(v, install)
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
		COM_KV(v, import_dir),
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
		COM_KV(v, import_dir),
		COM_KV(v, profile),
		COM_KV(v, target)
	);
}

}
