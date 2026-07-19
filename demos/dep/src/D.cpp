// cgen dep demo - <one line to give the program's name and a brief idea of what it does.>
// Copyright (C) 2026  Mandal Dev
#include "D.hpp"
#include "E.hpp"
#include "F.hpp"

char from_d()
{
	if (from_e() == 'E' && from_f() == 'F') {
		return 'D';
	}
	return '0';
}
