#ifndef MAS_SRC_TARGET_HPP
#define MAS_SRC_TARGET_HPP
#include "mas/Value.hpp"
#include "common.hpp"
#include "cdb.hpp"
#include <set>
#include <optional>
#include <memory>

class Project;

/// a cgen target
class Target
{
	enum class Lang {
		C,
		CPlusPlus,
	}; // enum class Lang

	Project *_project = nullptr;

	std::string _name;
	Lang _lang;
	std::string _std;
	TargetType _type;

	std::set<std::string> _src;
	std::vector<std::string> _dep;
	std::vector<std::string> _pkg; // pkg-conf

	std::string _extra_cflags;
	std::string _extra_lflags;

	std::string _pkg_cflags;
	std::string _pkg_lflags;

	bool _deps_resolved = false;

	bool load_pkgconfs(Mas::Map const &target_mas);

public:

	Target(Project *project);
	Target(Target const &) = delete;
	Target &operator=(Target const &) = delete;

	/// load target data from mas
	bool load(Mas::Map const &target_mas);

	std::string name() const;

	TargetType type() const;

	std::vector<std::string> dep() const;

	/// from pkgconf and extras
	std::string external_cflags() const;

	/// from pkgconf and extras
	std::string external_lflags() const;

	/// resolve deps
	bool resolve_deps(std::map<std::string, std::shared_ptr<Target>> const &all_targets);

	/// generate build scripts
	bool gen(std::optional<Cdb> &opt_cdb) const;

}; // class Target

#endif
