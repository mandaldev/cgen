#ifndef CGEN_SRC_CDB_H
#define CGEN_SRC_CDB_H
#include <filesystem>
#include <set>
#include <string>
#include <vector>

/// compilation database item
struct CdbItem
{
	std::string cmd; // gcc or g++
	std::string flags;
	std::set<std::filesystem::path> files; // not just one file, all the same in one target
};

using Cdb = std::vector<CdbItem>;

bool gen_cdb_file(Cdb const &cdb, std::filesystem::path const &dir);

#endif
