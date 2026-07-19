// cgen dep demo - <one line to give the program's name and a brief idea of what it does.>
// Copyright (C) 2026  Mandal Dev
#include "G.hpp"
#include "A.hpp"
#include "D.hpp"

char from_g()
{
	if (from_a() == 'A' && from_d() == 'D') {
		return 'G';
	}
	return true;
}
