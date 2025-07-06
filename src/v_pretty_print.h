#pragma once

#include "ast.h"
#include "program_ctx.h"
#include <print>
#include <variant>

struct PrettyPrintAstVisitor {
  ProgramCtx *ctx;
  int indent = 0;

  void print_indent() {
    for (int i = 0; i < indent; i++) {
      std::print("  ");
    }
  }

  std::string get_type_name(AstTypeId id) {
    auto type = ctx->type_db.get_type(id);
    if(!type) return "?";
    return type.value()->get_name();
  }

  void operator()(uptr<IntExprAst> &node);

  void operator()(uptr<FloatExprAst> &node);

  void operator()(uptr<StringExprAst> &node);

  void operator()(uptr<BoolExprAst> &node);

  void operator()(uptr<VarExprAst> &node);

  void operator()(uptr<ArgDefAst> &node);

  void operator()(uptr<CallExprAst> &node);

  void operator()(uptr<FnHeaderAst> &node);

  void operator()(uptr<BodyExprAst> &node);

  void operator()(uptr<StatementExprAst> &node);

  void operator()(uptr<FnDefAst> &node);

  void operator()(uptr<MetaDefExprAst> &node);

  void operator()(uptr<VarDefStmtAst> &node);

  void operator()(uptr<ReturnStmtAst> &node);

  void operator()(uptr<GroupExprAst> &node);

  void operator()(uptr<IfExprAst> &node);

  void operator()(uptr<SingleMatchExprAst> &node);

  void operator()(uptr<ForExprAst> &node);

  void operator()(uptr<VarAssignStmtAst> &node);

  void operator()(uptr<StructDefAst> &node);

  void operator()(uptr<StructExprAst> &node);

  void operator()(uptr<MemberAccesorExprAst> &node);

  void operator()(uptr<EnumDefAst> &node);

  void operator()(uptr<EnumExprAst> &node);

  void operator()(uptr<RefExprAst> &node);
  void operator()(uptr<DerefExprAst> &node);
  void operator()(uptr<NoOpAst> &node) {}
};
