#ifndef MAS_SRC_PROJECT_HPP
#define MAS_SRC_PROJECT_HPP
#include "Target.hpp"
#include "pkgconf/pkgconf.hpp"
#include <set>

/// a cgen project
class Project
: public Com::Unique
{
	std::string _project_cached;

	ProjectModel _pm;
	std::vector<std::filesystem::path> _data_dirs;
	std::map<std::string, Profile> _profiles;

	std::map<std::string, std::shared_ptr<Target>> _targets, _templates, _phonys, _imports;

	Pkgconf _pkgconf;

	bool gen(std::string const &profile, std::optional<Cdb> &opt_cdb);

	// generate target's dep first and then itself
	bool gen_with_dep(
		std::string const &profile,
		std::shared_ptr<Target> target,
		std::set<std::string> &generated_libs,
		std::set<std::string> &generated_exes,
		std::optional<Cdb> &opt_cdb
	);

	void setup_profiles();

	bool load_target(std::string const &target_name, ProjectModel::TargetModel const &tm, std::filesystem::path const &basedir);

	// visit
	bool load_target(std::string const &target_name, ProjectModel::TargetModel const &tm);

	// visit
	bool load_target(std::string const &target_name, std::string const &include);

	// visit
	bool load_targets(ProjectModel::TargetMap const &targets);

	// visit
	bool load_targets(ProjectModel::TargetArray const &targets);

public:
	std::string const &name() const;

	std::string const &author() const;

	std::string const &email() const;

	std::string const &license() const;

	std::string const &desc() const;

	std::vector<std::filesystem::path> const &import_dirs() const;

	std::map<std::string, std::shared_ptr<Target>> const &targets() const;

	std::optional<Profile> get_profile(std::string const &name) const;

	Pkgconf *pkgconf();

	/// load project from a Mas::Map value
	bool load(ProjectModel const &project);

	/// generate build scripts
	bool configure(std::string const &profile, std::optional<Cdb> &opt_cdb);

	/// build all targets in this project
	bool build_all_targets(std::string const &profile);

	/// build a specific target and its deps
	bool build_target(std::string const &profile, std::string const &target_name);

	bool build_targets(std::string const &profile, std::vector<std::string> const &targets);

	/// install all targets in this project
	bool install_all_targets(std::string const &profile, std::filesystem::path const &prefix);

	/// install a specific target
	bool install_target(std::string const &profile, std::filesystem::path const &prefix, std::string const &target_name);
}; // class Project

#endif
