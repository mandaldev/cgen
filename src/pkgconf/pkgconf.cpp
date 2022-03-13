#include "pkgconf.hpp"
#include "../common.hpp"
#include <iostream>
#include <cassert>
#include <functional>

bool pkgconf_error_handler(const char *msg, const pkgconf_client_t *client, const void *data)
{
	auto self = (Pkgconf*)data;
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
	std::cerr << "pkgconf error: " << msg << std::endl;
}

template <typename Data>
static inline void cast_pkgconf_list(pkgconf_list_t const &list, std::function<void(Data const *data)> visitor)
{
	auto it = list.head;
	for (size_t i = 0; i < list.length && it; i++, it = it->next) {
		visitor((Data const *)it->data);
	}
}

static std::vector<std::string> cast_string_fragment_list(pkgconf_list_t const &list)
{
	std::vector<std::string> strs;
	cast_pkgconf_list<pkgconf_fragment_t>(list, [&](auto fragment)
	{
		if (!fragment || !fragment->data) {
			return;
		}
		strs.push_back("-"s + (char)fragment->type + fragment->data);
	});
	return strs;
}

// TODO version info
static std::vector<std::string> cast_dependency_fragment_list(pkgconf_list_t const &list)
{
	std::vector<std::string> deps;
	cast_pkgconf_list<pkgconf_dependency_t>(list, [&](auto dep)
	{
		if (!dep || !dep->package) {
			return;
		}
		deps.push_back(dep->package);
	});
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
	info.cflags = join_strings(cast_string_fragment_list(pkg->cflags), ' ');
	info.libs = join_strings(cast_string_fragment_list(pkg->libs), ' ');
	info.libs_private = join_strings(cast_string_fragment_list(pkg->libs_private), ' ');
	info.requires = cast_dependency_fragment_list(pkg->required);
	info.requires_private = cast_dependency_fragment_list(pkg->requires_private);
	_pkgs.insert({ pkg_name, info });
	pkgconf_pkg_unref(_client, pkg);

	for (auto const &dep : info.requires) {
		if (!find(dep)) {
			return false;
		}
	}
	for (auto const &dep : info.requires_private) {
		if (!find(dep)) {
			return false;
		}
	}
	return true;
}

std::string Pkgconf::flags_no_recursively(
	std::set<std::string> &stack,
	std::string const &pkg_name,
	std::string PkgInfo::* flags,
	std::vector<std::string> PkgInfo::* requires,
	std::vector<std::string> PkgInfo::* requires_private
)
{
	if (stack.count(pkg_name)) {
		return ""s;
	}
	auto it = _pkgs.find(pkg_name);
	if (it == _pkgs.end()) {
		return ""s;
	}
	stack.insert(pkg_name);

	auto &pkg = it->second;
	auto ret = pkg.*flags;
	if (requires) {
		for (auto &dep : pkg.*requires) {
			ret += ' ' + flags_no_recursively(stack, dep, flags, requires, requires_private);
		}
	}
	if (requires_private) {
		for (auto &dep : pkg.*requires_private) {
			ret += ' ' + flags_no_recursively(stack, dep, flags, requires, requires_private);
		}
	}
	return ret;
}

std::string Pkgconf::cflags(std::string const &pkg_name)
{
	std::set<std::string> stack;
	return flags_no_recursively(
		stack,
		pkg_name,
		&PkgInfo::cflags,
		&PkgInfo::requires,
		&PkgInfo::requires_private
	);
}

std::string Pkgconf::libs(std::string const &pkg_name)
{
	std::set<std::string> stack;
	return flags_no_recursively(
		stack,
		pkg_name,
		&PkgInfo::libs,
		&PkgInfo::requires,
		nullptr
	);
}