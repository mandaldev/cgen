#ifndef MAS_SRC_VALUE_HPP
#define MAS_SRC_VALUE_HPP
#include <map>
#include <vector>
#include <string>
#include <variant>
#include <optional>
#include <sstream>

/// A simple Mas text.
///
/// ```
/// {
///   "string as key" [ "this" "is" "ok" ]
///   key1 [ "special" "key" "can" "avoid" "quoting" ]
///   map_value {
///     "1" "a nested map"
///   }
/// }
/// ```
///
/// As that: Mas stands for Map(M), Array(A), and String(S).
/// So simple that no ',' marks needed.

namespace Mas {

class Value;

/// Mas string type, sam as std::string
using String = std::string;
/// Mas array type, vector of Value
using Array = std::vector<Value>;
/// Mas map type, map of String-Value pair
using Map = std::map<String, Value>;

template <typename T>
static inline std::string to_string(T const &v)
{
	std::ostringstream oss;
	oss << v;
	return oss.str();
}

template <typename T>
static inline bool from_string(std::string const &str, T &v)
{
	std::istringstream iss{str};
	iss >> v;
	return iss.good();
}

template <typename T>
static inline T from_string(std::string const &str)
{
	T v{};
	from_string(str, v);
	return v;
}

/// encode option struct.
/// default is best for performence
struct EncodeOption
{
	/// use pretty output (add new lines and indentations)
	bool pretty = false;
	/// force key string quoting
	bool force_quoting = true;
}; // struct EncodeOption

/// Encode a Mas value to text
std::string encode(Value const &value, EncodeOption const &option = {});

/// Decode a Mas value from text
std::optional<Value> decode(std::string const &mas_text);

/// Wrapper class of MAS union
class Value
{
	friend class Decoder;

	/// union of MAS
	std::variant<Map, Array, String> _var;

public:
	Value() = default;
	Value(Map const &src): _var(src) {}
	Value(Array const &src): _var(src) {}
	Value(String const &src): _var(src) {}
	Value(char const *src): _var(String(src)) {}

	/// type checker

	bool is_map() const;
	bool is_array() const;
	bool is_string() const;

	/// value getter, assign with `src` if type does't match

	Map &as_map(Map const &src = {});
	Array &as_array(Array const &src = {});
	String &as_string(String const &src = {});

	/// const value getter, return `what` if type does't match

	Map const &map_or(Map const &what) const;
	Array const &array_or(Array const &what) const;
	String const &string_or(String const &what) const;

	template <typename Visitor>
	auto visit_by(Visitor &visitor) const
	{
		return std::visit(visitor, _var);
	}

	// format `v` to a string value
	template <typename T>
	String &as_any(T const &v)
	{
		return as_string(to_string(v));
	}

	// parse any type data from a string value
	template <typename T>
	bool to_any(T &v) const
	{
		return from_string(string_or(""), v);
	}

	// parse any type data from a string value
	template <typename T>
	T to_any() const
	{
		T v{};
		to_any(v);
		return v;
	}
}; // class Value

template <>
inline String &Value::as_any<String>(String const &v)
{
	return as_string(v);
}

template <>
inline bool Value::to_any<String>(String &v) const
{
	v = string_or(v);
	return is_string();
}

bool get_string(Mas::Map const &m, std::string const &key, std::string &value);

} // namespace Mas

#endif
