#ifndef CGEN_SRC_PKHCONF_PKGCONF_HPP
#define CGEN_SRC_PKHCONF_PKGCONF_HPP
#include <libpkgconf/libpkgconf.h>
#include <map>
#include <set>
#include <string>
#include <vector>

/// Wrapper of libpkgconf.
/// Call `find` first to check if a pkg exists, since `cflags` and `libs`
/// return string only.
class Pkgconf
{
	pkgconf_client_t *_client = nullptr;

	struct PkgInfo
	{
		// flags for compile and linking.
		// private only dynamic linking needed.
		std::string cflags, libs, libs_private;
		// pkgs those this pkg depands.
		// `required` need both cflags and libs,
		// private only cflags needed (for dynamic linking).
		std::vector<std::string> required, required_private;
	};

	std::map<std::string, PkgInfo> _pkgs; // cached pkg infos

	friend bool pkgconf_error_handler(const char *msg, const pkgconf_client_t *client, const void *data);

	void on_pkgconf_error(std::string const &msg, pkgconf_client_t const *client);

	struct RecursiveStack
	{
		std::set<std::string> pkgs;
		bool cflags = false;
		bool lflags = false;
		bool required = false;
		bool required_private = false;
	};
	std::string flags_no_recursively(RecursiveStack &stack, std::string const &pkg_name);

public:
	Pkgconf();
	~Pkgconf();

	bool find(std::string const &pkg_name);

	std::string cflags(std::string const &pkg_name);

	std::string libs(std::string const &pkg_name);
}; // class Pkgconf

#endif
