#pragma once

#include "ast.h"
#include "types.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"
#include <map>
#include <optional>
#include <string>

struct MetaFunction;

struct ProgramCtx {
private:
  std::map<AstTypeId, llvm::Type *> llvm_types;
  std::map<std::string, EnumDefAst *> enums;
  std::map<std::string, StructDefAst *> structs;
  std::map<std::string, AstTypeId> primitives;
  std::map<std::string, MetaFunction *> defined_meta;


public:
  AstTypeDb type_db;
  llvm::Type *ptr_llvm_ty;
  std::map<std::string, VarDefStmtAst *> defined_vars;

  ~ProgramCtx() {
    for (auto &pair : defined_meta) {
      free(pair.second);
    }
  }

  void define_primitive(std::string name, AstTypeId id);

  /// Helper to register a fn into the type db.
  AstTypeId define_fn(std::string name, FnHeaderAst *fn,
                      std::optional<AstTypeId> parent_struct);

  void define_llvm_type(AstTypeId type_id, llvm::Type *type);

  std::optional<llvm::Type *> get_llvm_type(AstTypeId id);

  void define_enum(std::string name, EnumDefAst *value);

  std::optional<EnumDefAst *> get_enum(std::string name);

  void define_struct(std::string name, StructDefAst *value);

  std::optional<StructDefAst *> get_struct(std::string name);

  void define_meta(std::string name, MetaFunction *meta);

  std::optional<MetaFunction *> get_meta(std::string name);
};
