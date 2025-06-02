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
struct VarExprAst;

using AstExpression =
    std::variant<uptr<IntExprAst>, uptr<FloatExprAst>, uptr<StringExprAst>,
                 uptr<CallExprAst>, uptr<BodyExprAst>, uptr<VarExprAst>>;

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

struct VarExprAst {
  std::string name;
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
  bool is_external;
  std::vector<uptr<FieldDefAst>> prefix_args;
  std::vector<uptr<FieldDefAst>> suffix_args;

  bool is_vararic() { return suffix_args.size() > 0 && suffix_args[suffix_args.size() - 1]->is_varadic; }
};

struct BodyExprAst {
  std::vector<AstExpression> exprs;
};

/// Function declaration 'main := | | {}'
struct FnDefAst {
  uptr<FnHeaderAst> fn_header;
  std::optional<AstExpression> body;
};
