#pragma once

#include "ast.h"
#include "types.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include <map>
#include <optional>
#include <string>

struct AstExprTypeVisitor;

/// Holds a group of functions with the same name but different definitions
struct OverloadFnGroup {
  std::vector<FnHeaderAst *> fns;

  bool eq_arg_types(std::vector<uptr<ArgDefAst>> &lhs,
                    std::vector<AstTypeId> &rhs, bool is_varadic = false) {
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
  get_fn(std::vector<AstTypeId> pre, std::vector<AstTypeId> suf) {
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

    std::vector<AstTypeId> prefix_args;
    for (auto &prefix : eq_fn->prefix_args) {
      prefix_args.push_back(prefix->type);
    }

    std::vector<AstTypeId> suffix_args;
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
  std::map<AstTypeId, llvm::Type *> llvm_types;
  std::map<std::string, EnumDefAst *> enums;
  std::map<std::string, StructDefAst *> structs;
  std::map<std::string, AstTypeId> primitives;
  std::map<std::string, uptr<MetaFunction>> defined_meta;

public:
  AstTypeDb type_db;
  llvm::Type *ptr_llvm_ty;
  std::map<std::string, VarDefStmtAst *> defined_vars;

  void define_primitive(std::string name, AstTypeId id) {
    primitives.insert({name, id});
  }

  void define_fn(std::string name, FnHeaderAst *fn) {
    auto overloads = fns[name].get();
    if (overloads == nullptr) {
      fns[name] = std::make_unique<OverloadFnGroup>();
      overloads = fns[name].get();
    }

    auto fn_info = overloads->get_fn(fn);
    if (fn_info) {
      // Function already defined. Redefine.
      overloads->fns[std::get<0>(*fn_info)] = fn;
    } else {
      overloads->fns.push_back(fn);
    }
  }

  std::optional<OverloadFnGroup *> get_overloads(std::string name) {
    auto fn = &fns[name];
    if (fn->get() != nullptr)
      return fn->get();

    /* auto fn_ext = ext_fns[name].get(); */
    /* if (fn_ext != nullptr) */
    /*   return fn_ext; */

    return std::nullopt;
  }

  void define_llvm_type(AstTypeId type_id, llvm::Type *type) {
    llvm_types[type_id] = type;
  }

  std::optional<llvm::Type *> get_llvm_type(AstTypeId id) {
    auto ty = type_db.get_type(id);
    if (!ty)
      return std::nullopt;

    if (ty.value()->is_ref())
      return ptr_llvm_ty;

    if (ty.value()->is_array()) {
      auto base_type = get_llvm_type(ty.value()->get_subtype()).value();
      return llvm::ArrayType::get(base_type, ty.value()->get_array_size());
    }

    auto type = llvm_types[id];
    if (type == nullptr)
      return std::nullopt;
    return type;
  }

  void define_enum(std::string name, EnumDefAst *value) { enums[name] = value; }

  std::optional<EnumDefAst *> get_enum(std::string name) {
    auto e = enums[name];
    if (e == nullptr)
      return std::nullopt;
    return e;
  }

  void define_struct(std::string name, StructDefAst *value) {
    /* if (structs.contains(name)) */
    /*   throw "trying to redefine a struct"; */
    /**/
    structs[name] = value;
  }

  std::optional<StructDefAst *> get_struct(std::string name) {
    if (!structs.contains(name))
      return std::nullopt;
    return structs[name];
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
