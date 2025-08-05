#pragma once

#include "ast.h"
#include "program_ctx.h"
#include "types.h"
#include <functional>
#include <print>
#include <stdexcept>
#include <variant>

struct AstExprTypeVisitor {
  ProgramCtx *ctx;

  template <class T> static AstTypeId get_type_id(ProgramCtx *ctx, T &expr);

  template <class T> static const AstType *get_type(ProgramCtx *ctx, T &expr);

  static AstTypeId get_type_id(ProgramCtx *ctx, AstExpression &expr);

  static const AstType *get_type(ProgramCtx *ctx, AstExpression &expr);

  AstTypeId operator()(uptr<IntExprAst> &_) const;

  AstTypeId operator()(uptr<FloatExprAst> &_) const;

  AstTypeId operator()(uptr<BoolExprAst> &_) const;

  AstTypeId operator()(uptr<StringExprAst> &_) const;

  AstTypeId operator()(uptr<ArrayExprAst> &node) const;

  AstTypeId operator()(uptr<IndexExprAst> &node) const;

  AstTypeId operator()(uptr<NoOpAst> &_) const;

  AstTypeId operator()(uptr<RefExprAst> &node) const;

  AstTypeId operator()(uptr<DerefExprAst> &node) const;

  AstTypeId operator()(uptr<StructExprAst> &node) const;
  AstTypeId operator()(uptr<EnumExprAst> &node) const;

  AstTypeId operator()(uptr<MemberAccesorExprAst> &node) const;

  AstTypeId operator()(uptr<VarExprAst> &node) const;

  AstTypeId operator()(uptr<ArgDefAst> &node) const;

  // Expressions
  AstTypeId operator()(uptr<CallExprAst> &node) const;

  AstTypeId operator()(uptr<BodyExprAst> &_) const;

  AstTypeId operator()(uptr<StatementExprAst> &node) const;

  AstTypeId operator()(uptr<GroupExprAst> &node) const;

  AstTypeId operator()(uptr<IfExprAst> &node) const;

  AstTypeId operator()(uptr<SingleMatchExprAst> &node) const;

  AstTypeId operator()(uptr<ForExprAst> &node) const;

  AstTypeId operator()(uptr<MetaDefExprAst> &node) const;
};

template <class T>
inline AstTypeId AstExprTypeVisitor::get_type_id(ProgramCtx *ctx, T &expr) {
  AstExprTypeVisitor visitor = {ctx};
  return visitor(expr);
}

template <class T>
inline const AstType *AstExprTypeVisitor::get_type(ProgramCtx *ctx, T &expr) {
  auto id = get_type_id(ctx, expr);
  return ctx->type_db.get_type(id).value();
}
