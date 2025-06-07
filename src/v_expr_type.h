#pragma once

#include "ast.h"
#include "program_ctx.h"
#include <exception>

struct AstExprTypeVisitor {
  ProgramCtx *ctx;

  AstType operator()(uptr<IntExprAst> &_) { return INT_TYPE; }

  AstType operator()(uptr<FloatExprAst> &_) { return FLOAT_TYPE; }

  AstType operator()(uptr<BoolExprAst> &_) { return BOOL_TYPE; }

  AstType operator()(uptr<StringExprAst> &_) { return RAW_STRING_TYPE; }

  AstType operator()(uptr<VarExprAst> &node) {
    auto def_var = ctx->defined_vars[node->name];
    if (!def_var)
      throw "defined var not found";

    if (def_var->type)
      return *def_var->type;

    if (def_var->assignment)
      return std::visit(*this, *def_var->assignment);

    throw "unreacheable code";
  }

  AstType operator()(uptr<ArgDefAst> &node) { return node->type; }

  // Expressions
  AstType operator()(uptr<CallExprAst> &node) {
    auto overloads = ctx->get_overloads(node->fn_name);
    if (!overloads)
      throw "no overloads found";

    std::vector<AstType> prefix_types;
    for (auto &pre : node->prefix_args) {
      prefix_types.push_back(std::visit(*this, pre));
    }
    std::vector<AstType> suffix_types;
    for (auto &suf : node->suffix_args) {
      suffix_types.push_back(std::visit(*this, suf));
    }

    auto fn = overloads.value()->get_fn(prefix_types, suffix_types);
    if (!fn)
      throw "no fn found in overloads";

    return std::get<1>(*fn)->ret_type;
  }

  AstType operator()(uptr<BodyExprAst> &_) { return VOID_TYPE; }

  AstType operator()(uptr<StatementExprAst> &node) {
    return std::visit(*this, node->expr);
  }

  AstType operator()(uptr<GroupExprAst> &node) {
    return std::visit(*this, node->expr);
  }

  AstType operator()(uptr<IfExprAst> &node) {
    return std::visit(*this, node->then_expr);
  }

  AstType operator()(uptr<ForExprAst> &node) {
    return std::visit(*this, node->for_body);
  }

  AstType operator()(uptr<MetaDefExprAst> &node) {
    auto meta = ctx->get_meta(node->name);
    switch (meta.value()->kind) {
    case MetaFunctionKind::AddInt:
    case MetaFunctionKind::SubInt:
    case MetaFunctionKind::MulInt:
    case MetaFunctionKind::DivInt:
    case MetaFunctionKind::ModInt:
      return INT_TYPE;

    case MetaFunctionKind::AddFloat:
    case MetaFunctionKind::SubFloat:
    case MetaFunctionKind::MulFloat:
    case MetaFunctionKind::DivFloat:
    case MetaFunctionKind::ModFloat:
      return FLOAT_TYPE;

    case MetaFunctionKind::EqBool:
    case MetaFunctionKind::NotEqBool:
    case MetaFunctionKind::LtBool:
    case MetaFunctionKind::GtBool:
    case MetaFunctionKind::LtEqBool:
    case MetaFunctionKind::GtEqBool:
    case MetaFunctionKind::AndBool:
    case MetaFunctionKind::OrBool:
      return BOOL_TYPE;

    default:
      throw "no type found for meta fn";
    }
  }
};
