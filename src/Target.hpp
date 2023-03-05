#ifndef MAS_SRC_TARGET_HPP
#define MAS_SRC_TARGET_HPP
#include "mas/Value.hpp"
#include "common.hpp"
#include "Cdb.hpp"
#include <set>
#include <optional>
#include <memory>
#include <filesystem>
#include <functional>

class Project;

/// a cgen target
class Target
{
	struct InstallItem
	{
		std::string dir = ".";
		std::set<std::string> files;
	};

	Project *_project = nullptr;

	/// fields load from mas

	std::string _name;
	std::string _lang;
	std::string _std;
	TargetType _type = TargetType::None;

	std::set<std::string> _src;
	std::vector<std::string> _dep; // append indirect deps after `resolve_deps`
	std::vector<std::string> _pkg; // pkg-conf

	std::string _template;

	std::string _extra_cflags;
	std::string _extra_lflags;

	/// loaded from `_pkg`

	std::string _pkg_cflags;
	std::string _pkg_lflags;

	/// extra items to install
	std::vector<InstallItem> _installItems;

	bool _deps_resolved = false; // mark, dont resolve again

	bool load_pkgconfs(Mas::Map const &target_mas);

	bool load_installs(Mas::Map const &target_mas);

	bool apply_override(
		std::map<std::string, std::shared_ptr<Target>> const &all_templates,
		const char *field_name,
		std::function<std::string &(Target &target)> field
	);

	bool apply_append(std::map<std::string, std::shared_ptr<Target>> const &all_templates);

	void append(std::shared_ptr<Target> target);

public:
	Target(Project *project);
	Target(Target const &) = delete;
	Target &operator=(Target const &) = delete;

	/// load target data from mas
	bool load(std::string const &name, Mas::Map const &target_mas);

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

	/// apply templates
	bool apply_templates(std::map<std::string, std::shared_ptr<Target>> const &all_templates);

	/// install target files
	bool install(std::filesystem::path const &prefix, std::string const &profile) const;

}; // class Target

#endif
