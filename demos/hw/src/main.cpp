#include <iostream>
#include <optional>

int main(int argc, const char *argv[])
{
	std::cout << "Hello world!" << std::endl;
	std::optional<int> oi = 0;
	return oi.value();
}
