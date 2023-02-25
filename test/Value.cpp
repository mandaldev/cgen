#include "Value.hpp"
#include <iostream>
#include <cassert>

int main(int argc, const char *argv[])
{
	auto mas = Mas::Map{};

	auto &project = mas["project"].as_map();
	project["name"] = "Mas";
	project["src"] = Mas::Array{"Value.hpp", "Value.cpp"};
	project["author"] = Mas::Array{};
	project["target"] = Mas::Map{};

	{
		auto enc_opt = Mas::EncodeOption{};
		enc_opt.pretty = true;
		enc_opt.force_quoting = false;
		auto text = Mas::encode(mas, enc_opt);
		std::cout << text << "\n";
		auto decoded = Mas::decode(text);
		assert(decoded.has_value());
		text = Mas::encode(decoded.value(), enc_opt);
		std::cout << text << "\n";
	}

	{
		auto text = Mas::encode(mas);
		std::cout << text << "\n";
		auto decoded = Mas::decode(text);
		assert(decoded.has_value());
		text = Mas::encode(decoded.value());
		std::cout << text << "\n";
	}

	return 0;
}
