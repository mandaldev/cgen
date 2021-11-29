#include <iostream>
#include <optional>
#include <cassert>

void hw()
{
	std::cout << "Hello world!" << std::endl;
	std::optional<int> oi = 0;
	assert(oi.has_value());
}
