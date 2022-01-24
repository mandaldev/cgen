#include "lib.hpp"
#include "../slib1/lib.hpp"
#include <iostream>

int dlib1_main(int argc, const char *argv[])
{
	std::cout << slib1_get_message() << std::endl;
	return 0;
}
