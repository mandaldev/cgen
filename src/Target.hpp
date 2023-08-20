#ifndef MAS_SRC_TARGET_HPP
#define MAS_SRC_TARGET_HPP
#include <Com/Mas/Value.hpp>
#include "common.hpp"
#include "Cdb.hpp"
#include <set>
#include <optional>
#include <memory>
#include <filesystem>
#include <functional>
#include "ProjectModel.hpp"

class Project;

/// a cgen target
class Target
{
	Project *_project = nullptr;

	ProjectModel::TargetModel _tm;

	TargetType _type = TargetType::None;

	std::filesystem::path _basedir;

	/// loaded from `_pkg`

	std::string _pkg_cflags;
	std::string _pkg_lflags;

	bool _deps_resolved = false; // mark, dont resolve again
	/// resolved deps
	std::vector<std::string> _dep;

	bool load_pkgconfs();

	void load_installs();

	bool apply_override(
		std::map<std::string, std::shared_ptr<Target>> const &all_templates,
		const char *field_name,
		std::function<std::string &(ProjectModel::TargetModel &)> field
	);

	bool apply_append(std::map<std::string, std::shared_ptr<Target>> const &all_templates);

	void append(std::shared_ptr<Target> target);

public:
	Target(Project *project);
	Target(Target const &) = delete;
	Target &operator=(Target const &) = delete;

	/// load target data from mas
	bool load(std::string const &name, ProjectModel::TargetModel &tm, std::filesystem::path const &basedir = "");

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
	bool gen(std::string const &profile, std::optional<Cdb> &opt_cdb) const;

	/// apply templates
	bool apply_templates(std::map<std::string, std::shared_ptr<Target>> const &all_templates);

	/// install target files
	bool install(std::filesystem::path const &prefix, std::string const &profile) const;

	Com::Mas::Value exports() const;

}; // class Target

#endif
