#ifndef CGEN_SRC_CDB_H
#define CGEN_SRC_CDB_H
#include <vector>
#include <string>
#include <set>
#include <algorithm>

/// compilation database item
struct CdbItem
{
	std::string cmd; // gcc or g++
	std::string flags;
	std::set<std::string> files; // not just one file, all the same in one target
};

using Cdb = std::vector<CdbItem>;

#endif
