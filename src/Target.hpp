#ifndef MAS_SRC_TARGET_HPP
#define MAS_SRC_TARGET_HPP
#include "Value.hpp"
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

	std::string const &name() const
	{
		return _name;
	}

	/// generate build scripts
	bool gen() const;

private:

private:

	std::string _name;
	Lang _lang;
	std::string _std;
	TargetType _type;

	std::set<std::string> _src;

}; // class Target

#endif
