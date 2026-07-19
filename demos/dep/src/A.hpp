// cgen dep demo - <one line to give the program's name and a brief idea of what it does.>
// Copyright (C) 2026  Mandal Dev
#ifndef A_HPP
#define A_HPP
#include "C.hpp"

char from_a();

static inline char export_c()
{
	return from_c();
}

#endif
