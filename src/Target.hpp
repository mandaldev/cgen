#ifndef MAS_SRC_TARGET_HPP
#define MAS_SRC_TARGET_HPP
#include "mas/Value.hpp"
#include "common.hpp"
#include <set>

class Target
{
	enum class Lang {
		C,
		CPlusPlus,
	}; // enum class Lang
public:

	Target() = default;
	Target(Target const &) = delete;
	Target &operator=(Target const &) = delete;

	/// load target data from mas
	bool load(Mas::Map const &target_mas);

	std::string name() const;

	TargetType type() const;

	std::set<std::string> dep() const;

	/// generate build scripts
	bool gen(std::map<std::string, TargetType> const &all_dep_type) const;

private:

private:

	std::string _name;
	Lang _lang;
	std::string _std;
	TargetType _type;

	std::set<std::string> _src;
	std::set<std::string> _dep;

}; // class Target

#endif
