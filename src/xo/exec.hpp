#ifndef CGEN_SRC_XO_EXEC_HPP
#define CGEN_SRC_XO_EXEC_HPP
#include <string>

namespace XO {

/// Exec cmd with `system` call.
/// `exec` use shell '>' to redirect stdout and stderr to temp files,
/// so `cmd` should not use '>' or '|', etc.
int exec(std::string const &cmd, std::string const &tmpname, std::string &stdout_content, std::string &stderr_content);

} // namespace XO

#endif
