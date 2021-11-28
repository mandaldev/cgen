#ifndef MAS_SRC_TARGET_HPP
#define MAS_SRC_TARGET_HPP
#include "Value.hpp"
#include <set>

class Target
{
	enum class Lang {
		C,
		CPlusPlus,
	}; // enum class Lang
	enum class Type {
		Executable,
		StaticLibrary,
		DynamicLibrary,
	}; // enum class Type
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

	bool gen_exe() const;

	bool gen_slib() const;

	bool gen_dlib() const;

private:

	std::string _name;
	Lang _lang;
	std::string _std;
	Type _type;

	std::set<std::string> _src;

}; // class Target

static inline void load_string(Mas::Map const &m, std::string const &key, std::string &value)
{
	auto it = m.find(key);
	if (it != m.end()) {
		value = it->second.string_or({});
	}
}

#endif
