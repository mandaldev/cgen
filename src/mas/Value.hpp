#ifndef MAS_SRC_VALUE_HPP
#define MAS_SRC_VALUE_HPP
#include <map>
#include <vector>
#include <string>
#include <variant>
#include <optional>

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

private:

	friend class Decoder;

	/// union of MAS
	std::variant<Map, Array, String> _var;

}; // class Value

bool get_string(Mas::Map const &m, std::string const &key, std::string &value);

} // namespace Mas

#endif
