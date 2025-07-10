#pragma once

#include "ast.h"
#include "types.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

struct ProgramCtx {
private:
  /* std::map<std::string, uptr<OverloadFnGroup>> fns; */
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

  /// Helper to register a fn into the type db.
  AstTypeId define_fn(std::string name, FnHeaderAst *fn,
                      std::optional<AstTypeId> parent_struct) {
    // Convert arguments
    std::vector<AstNamedType> pre_args;
    for (auto &pre : fn->prefix_args)
      pre_args.push_back(AstNamedType{pre->name, pre->type, false});

    std::vector<AstNamedType> su_args;
    for (auto &su : fn->suffix_args)
      su_args.push_back(AstNamedType{su->name, su->type, su->is_varadic});

    auto fns = type_db.get_fns_by_name(name);
    auto overload_n = fns.size();
    auto registered_fn = type_db.get_fn_by_args(name, pre_args, su_args);
    if (registered_fn) {
      auto registered_fn_ty = type_db.get_type(*registered_fn);
      overload_n = registered_fn_ty.value()->get_overload();
    }

    auto fn_ty = type_db.new_fn(name, overload_n, pre_args, su_args,
                                fn->ret_type, parent_struct);

    return fn_ty;
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

    if (!llvm_types.contains(id))
      return std::nullopt;
    return llvm_types[id];
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
