#ifndef CGEN_SRC_XO_EXEC_HPP
#define CGEN_SRC_XO_EXEC_HPP
#include <string>
#include <filesystem>

namespace XO {

/// Exec cmd with `system` call.
/// `exec` use shell '>' to redirect stdout and stderr to temp files,
/// so `cmd` should not use '>' or '|', etc.
int exec(std::string const &cmd, std::string const &tmpname, std::string &stdout_content, std::string &stderr_content);

/// get target path of `/proc/self/exe`
std::filesystem::path exec_path();

/// like `/usr/bin/cgen`, return `/usr`
std::filesystem::path exec_install_prefix();

} // namespace XO

#endif
