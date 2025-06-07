#pragma once

#include "helpers.h"
#include "llvm/IR/Type.h"
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <variant>
#include <vector>

using AstTypeId = std::size_t;

struct AstType {
  AstTypeId id;
  std::string name;

  AstType(std::string name) : name(name) {
    id = std::hash<std::string>{}(name);
  }

  bool operator==(const AstType &rhs) const { return id == rhs.id; }

  bool is_void();
};

const AstType VOID_TYPE = AstType{"Void"};
const AstType BOOL_TYPE = AstType{"Bool"};
const AstType INT_TYPE = AstType{"Int"};
const AstType FLOAT_TYPE = AstType{"Float"};
const AstType RAW_STRING_TYPE = AstType{"RawString"};

struct IntExprAst;
struct FloatExprAst;
struct StringExprAst;
struct BoolExprAst;
struct CallExprAst;
struct BodyExprAst;
struct FnDefAst;
struct VarExprAst;
struct MetaDefExprAst;
struct StatementExprAst;
struct GroupExprAst;
struct IfExprAst;
struct ForExprAst;

using AstExpression =
    std::variant<uptr<IntExprAst>, uptr<FloatExprAst>, uptr<StringExprAst>,
                 uptr<BoolExprAst>, uptr<CallExprAst>, uptr<BodyExprAst>,
                 uptr<VarExprAst>, uptr<MetaDefExprAst>, uptr<StatementExprAst>,
                 uptr<GroupExprAst>, uptr<IfExprAst>, uptr<ForExprAst>>;

struct ArgDefAst;
struct FnHeaderAst;
struct VarDefStmtAst;
struct ReturnStmtAst;
struct VarAssignStmtAst;

using AstStatement =
    std::variant<uptr<ArgDefAst>, uptr<FnHeaderAst>, uptr<FnDefAst>,
                 uptr<StatementExprAst>, uptr<VarDefStmtAst>,
                 uptr<ReturnStmtAst>, uptr<VarAssignStmtAst>>;

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

/// Cannot be defined on Honey code, only internal implementation.
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
  std::optional<AstType> type;

  std::optional<AstExpression> assignment;
};

struct VarAssignStmtAst {
  std::string id;
  AstExpression rvalue;
};

struct ReturnStmtAst {
  std::optional<AstExpression> expr;
};

struct ArgDefAst {
  std::string name;
  AstType type;
  bool is_varadic;
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

struct VarExprAst {
  std::string name;
};

struct GroupExprAst {
  AstExpression expr;
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
  std::string fn_name;
  std::vector<AstExpression> prefix_args;
  std::vector<AstExpression> suffix_args;
};

/// 'main := prev | ret | next '
struct FnHeaderAst {
  std::string name;
  AstType ret_type;
  bool is_external;
  std::vector<uptr<ArgDefAst>> prefix_args;
  std::vector<uptr<ArgDefAst>> suffix_args;

  bool is_vararic() {
    return suffix_args.size() > 0 &&
           suffix_args[suffix_args.size() - 1]->is_varadic;
  }
};

struct BodyExprAst {
  std::vector<AstStatement> statements;
};

/// Function declaration 'main := | | {}'
struct FnDefAst {
  uptr<FnHeaderAst> fn_header;
  std::optional<AstExpression> body;
};
