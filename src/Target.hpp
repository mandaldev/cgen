#ifndef MAS_SRC_TARGET_HPP
#define MAS_SRC_TARGET_HPP
#include "Cdb.hpp"
#include "ProjectModel.hpp"
#include "common.hpp"
#include "pkgconf/pkgconf.hpp"
#include <Com/Mas/Value.hpp>
#include <Com/raii.hpp>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

/// a cgen target
class Target
: public Com::Unique
{
	ProjectModel::TargetModel _tm;

	TargetType _type = TargetType::None;

	std::filesystem::path _basedir;

	/// loaded from `_pkg`

	std::string _pkg_cflags;
	std::string _pkg_lflags;

	bool _deps_resolved = false; // mark, dont resolve again
	/// resolved deps
	std::vector<std::string> _dep;

	bool _phony_applyed = false;

	void load_installs();

	bool apply_override(
		std::map<std::string, std::shared_ptr<Target>> const &all_templates,
		std::function<std::string &(ProjectModel::TargetModel &)> field
	);

	bool apply_append(std::map<std::string, std::shared_ptr<Target>> const &all_templates);

	void append(std::shared_ptr<Target> target);

	std::shared_ptr<Target> import_target(
		std::vector<std::filesystem::path> import_dirs,
		std::map<std::string, std::shared_ptr<Target>> &imports,
		std::string const &name
	);

	/// export this target only: not include its indirect dep and pkgconf
	std::filesystem::path exports(std::filesystem::path const &prefix) const;

public:
	/// load target data from model
	bool load(std::string const &name, ProjectModel::TargetModel const &tm, std::filesystem::path const &basedir = "");

	std::string name() const;

	TargetType type() const;

	ProjectModel::TargetModel const &model() const;

	std::vector<std::string> dep() const;

	/// from pkgconf and extras
	std::string external_cflags() const;

	/// from pkgconf and extras
	std::string external_lflags() const;

	bool load_pkgconfs(Pkgconf &pkgconf);

	/// resolve deps
	bool resolve_deps(
		std::map<std::string, std::shared_ptr<Target>> const &all_targets,
		std::vector<std::filesystem::path> import_dirs,
		std::map<std::string, std::shared_ptr<Target>> &imports
	);

	/// generate build scripts
	bool gen(std::string const &profile, std::map<std::string, std::shared_ptr<Target>> const &targets, std::map<std::string, std::shared_ptr<Target>> const &imports, std::optional<Cdb> &opt_cdb) const;

	/// apply templates
	bool apply_templates(std::map<std::string, std::shared_ptr<Target>> const &all_templates);

	bool apply_phonys(std::map<std::string, std::shared_ptr<Target>> const &all_phonys);

	/// install target files
	bool install(std::filesystem::path const &prefix, std::string const &profile) const;

}; // class Target

#endif
