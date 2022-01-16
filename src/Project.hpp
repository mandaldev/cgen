#ifndef MAS_SRC_PROJECT_HPP
#define MAS_SRC_PROJECT_HPP
#include "Target.hpp"
#include <memory>

class Project
{
public:

	Project() = default;
	Project(Project const &) = delete;
	Project &operator=(Project const &) = delete;

	bool load(Mas::Map const &project_mas);

	bool configure() const;

	bool build_all_targets();

	bool build_target(std::string const &target_name);

protected:

	bool gen() const;

	bool gen_with_dep(std::shared_ptr<Target> target, std::set<std::string> &generated) const;

private:

	std::string _name;
	std::string _author;
	std::string _email;
	std::string _license;

	std::map<std::string, std::shared_ptr<Target>> _targets;

}; // class Project

#endif
