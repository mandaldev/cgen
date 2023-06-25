#ifndef MAS_SRC_PROJECT_HPP
#define MAS_SRC_PROJECT_HPP
#include "Target.hpp"
#include "pkgconf/pkgconf.hpp"

/// a cgen project
class Project
{
	std::string _name;
	std::string _author;
	std::string _email;
	std::string _license;
	std::string _desc;

	std::map<std::string, std::shared_ptr<Target>> _targets, _templates;

	Pkgconf _pkgconf;

	std::map<std::string, Profile> _profiles;

	bool gen(std::string const &profile, std::optional<Cdb> &opt_cdb) const;

	// generate target's dep first and then itself
	bool gen_with_dep(std::string const &profile, std::shared_ptr<Target> target, std::set<std::string> &generated, std::optional<Cdb> &opt_cdb) const;

	bool load_profiles(Mas::Map const &mas);

	void setup_default_profiles();

public:
	Project() = default;
	Project(Project const &) = delete;
	Project &operator=(Project const &) = delete;

	std::string const &name() const;

	std::string const &author() const;

	std::string const &email() const;

	std::string const &license() const;

	std::string const &desc() const;

	std::map<std::string, std::shared_ptr<Target>> const &targets() const;

	std::optional<Profile> get_profile(std::string const &name) const;

	Pkgconf *pkgconf();

	/// load project from a Mas::Map value
	bool load(Mas::Map const &project_mas);

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
