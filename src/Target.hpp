#ifndef MAS_SRC_TARGET_HPP
#define MAS_SRC_TARGET_HPP
#include "mas/Value.hpp"
#include "common.hpp"
#include <set>
#include <memory>

/// a cgen target
class Target
{
	enum class Lang {
		C,
		CPlusPlus,
	}; // enum class Lang

	std::string _name;
	Lang _lang;
	std::string _std;
	TargetType _type;

	std::set<std::string> _src;
	std::vector<std::string> _dep;

	bool _indirected_loaded = false;

public:

	Target() = default;
	Target(Target const &) = delete;
	Target &operator=(Target const &) = delete;

	/// load target data from mas
	bool load(Mas::Map const &target_mas);

	std::string name() const;

	TargetType type() const;

	std::vector<std::string> dep() const;

	bool load_indirected(std::map<std::string, std::shared_ptr<Target>> const &all_targets);

	/// generate build scripts
	bool gen(std::map<std::string, std::shared_ptr<Target>> const &all_targets) const;

}; // class Target

#endif
