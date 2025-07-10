#pragma once

#include "helpers.h"
#include "types.h"
#include <expected>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <variant>
#include <vector>

struct ArrayExprAst;
struct IndexExprAst;
struct IntExprAst;
struct FloatExprAst;
struct StringExprAst;
struct BoolExprAst;
struct CallExprAst;
struct BodyExprAst;
struct FnDefAst;
struct VarExprAst;
struct PtrExprAst;
struct MetaDefExprAst;
struct StatementExprAst;
struct GroupExprAst;
struct IfExprAst;
struct ForExprAst;
struct StructExprAst;
struct MemberAccesorExprAst;
struct EnumExprAst;
struct SingleMatchExprAst;
struct RefExprAst;
struct DerefExprAst;
struct NoOpAst;

using AstExpression =
    std::variant<uptr<ArrayExprAst>, uptr<IndexExprAst>, uptr<IntExprAst>,
                 uptr<FloatExprAst>, uptr<StringExprAst>, uptr<RefExprAst>,
                 uptr<DerefExprAst>, uptr<BoolExprAst>, uptr<CallExprAst>,
                 uptr<BodyExprAst>, uptr<VarExprAst>, uptr<MetaDefExprAst>,
                 uptr<StatementExprAst>, uptr<GroupExprAst>, uptr<IfExprAst>,
                 uptr<ForExprAst>, uptr<StructExprAst>,
                 uptr<MemberAccesorExprAst>, uptr<EnumExprAst>,
                 uptr<SingleMatchExprAst>, uptr<NoOpAst>>;

struct ArgDefAst;
struct FnHeaderAst;
struct VarDefStmtAst;
struct ReturnStmtAst;
struct VarAssignStmtAst;
struct StructDefAst;
struct EnumDefAst;

using AstStatement =
    std::variant<uptr<ArgDefAst>, uptr<FnHeaderAst>, uptr<FnDefAst>,
                 uptr<StatementExprAst>, uptr<VarDefStmtAst>,
                 uptr<ReturnStmtAst>, uptr<VarAssignStmtAst>,
                 uptr<StructDefAst>, uptr<EnumDefAst>>;

enum struct MetaFunctionKind {
  AddInt,
  SubInt,
  MulInt,
  DivInt,
  ModInt,

  AddFloat,
  SubFloat,
  MulFloat,
  DivFloat,
  ModFloat,

  EqBool,
  NotEqBool,
  AndBool,
  OrBool,
  LtBool,
  GtBool,
  LtEqBool,
  GtEqBool,
};

/// Internal functions that are resolved at compile time
struct MetaFunction {
  MetaFunctionKind kind;
  int num_args;

  int arg_num() { return num_args; }
};

struct MetaDefExprAst {
  std::string name;
  std::vector<AstExpression> args;
};

/// Used for body statements that can be used as well as expressions.
/// This ignores the value of the expression.
struct StatementExprAst {
  AstExpression expr;
};

struct VarDefStmtAst {
  std::string name;

  /// It can be implicit based on the expression.
  AstTypeId type;

  std::optional<AstExpression> assignment;
};

struct NoOpAst {};

struct VarAssignStmtAst {
  /* std::string id; */
  AstExpression lvalue;
  AstExpression rvalue;
};

struct MemberAccesorExprAst {
  AstExpression base;
  std::optional<uptr<VarExprAst>> field;
  std::optional<uptr<CallExprAst>> method;
};

struct FileStmtAst {
  // TODO: imports
  std::string filename;
  std::vector<AstStatement> statements;
};

struct EnumDefAst {
  AstTypeId type;
  std::vector<AstStatement> values;
};

struct EnumExprAst {
  AstTypeId enum_type;
  uptr<StructExprAst> struct_expr;
};

struct StructDefAst {
  AstTypeId type;
  std::vector<uptr<ArgDefAst>> fields;
  std::vector<uptr<FnDefAst>> methods;
};

struct StructExprAst {
  struct StructFieldAssign {
    std::string name;
    AstExpression rvalue;
  };

  AstTypeId type;
  std::vector<uptr<StructFieldAssign>> fields;
};

struct TupleDefAst {
  AstTypeId type;
  std::vector<uptr<ArgDefAst>> fields;
};

struct ReturnStmtAst {
  std::optional<AstExpression> expr;
};

struct ArgDefAst {
  std::string name;
  AstTypeId type;
  bool is_varadic;
};

struct ArrayExprAst {
  AstTypeId type;
  std::vector<AstExpression> elements;
};

struct IndexExprAst {
  AstExpression base;
  AstExpression index;
};

struct IntExprAst {
  int value;
};

struct FloatExprAst {
  double value;
};

struct BoolExprAst {
  bool value;
};

struct StringExprAst {
  std::string value;
};

struct RefExprAst {
  AstExpression expr;
};

struct DerefExprAst {
  AstExpression expr;
};

struct VarExprAst {
  std::string name;
};

struct GroupExprAst {
  AstExpression expr;
};

struct SingleMatchExprAst {
  AstExpression enum_expr;
  uptr<VarDefStmtAst> casted_enum_var;
  AstExpression then_expr;
};

struct IfExprAst {
  AstExpression condition;
  AstExpression then_expr;
  std::optional<AstExpression> else_expr;
};

struct ForExprAst {
  AstExpression condition;
  AstExpression for_body;
};

struct CallExprAst {
  /// Non-mangled name of the function
  std::string fn_name;
  /// Actual overloaded fn reference
  AstTypeId fn_id;
  std::vector<AstExpression> prefix_args;
  std::vector<AstExpression> suffix_args;
};

/// 'main := prev | ret | next '
struct FnHeaderAst {
  std::string name;
  AstTypeId ret_type;
  bool is_external;
  std::vector<uptr<ArgDefAst>> prefix_args;
  std::vector<uptr<ArgDefAst>> suffix_args;

  bool is_vararic() {
    return suffix_args.size() > 0 &&
           suffix_args[suffix_args.size() - 1]->is_varadic;
  }

  std::vector<AstNamedType> get_prefix_named_ids() {
    std::vector<AstNamedType> ids;
    for (auto &arg : prefix_args)
      ids.push_back(AstNamedType{arg->name, arg->type, arg->is_varadic});
    return ids;
  }
  std::vector<AstNamedType> get_suffix_named_ids() {
    std::vector<AstNamedType> ids;
    for (auto &arg : suffix_args)
      ids.push_back(AstNamedType{arg->name, arg->type, arg->is_varadic});
    return ids;
  }

  /// Will treat prefix and suffix as a single list and retrieve by index.
  const ArgDefAst *get_arg_linear(int idx) {
    if (idx < (int)prefix_args.size())
      return prefix_args[idx].get();
    idx -= prefix_args.size();
    if (idx < (int)suffix_args.size())
      return suffix_args[idx].get();
    return nullptr;
  }
};

struct BodyExprAst {
  std::vector<AstStatement> statements;
};

/// Function declaration 'main := | | {}'
struct FnDefAst {
  AstTypeId id;
  uptr<FnHeaderAst> fn_header;
  std::optional<AstExpression> body;
};
