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
	std::map<std::string, std::shared_ptr<Target>> _targets;

	Pkgconf _pkgconf;

	bool gen() const;

	// generate target's dep first and then itself
	bool gen_with_dep(std::shared_ptr<Target> target, std::set<std::string> &generated) const;

public:

	Project() = default;
	Project(Project const &) = delete;
	Project &operator=(Project const &) = delete;

	std::map<std::string, std::shared_ptr<Target>> const &targets() const;

	Pkgconf *pkgconf();

	/// load project from a Mas::Map value
	bool load(Mas::Map const &project_mas);

	/// generate build scripts
	bool configure() const;

	/// build all targets in this project
	bool build_all_targets();

	/// build a specific target and its deps
	bool build_target(std::string const &target_name);

}; // class Project

#endif
