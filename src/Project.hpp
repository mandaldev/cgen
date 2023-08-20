#ifndef MAS_SRC_PROJECT_HPP
#define MAS_SRC_PROJECT_HPP
#include "Target.hpp"
#include "pkgconf/pkgconf.hpp"
#include <set>

/// a cgen project
class Project
{
	std::string _project_cached;

	ProjectModel _pm;
	std::vector<std::filesystem::path> _import_dir;
	std::map<std::string, ProjectModel::Profile> _profile;

	std::map<std::string, std::shared_ptr<Target>> _targets, _templates, _imports;

	Pkgconf _pkgconf;

	bool gen(std::string const &profile, std::optional<Cdb> &opt_cdb) const;

	// generate target's dep first and then itself
	bool gen_with_dep(std::string const &profile, std::shared_ptr<Target> target, std::set<std::string> &generated, std::optional<Cdb> &opt_cdb) const;

	void setup_default_profiles();

	bool load_target(std::string const &target_name, ProjectModel::TargetModel &tm, std::filesystem::path const &basedir);

	// visit
	bool load_target(std::string const &target_name, ProjectModel::TargetModel &tm, ProjectModel::Target &variant);

	// visit
	bool load_target(std::string const &target_name, std::string const &include, ProjectModel::Target &variant);

	// visit
	bool load_targets(ProjectModel::TargetMap &targets);

	// visit
	bool load_targets(ProjectModel::TargetArray &targets);

public:
	Project() = default;
	Project(Project const &) = delete;
	Project &operator=(Project const &) = delete;

	std::string const &name() const;

	std::string const &author() const;

	std::string const &email() const;

	std::string const &license() const;

	std::string const &desc() const;

	std::vector<std::filesystem::path> const &import_dirs() const;

	std::map<std::string, std::shared_ptr<Target>> const &targets() const;

	std::optional<ProjectModel::Profile> get_profile(std::string const &name) const;

	Pkgconf *pkgconf();

	/// load project from a Mas::Map value
	bool load(ProjectModel const &project);

	/// generate build scripts
	bool configure(std::string const &profile, std::optional<Cdb> &opt_cdb) const;

	/// build all targets in this project
	bool build_all_targets(std::string const &profile);

	/// build a specific target and its deps
	bool build_target(std::string const &profile, std::string const &target_name);

	/// install all targets in this project
	bool install_all_targets(std::string const &profile, std::filesystem::path const &prefix);

	/// install a specific target
	bool install_target(std::string const &profile, std::filesystem::path const &prefix, std::string const &target_name);
}; // class Project

#endif
