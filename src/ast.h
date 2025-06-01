#pragma once

#include <memory>
#include <print>
#include <string>
#include <vector>

struct AstNode {
  virtual ~AstNode() {}

  virtual void print_node() {}
};

struct FieldDefAst : AstNode {
  std::string name;
  std::string type;

  FieldDefAst(std::string name, std::string type)
      : name(std::move(name)), type(std::move(type)) {}

  void print_node() override { std::println("{}: {}", name, type); }
};

struct ExprAst : AstNode {};

struct IntExprAst : ExprAst {
  int value;

  void print_node() override { std::println("{}", value); }
};

struct FloatExprAst : ExprAst {
  float value;

  void print_node() override { std::println("{}", value); }
};

struct StringExprAst : ExprAst {
  std::string value;

  explicit StringExprAst(std::string value) : value(std::move(value)) {}

  void print_node() override { std::println("{}", value); }
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

  void print_node() override {
    for (auto &arg : prefix_args) {
      arg->print_node();
    }

    std::print(" |{}| ", fn_name);

    for (auto &arg : suffix_args) {
      arg->print_node();
    }
  }
};

struct FnHeaderAst : AstNode {
  std::string name;
  std::string type;
  std::vector<FieldDefAst> prefix_args;
  std::vector<FieldDefAst> suffix_args;

  FnHeaderAst(std::string name, std::string type,
              std::vector<FieldDefAst> prefix_args,
              std::vector<FieldDefAst> suffix_args)
      : name(std::move(name)), type(std::move(type)),
        prefix_args(std::move(prefix_args)),
        suffix_args(std::move(suffix_args)) {}

  void print_node() override {
    std::print("{} := ", name);

    for (auto &arg : prefix_args) {
      arg.print_node();
    }

    std::print("|{}|", type);

    for (auto &arg : suffix_args) {
      arg.print_node();
    }
  }
};

struct BodyExprAst : ExprAst {
  std::vector<std::unique_ptr<ExprAst>> exprs;

  explicit BodyExprAst(std::vector<std::unique_ptr<ExprAst>> exprs)
      : exprs(std::move(exprs)) {}

  void print_node() override {
    std::println("{{");
    for (auto& expr : exprs) {
      expr->print_node();
    }
    std::println("}}");
  }
};

struct FnExprAst : ExprAst {
  std::unique_ptr<FnHeaderAst> fn_header;
  std::unique_ptr<ExprAst> body;

  FnExprAst(std::unique_ptr<FnHeaderAst> fn_header,
            std::unique_ptr<ExprAst> body)
      : fn_header(std::move(fn_header)), body(std::move(body)) {}

  void print_node() override {
    fn_header->print_node();
    body->print_node();
  }
};
