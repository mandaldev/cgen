#ifndef MAS_SRC_TARGET_HPP
#define MAS_SRC_TARGET_HPP
#include "Cdb.hpp"
#include "ProjectModel.hpp"
#include "common.hpp"
#include "pkgconf/pkgconf.hpp"
#include <Com/Mas/Value.hpp>
#include <Com/raii.hpp>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/// a cgen target
class Target
: public Com::Unique
{
	ProjectModel::TargetModel _tm;
	std::vector<ProjectModel::DepDesc> _tm_dep, _tm_pkgconf;
	std::vector<std::string>
		_tm_cflags, _tm_cflags_static, _tm_cflags_shared, _tm_cflags_private,
		_tm_lflags, _tm_lflags_static, _tm_lflags_shared, _tm_lflags_private;

	TargetType _type = TargetType::None;

	std::filesystem::path _basedir;

	// struct {
	// 	/// loaded from `_pkg`
	// 	std::string pkg_cflags, pkg_lflags;
	// } _pri, _pub; // 编译动态库时只需要 pub

	// struct {
	// 	/// resolved deps, 既保含内部的，也包含外部的
	// 	std::vector<ProjectModel::DepDesc> dep;
	// } _static, _shared; // 不同方法链接此目标时的依赖，此为间接依赖

	struct DepDesc {
		struct {
			std::vector<std::string> cflags, lflags;
		} pkg, user;
		std::vector<ProjectModel::DepDesc> dep;
	};

	// 静态库的编译和导出依赖。
	DepDesc _static;
	// 动态库的编译依赖。
	DepDesc _build_shared;
	// 动态库的导出依赖。
	DepDesc _export_shared;

	bool _deps_resolved = false; // mark, dont resolve again

	bool _phony_applyed = false;

	void load_installs();

	bool apply_override(
		std::map<std::string, std::shared_ptr<Target>> const &all_templates,
		std::function<std::string &(ProjectModel::TargetModel &)> field
	);

	bool apply_append(std::map<std::string, std::shared_ptr<Target>> const &all_templates);

	void append(std::shared_ptr<Target> target, bool include_deps = true);

	std::shared_ptr<Target> import_target(
		std::vector<std::filesystem::path> const &data_dirs,
		std::map<std::string, std::shared_ptr<Target>> &imports,
		Pkgconf &pkgconf,
		std::string const &name
	);

	/// export this target only: not include its indirect dep and pkgconf
	std::filesystem::path exports(std::filesystem::path const &prefix) const;

	bool load_pkgconfs(Pkgconf &pkgconf);

	bool load_dep_flags(
		std::map<std::string, std::shared_ptr<Target>> const &targets,
		std::map<std::string, std::shared_ptr<Target>> const &imports,
		DepDesc const &for_inner,
		DepDesc const &for_flags,
		std::vector<std::string> &inner_deps,
		std::vector<std::string> &cflags,
		std::vector<std::string> &lflags
	);
	bool gen_exe(
		std::string const &profile_name,
		Profile const &profile,
		std::map<std::string, std::shared_ptr<Target>> const &targets,
		std::map<std::string, std::shared_ptr<Target>> const &imports,
		std::string const &cc,
		std::vector<std::string> cflags,
		std::vector<std::string> lflags,
		std::optional<Cdb> &opt_cdb
	);
	bool gen_lib(
		std::string const &profile_name,
		Profile const &profile,
		std::map<std::string, std::shared_ptr<Target>> const &targets,
		std::map<std::string, std::shared_ptr<Target>> const &imports,
		std::string const &cc,
		DepDesc const &dep,
		std::vector<std::string> cflags,
		std::vector<std::string> lflags,
		std::optional<Cdb> &opt_cdb
	);

public:
	/// load target data from model
	bool load(
		std::string const &name,
		ProjectModel::TargetModel const &tm,
		std::filesystem::path const &basedir = ""
	);

	std::string name() const;

	TargetType type() const;

	ProjectModel::TargetModel const &model() const;

	std::vector<ProjectModel::DepDesc> const &dep_for_build(bool shared) const;

	std::vector<ProjectModel::DepDesc> const &dep_for_export(bool shared) const;

	// /// from pkgconf and extras
	// std::string external_cflags() const;

	// /// from pkgconf and extras
	// std::string external_lflags() const;

	/// resolve deps
	bool resolve_deps(
		std::map<std::string, std::shared_ptr<Target>> const &all_targets,
		std::vector<std::filesystem::path> const &data_dirs,
		std::map<std::string, std::shared_ptr<Target>> &imports,
		Pkgconf &pkgconf
	);

	/// generate build scripts
	bool gen(
		std::string const &profile_name,
		Profile const &profile,
		std::map<std::string, std::shared_ptr<Target>> const &targets,
		std::map<std::string, std::shared_ptr<Target>> const &imports,
		std::vector<std::filesystem::path> const &import_dirs,
		std::optional<Cdb> &opt_cdb
	);

	/// apply templates
	bool apply_templates(std::map<std::string, std::shared_ptr<Target>> const &all_templates);

	bool apply_phonys(std::map<std::string, std::shared_ptr<Target>> const &all_phonys);

	/// install target files
	bool install(
		std::filesystem::path const &prefix,
		std::string const &profile_name,
		Profile const &profile
	) const;

}; // class Target

bool uninstall_list(std::filesystem::path const &p);

#endif
