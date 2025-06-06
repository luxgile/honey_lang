#pragma once

#include "ast.h"
#include "llvm/IR/Type.h"
#include <map>
#include <variant>

struct AstExprTypeVisitor;

/// Holds a group of functions with the same name but different definitions
struct OverloadFnGroup {
  std::vector<FnHeaderAst *> fns;

  // TODO: Terribly inneficient. Start using types as ids instead of strings to
  // improve checks.

  bool eq_arg_types(std::vector<uptr<ArgDefAst>> &lhs,
                    std::vector<std::string> &rhs, bool is_varadic = false) {
    if (lhs.size() != rhs.size() && !is_varadic)
      return false;

    for (int i = 0; i < (int)lhs.size(); i++) {
      if (!lhs[i]->is_varadic && lhs[i]->type != rhs[i])
        return false;
    }

    return true;
  }

  /// Returns the fn and the index it was found.
  std::optional<std::tuple<int, FnHeaderAst *>>
  get_fn(std::vector<std::string> pre, std::vector<std::string> suf) {
    for (int i = 0; i < (int)fns.size(); i++) {
      auto fn = fns[i];

      if (!eq_arg_types(fn->prefix_args, pre))
        continue;

      if (!eq_arg_types(fn->suffix_args, suf, fn->is_vararic()))
        continue;

      return std::tuple(i, fn);
    }
    return {};
  }

  /// Returns the fn and the index it was found.
  std::optional<std::tuple<int, FnHeaderAst *>> get_fn(FnHeaderAst *eq_fn) {
    if (fns.size() == 0 || fns[0]->name != eq_fn->name)
      return std::nullopt;

    std::vector<std::string> prefix_args;
    for (auto &prefix : eq_fn->prefix_args) {
      prefix_args.push_back(prefix->type);
    }

    std::vector<std::string> suffix_args;
    for (auto &suffix : eq_fn->suffix_args) {
      suffix_args.push_back(suffix->type);
    }

    return get_fn(prefix_args, suffix_args);
  }

  std::optional<std::string> get_mangled_name(FnHeaderAst *fn);

  std::optional<std::string> get_mangled_name(CallExprAst *call,
                                              AstExprTypeVisitor *type_visitor);
};

struct ProgramCtx {
private:
  std::map<std::string, uptr<OverloadFnGroup>> fns;
  std::map<std::string, uptr<OverloadFnGroup>> ext_fns;
  std::map<std::string, llvm::Type *> types;
  std::map<std::string, uptr<VarExprAst>> variables;
  std::map<std::string, uptr<MetaFunction>> defined_meta;

public:
  std::map<std::string, VarDefStmtAst *> defined_vars;

  void define_fn(std::string name, FnHeaderAst *fn) {
    auto overloads = fns[name].get();
    if (overloads == nullptr) {
      fns[name] = std::make_unique<OverloadFnGroup>();
      overloads = fns[name].get();
    }
    overloads->fns.push_back(fn);
  }

  void define_fn_ext(std::string name, FnHeaderAst *fn) {
    auto overloads = ext_fns[name].get();
    if (overloads == nullptr) {
      fns[name] = std::make_unique<OverloadFnGroup>();
      overloads = ext_fns[name].get();
    }
    overloads->fns.push_back(fn);
  }

  std::optional<OverloadFnGroup *> get_overloads(std::string name) {
    auto fn = fns[name].get();
    if (fn != nullptr)
      return fn;

    auto fn_ext = ext_fns[name].get();
    if (fn_ext != nullptr)
      return fn_ext;

    return std::nullopt;
  }

  std::vector<FnHeaderAst *> get_all_ext_fn() {
    std::vector<FnHeaderAst *> fns;
    for (auto it = ext_fns.begin(); it != ext_fns.end(); it++) {
      for (auto fn : it->second.get()->fns) {
        fns.push_back(fn);
      }
    }
    return fns;
  }

  void define_type(std::string name, llvm::Type *type) { types[name] = type; }

  std::optional<llvm::Type *> get_type(std::string name) {
    auto type = types[name];
    if (type == nullptr)
      return std::nullopt;
    return type;
  }

  void define_var(std::string name, uptr<VarExprAst> &value) {
    variables[name] = std::move(value);
  }

  std::optional<VarExprAst *> get_var(std::string name) {
    auto var = &variables[name];
    if (var == nullptr)
      return std::nullopt;
    return var->get();
  }

  void define_meta(std::string name, uptr<MetaFunction> meta) {
    defined_meta[name] = std::move(meta);
  }

  std::optional<MetaFunction *> get_meta(std::string name) {
    auto meta = &defined_meta[name];
    if (meta->get() == nullptr)
      return std::nullopt;
    return meta->get();
  }
};
