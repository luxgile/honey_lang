#pragma once

#include "ast.h"

struct PrettyPrintAstVisitor {
  int indent = 0;

  void print_indent() {
    for (int i = 0; i < indent; i++) {
      std::print("  ");
    }
  }

  // Literals
  void operator()(uptr<IntExprAst> &node);

  void operator()(uptr<FloatExprAst> &node);

  void operator()(uptr<StringExprAst> &node);

  void operator()(uptr<VarExprAst> &node);

  void operator()(uptr<ArgDefAst> &node);

  // Expressions
  void operator()(uptr<CallExprAst> &node);

  // Function
  void operator()(uptr<FnHeaderAst> &node);

  void operator()(uptr<BodyExprAst> &node);

  void operator()(uptr<StatementExprAst> &node);

  void operator()(uptr<FnDefAst> &node);

  void operator()(uptr<MetaDefExprAst> &node);

  void operator()(uptr<VarDefStmtAst> &node);

  void operator()(uptr<ReturnStmtAst> &node);
};
