#pragma once

#include "ast.h"
#include "program_ctx.h"

struct AstExprTypeVisitor {
  ProgramCtx *ctx;

  const std::string UNDEFINED = "Undefined";

  std::string operator()(uptr<IntExprAst> &node) { return "Int"; }

  std::string operator()(uptr<FloatExprAst> &node) { return "Float"; }

  std::string operator()(uptr<StringExprAst> &node) { return "RawString"; }

  std::string operator()(uptr<VarExprAst> &node) {
    auto def_var = ctx->defined_vars[node->name];
    if (!def_var)
      return UNDEFINED;

    if (def_var->type)
      return *def_var->type;

    if (def_var->assignment)
      return std::visit(*this, *def_var->assignment);

    return UNDEFINED;
  }

  std::string operator()(uptr<ArgDefAst> &node) { return node->type; }

  // Expressions
  std::string operator()(uptr<CallExprAst> &node) {
    auto overloads = ctx->get_overloads(node->fn_name);
    if (!overloads)
      return UNDEFINED;
    std::vector<std::string> prefix_types;
    for (auto &pre : node->prefix_args) {
      prefix_types.push_back(std::visit(*this, pre));
    }
    std::vector<std::string> suffix_types;
    for (auto &suf : node->suffix_args) {
      suffix_types.push_back(std::visit(*this, suf));
    }

    auto fn = overloads.value()->get_fn(prefix_types, suffix_types);
    auto ret_type = std::get<1>(*fn)->ret_type;
    return ret_type ? *ret_type : "Void";
  }

  std::string operator()(uptr<BodyExprAst> &node) { return "Void"; }

  std::string operator()(uptr<StatementExprAst> &node) {
    return std::visit(*this, node->expr);
  }

  std::string operator()(uptr<GroupExprAst> &node) {
    return std::visit(*this, node->expr);
  }

  std::string operator()(uptr<MetaDefExprAst> &node) {
    auto meta = ctx->get_meta(node->name);
    switch (meta.value()->kind) {
    case MetaFunctionKind::AddInt:
    case MetaFunctionKind::SubInt:
    case MetaFunctionKind::MulInt:
    case MetaFunctionKind::DivInt:
    case MetaFunctionKind::ModInt:
      return "Int";

    case MetaFunctionKind::AddFloat:
    case MetaFunctionKind::SubFloat:
    case MetaFunctionKind::MulFloat:
    case MetaFunctionKind::DivFloat:
    case MetaFunctionKind::ModFloat:
      return "Float";

    default:
      return UNDEFINED;
    }
  }
};
