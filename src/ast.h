#pragma once

#include "helpers.h"
#include <memory>
#include <print>
#include <string>
#include <variant>
#include <vector>

struct IntExprAst;
struct FloatExprAst;
struct StringExprAst;
struct CallExprAst;
struct BodyExprAst;
struct FnDefAst;

using AstExpression =
    std::variant<uptr<IntExprAst>, uptr<FloatExprAst>, uptr<StringExprAst>,
                 uptr<CallExprAst>, uptr<BodyExprAst>>;

struct FieldDefAst;
struct FnHeaderAst;

using AstStatement = std::variant<uptr<FieldDefAst>, uptr<FnHeaderAst>, uptr<FnDefAst>>;

struct FieldDefAst {
  std::string name;
  std::string type;
  bool is_varadic;
};

struct IntExprAst {
  int value;
};

struct FloatExprAst {
  float value;
};

struct StringExprAst {
  std::string value;
};

struct CallExprAst {
  std::string fn_name;
  std::vector<AstExpression> prefix_args;
  std::vector<AstExpression> suffix_args;
};

/// 'main := prev | ret | next '
struct FnHeaderAst {
  std::string name;
  std::string type;
  std::vector<uptr<FieldDefAst>> prefix_args;
  std::vector<uptr<FieldDefAst>> suffix_args;
};

struct BodyExprAst {
  std::vector<AstExpression> exprs;
};

/// Function declaration 'main := | | {}'
struct FnDefAst {
  uptr<FnHeaderAst> fn_header;
  bool is_external;
  std::optional<AstExpression> body;
};
