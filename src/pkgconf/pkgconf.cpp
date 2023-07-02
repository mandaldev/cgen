#include "pkgconf.hpp"
#include "../common.hpp"
#include <Com/string.hpp>
#include <iostream>
#include <cassert>
#include <functional>

bool pkgconf_error_handler(const char *msg, const pkgconf_client_t *client, const void *data)
{
	auto self = (Pkgconf *)data;
	self->on_pkgconf_error(msg, client);
	return true;
}

Pkgconf::Pkgconf()
{
	_client = pkgconf_client_new(pkgconf_error_handler, this, pkgconf_cross_personality_default());
	assert(_client);
	pkgconf_client_dir_list_build(_client, pkgconf_cross_personality_default());
}

Pkgconf::~Pkgconf()
{
	if (_client) {
		pkgconf_client_free(_client);
		_client = nullptr;
	}
}

void Pkgconf::on_pkgconf_error(std::string const &msg, pkgconf_client_t const *client)
{
	std::cerr << "pkgconf error: " << msg << "\n";
}

template <typename Data>
static inline void cast_pkgconf_list(pkgconf_list_t const &list, std::function<void(Data const *data)> visitor)
{
	auto it = list.head;
	for (size_t i = 0; i < list.length && it; i++, it = it->next) {
		visitor((Data const *)it->data);
	}
}

static const std::set<std::string> SYSTEM_INCLUDE_DIRS = {
	"/usr/include",
	"/usr/local/include",
};
static const std::set<std::string> SYSTEM_LIB_DIRS = {
	"/usr/lib",
	"/usr/local/lib",
	"/usr/lib32",
	"/usr/local/lib32",
	"/usr/lib64",
	"/usr/local/lib64",
};

static std::vector<std::string> cast_string_fragment_list(pkgconf_list_t const &list)
{
	std::vector<std::string> strs;
	auto cat = [&strs](auto fragment)
	{
		if (!fragment || !fragment->data) {
			return;
		}
		std::string data = fragment->data;
		switch (fragment->type) {
		case 'I':
			if (SYSTEM_INCLUDE_DIRS.count(data)) {
				return;
			}
			break;
		case 'L':
			if (SYSTEM_LIB_DIRS.count(data)) {
				return;
			}
			break;
		}
		if (fragment->type > 0) {
			data = "-"s + fragment->type + data;
		}
		strs.push_back(data);
	};
	cast_pkgconf_list<pkgconf_fragment_t>(list, cat);
	return strs;
}

// TODO version info
static std::vector<std::string> cast_dependency_fragment_list(pkgconf_list_t const &list)
{
	std::vector<std::string> deps;
	auto cat = [&](auto dep)
	{
		if (!dep || !dep->package) {
			return;
		}
		deps.push_back(dep->package);
	};
	cast_pkgconf_list<pkgconf_dependency_t>(list, cat);
	return deps;
}

bool Pkgconf::find(std::string const &pkg_name)
{
	if (_pkgs.count(pkg_name)) {
		return true;
	}
	if (!_client) {
		return false;
	}
	auto pkg = pkgconf_pkg_find(_client, pkg_name.c_str());
	if (!pkg) {
		return false;
	}

	PkgInfo info;
	info.cflags = Com::join(" ", cast_string_fragment_list(pkg->cflags));
	info.libs = Com::join(" ", cast_string_fragment_list(pkg->libs));
	info.libs_private = Com::join(" ", cast_string_fragment_list(pkg->libs_private));
	info.required = cast_dependency_fragment_list(pkg->required);
	info.required_private = cast_dependency_fragment_list(pkg->requires_private);
	_pkgs.insert({pkg_name, info});
	pkgconf_pkg_unref(_client, pkg);

	for (auto const &dep : info.required) {
		if (!find(dep)) {
			return false;
		}
	}
	for (auto const &dep : info.required_private) {
		if (!find(dep)) {
			return false;
		}
	}
	return true;
}

std::string Pkgconf::flags_no_recursively(RecursiveStack &stack, std::string const &pkg_name)
{
	if (stack.pkgs.count(pkg_name)) {
		return ""s;
	}
	auto it = _pkgs.find(pkg_name);
	if (it == _pkgs.end()) {
		return ""s;
	}
	stack.pkgs.insert(pkg_name);

	auto &pkg = it->second;

	std::string ret;
	if (stack.cflags) {
		ret = Com::join(" ", ret, pkg.cflags);
	}
	if (stack.lflags) {
		ret = Com::join(" ", ret, pkg.libs);
	}
	if (stack.required) {
		for (auto &dep : pkg.required) {
			ret = Com::join(" ", ret, flags_no_recursively(stack, dep));
		}
	}
	if (stack.required_private) {
		for (auto &dep : pkg.required_private) {
			ret = Com::join(" ", ret, flags_no_recursively(stack, dep));
		}
	}
	return ret;
}

std::string Pkgconf::cflags(std::string const &pkg_name)
{
	RecursiveStack stack;
	stack.cflags = true;
	stack.required = true;
	stack.required_private = true;
	return flags_no_recursively(stack, pkg_name);
}

std::string Pkgconf::libs(std::string const &pkg_name)
{
	RecursiveStack stack;
	stack.lflags = true;
	stack.required = true;
	stack.required_private = false;
	return flags_no_recursively(stack, pkg_name);
}
