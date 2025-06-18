#pragma once

#include "ast.h"
#include "program_ctx.h"
#include <variant>

struct AstExprTypeVisitor {
  ProgramCtx *ctx;

  AstTypeId operator()(uptr<IntExprAst> &_) const { return INT_TYPE.get_id(); }

  AstTypeId operator()(uptr<FloatExprAst> &_) const {
    return FLOAT_TYPE.get_id();
  }

  AstTypeId operator()(uptr<BoolExprAst> &_) const {
    return BOOL_TYPE.get_id();
  }

  AstTypeId operator()(uptr<StringExprAst> &_) const {
    return RAW_STRING_TYPE.get_id();
  }

  AstTypeId operator()(uptr<StructExprAst> &node) const { return node->type; }
  AstTypeId operator()(uptr<EnumExprAst> &node) const {
    return node->enum_type;
  }

  AstTypeId operator()(uptr<MemberAccesorExprAst> &node) const {
    auto base_type = std::visit(*this, node->base);
    auto member = ctx->type_db.get_type(base_type).value()->get_field_by_name(
        node->member);
    if (!member)
      throw "no member found on type";
    return member.value()->type;
  }

  AstTypeId operator()(uptr<VarExprAst> &node) const {
    auto def_var = ctx->defined_vars[node->name];
    if (!def_var)
      throw "defined var not found";

    return def_var->type;
  }

  AstTypeId operator()(uptr<ArgDefAst> &node) const { return node->type; }

  // Expressions
  AstTypeId operator()(uptr<CallExprAst> &node) const {
    auto overloads = ctx->get_overloads(node->fn_name);
    if (!overloads)
      throw "no overloads found";

    std::vector<AstTypeId> prefix_types;
    for (auto &pre : node->prefix_args) {
      prefix_types.push_back(std::visit(*this, pre));
    }
    std::vector<AstTypeId> suffix_types;
    for (auto &suf : node->suffix_args) {
      suffix_types.push_back(std::visit(*this, suf));
    }

    auto fn = overloads.value()->get_fn(prefix_types, suffix_types);
    if (!fn)
      throw "no fn found in overloads";

    return std::get<1>(*fn)->ret_type;
  }

  AstTypeId operator()(uptr<BodyExprAst> &_) const {
    return VOID_TYPE.get_id();
  }

  AstTypeId operator()(uptr<StatementExprAst> &node) const {
    return std::visit(*this, node->expr);
  }

  AstTypeId operator()(uptr<GroupExprAst> &node) const {
    return std::visit(*this, node->expr);
  }

  AstTypeId operator()(uptr<IfExprAst> &node) const {
    return std::visit(*this, node->then_expr);
  }

  AstTypeId operator()(uptr<ForExprAst> &node) const {
    return std::visit(*this, node->for_body);
  }

  AstTypeId operator()(uptr<MetaDefExprAst> &node) const {
    auto meta = ctx->get_meta(node->name);
    switch (meta.value()->kind) {
    case MetaFunctionKind::AddInt:
    case MetaFunctionKind::SubInt:
    case MetaFunctionKind::MulInt:
    case MetaFunctionKind::DivInt:
    case MetaFunctionKind::ModInt:
      return INT_TYPE.get_id();

    case MetaFunctionKind::AddFloat:
    case MetaFunctionKind::SubFloat:
    case MetaFunctionKind::MulFloat:
    case MetaFunctionKind::DivFloat:
    case MetaFunctionKind::ModFloat:
      return FLOAT_TYPE.get_id();

    case MetaFunctionKind::EqBool:
    case MetaFunctionKind::NotEqBool:
    case MetaFunctionKind::LtBool:
    case MetaFunctionKind::GtBool:
    case MetaFunctionKind::LtEqBool:
    case MetaFunctionKind::GtEqBool:
    case MetaFunctionKind::AndBool:
    case MetaFunctionKind::OrBool:
      return BOOL_TYPE.get_id();

    default:
      throw "no type found for meta fn";
    }
  }
};
