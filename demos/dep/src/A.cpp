// cgen dep demo - <one line to give the program's name and a brief idea of what it does.>
// Copyright (C) 2026  Mandal Dev
#include "A.hpp"
#include "B.hpp"

char from_a()
{
	if (from_b() == 'B' && from_c() == 'C') {
		return 'A';
	}
	return '0';
}
