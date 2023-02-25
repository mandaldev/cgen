#include "Value.hpp"
#include <algorithm>
#include <cassert>

namespace Mas {

static bool is_key_need_quote(String const &key)
{
	auto is_key_char = [](char c)
	{
		switch (c) {
		case '\\': case '"':
			return true;
		default:
			return false;
		}
	};
	return std::any_of(key.cbegin(), key.cend(), is_key_char);
}

bool Value::is_map() const
{
	return std::holds_alternative<Map>(_var);
}

bool Value::is_array() const
{
	return std::holds_alternative<Array>(_var);
}

bool Value::is_string() const
{
	return std::holds_alternative<String>(_var);
}

Map &Value::as_map(Map const &src)
{
	if (!std::holds_alternative<Map>(_var)) {
		_var = src;
	}
	return std::get<Map>(_var);
}

Array &Value::as_array(Array const &src)
{
	if (!std::holds_alternative<Array>(_var)) {
		_var = src;
	}
	return std::get<Array>(_var);
}

String &Value::as_string(String const &src)
{
	if (!std::holds_alternative<String>(_var)) {
		_var = src;
	}
	return std::get<String>(_var);
}

Map const &Value::map_or(Map const &what) const
{
	if (std::holds_alternative<Map>(_var)) {
		return std::get<Map>(_var);
	} else {
		return what;
	}
}

Array const &Value::array_or(Array const &what) const
{
	if (std::holds_alternative<Array>(_var)) {
		return std::get<Array>(_var);
	} else {
		return what;
	}
}

String const &Value::string_or(String const &what) const
{
	if (std::holds_alternative<String>(_var)) {
		return std::get<String>(_var);
	} else {
		return what;
	}
}

class Encoder
{
public:
	Encoder(EncodeOption const &opt)
	: _opt(opt)
	{
	}

	std::string const &get() const
	{
		return _output;
	}

	void operator()(Map const &m)
	{
		_output.push_back('{');
		endline();
		_indent++;

		for (auto &&[k, v] : m) {
			indent();
			operator()(k, true); // key
			if (_opt.pretty) {
				_output.push_back(' ');
			}
			v.visit_by(*this); // value
		}

		_indent--;
		indent();
		_output.push_back('}');
		endline();
	}

	void operator()(Array const &a)
	{
		_output.push_back('[');
		endline();
		_indent++;

		for (auto const &v : a) {
			indent();
			v.visit_by(*this); // value
		}

		_indent--;
		indent();
		_output.push_back(']');
		endline();
	}

	// key is String actually, except quoting is optional.
	void operator()(String const &s, bool is_key = false)
	{
		bool quote = true;
		if (is_key && !_opt.force_quoting) {
			quote = is_key_need_quote(s);
		}
		if (!quote) {
			_output += s;
			return;
		}

		_output.push_back('"');
		for (auto c : s) {
			switch (c) {
			case '\\': case '"':
				_output.push_back('\\');
			default:
				_output.push_back(c);
			}
		}
		_output.push_back('"');
		if (!is_key) {
			endline();
		}
	}

private:
	void indent()
	{
		if (!_opt.pretty) {
			return;
		}
		for (int i = 0; i < _indent; i++) {
			_output += "  ";
		}
	}

	void endline()
	{
		if (!_opt.pretty) {
			return;
		}
		_output.push_back('\n');
	}

private:
	std::string _output;
	EncodeOption _opt;
	int _indent{0};

}; // class Encoder

std::string encode(Value const &value, EncodeOption const &option)
{
	auto encoder = Encoder(option);
	value.visit_by(encoder);
	return encoder.get();
}

class Decoder
{
public:
	Decoder(std::string const &text)
	: _source(text)
	{
	}

	std::optional<Value> decode_value()
	{
		forward_token();
		switch (_curType) {
		case TokenType::map_begin:
			return forward_map();
		case TokenType::array_begin:
			return forward_array();
		case TokenType::quoted:
			return _cur;
		default:
			return {};
		}
	}

private:
	enum class Error {
		none,
		ending,
		map_ending,
		array_ending,
		quoted,
		unquoted,
	}; // enum class Error

	enum class TokenType {
		uninit,
		end,
		map_begin,
		map_end,
		array_begin,
		array_end,
		quoted,
		unquoted,
	}; // enum class TokenType

	// skip whitspaces
	void skip_ws()
	{
		auto pred = [](auto c)
		{
			switch (c) {
			case '\t': case '\n': case '\r': case ' ':
				return false;
			default:
				return true;
			}
		};
		_pos = std::find_if(_source.begin() + _pos, _source.end(), pred) - _source.begin();
	}

	// has reach end-of-source
	bool is_eos()
	{
		return _pos >= _source.size();
	}

	void forward_quoted(TokenType &type, String &token)
	{
		assert(_source[_pos] == '"');
		_pos++;
		while (!is_eos()) {
			auto c = _source[_pos++];
			if (c == '\\') {
				if (is_eos()) {
					token.clear();
					type = TokenType::end;
					_error = Error::ending;
					return;
				}
				c = _source[_pos++];
			} else if (c == '"') {
				break;
			}
			token.push_back(c);
		}
	}

	void forward_unquoted(TokenType &type, String &token)
	{
		auto pred = [](auto c)
		{
			// chars need no quoting
			switch (c) {
			case 'a' ... 'z':
			case 'A' ... 'Z':
			case '0' ... '9':
			case '!': case '@':
			case '#': case '$':
			case '%': case '^':
			case '&': case '*':
			case '(': case ')':
			case '_': case '-':
			case '=': case '+':
			case '|':
			case '~': case ';':
			case ',':
			case '<': case '>':
			case '/': case '?':
				return true;
			default:
				return false;
			}
		};
		auto unquoted_end = std::find_if_not(_source.begin() + _pos, _source.end(), pred);
		token = String(_source.begin() + _pos, unquoted_end);
		_pos = unquoted_end - _source.begin();
		if (token.empty()) {
			type = TokenType::end;
			_error = Error::unquoted;
		}
	}

	void parse_token(TokenType &type, String &token)
	{
		token.clear();
		type = TokenType::end;

		skip_ws();
		if (is_eos()) {
			return;
		}

		auto c = _source[_pos++];
		switch (c) {
		case '{':
			type = TokenType::map_begin;
			break;
		case '}':
			type = TokenType::map_end;
			break;
		case '[':
			type = TokenType::array_begin;
			break;
		case ']':
			type = TokenType::array_end;
			break;
		case '"':
			// quoted
			_pos--;
			type = TokenType::quoted;
			forward_quoted(type, token);
			break;
		default:
			// unquoted
			_pos--;
			type = TokenType::unquoted;
			forward_unquoted(type, token);
			break;
		}
	}

	// forward one token
	void forward_token()
	{
		if (_curType == TokenType::uninit) { // first time
			parse_token(_curType, _cur);
		} else {
			_cur = _next;
			_curType = _nextType;
		}
		parse_token(_nextType, _next); // for peek
	}

	std::optional<Value> forward_map()
	{
		std::optional<Value> ret = Map{};
		auto &m = ret.value().as_map();

	again:
		forward_token(); // key or end
		if (_curType == TokenType::quoted || _curType == TokenType::unquoted) {
			auto k = _cur;
			auto v = decode_value();
			if (v.has_value()) {
				m.insert({k, v.value()});
				goto again;
			} else {
				return {};
			}
		}
		if (_curType != TokenType::map_end) {
			_error = Error::ending;
			return {};
		}
		return ret;
	}

	std::optional<Value> forward_array()
	{
		std::optional<Value> ret = Array{};
		auto &a = ret.value().as_array();

	again:
		if (_nextType != TokenType::array_end) { // peek next token
			auto v = decode_value();
			if (v.has_value()) {
				a.push_back(v.value());
				goto again;
			} else {
				return {};
			}
		}
		forward_token(); // end token
		return ret;
	}

private:
	size_t _pos{0};
	std::string const &_source;

	TokenType _curType{ TokenType::uninit },
		  _nextType{ TokenType::uninit };

	// holding key or string value
	String _cur,
	       _next;

	Error _error{Error::none};

}; // class Decoder

std::optional<Value> decode(std::string const &mas_text)
{
	auto decoder = Decoder(mas_text);
	return decoder.decode_value();
}

bool get_string(Mas::Map const &m, std::string const &key, std::string &value)
{
	auto it = m.find(key);
	if (it != m.end()) {
		if (it->second.is_string()) {
			value = it->second.string_or({});
			return true;
		}
	}
	value.clear();
	return false;
}

} // namespace Mas
