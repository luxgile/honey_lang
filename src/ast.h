#pragma once

#include <memory>
#include <string>
#include <vector>

struct FieldDefAst {
  std::string name;
  std::string type;
};

struct ExprAst {};

struct IntExprAst : ExprAst {
  int value;
};

struct FloatExprAst : ExprAst {
  float value;
};

struct StringExprAst : ExprAst {
  std::string value;

  explicit StringExprAst(std::string value) : value(std::move(value)) {}
};

struct CallExprAst : ExprAst {
  std::string fn_name;
  std::vector<std::unique_ptr<ExprAst>> prefix_args;
  std::vector<std::unique_ptr<ExprAst>> suffix_args;

  CallExprAst(std::string fn_name,
              std::vector<std::unique_ptr<ExprAst>> prefix_args,
              std::vector<std::unique_ptr<ExprAst>> suffix_args)
      : fn_name(std::move(fn_name)), prefix_args(std::move(prefix_args)),
        suffix_args(std::move(suffix_args)) {}
};

struct FnHeaderAst {
  std::string name;
  std::string type;
  std::vector<FieldDefAst> prefix_args;
  std::vector<FieldDefAst> suffix_args;
};

struct BodyExprAst : ExprAst {
  std::vector<std::unique_ptr<ExprAst>> exprs;

  explicit BodyExprAst(std::vector<std::unique_ptr<ExprAst>> exprs)
      : exprs(std::move(exprs)) {}
};

struct FnExprAst : ExprAst {
  std::unique_ptr<FnHeaderAst> fn_header;
  std::unique_ptr<ExprAst> body;
};
