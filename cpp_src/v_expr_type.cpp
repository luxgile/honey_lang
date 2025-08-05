#include "v_expr_type.h"
#include "v_llvm_ir.h"

AstTypeId AstExprTypeVisitor::operator()(uptr<MetaDefExprAst> &node) const {
  auto meta = ctx->get_meta(node->name);
  return meta.value()->get_type_id(ctx);
}

AstTypeId AstExprTypeVisitor::get_type_id(ProgramCtx *ctx,
                                          AstExpression &expr) {
  AstExprTypeVisitor visitor = {ctx};
  return std::visit(visitor, expr);
}
const AstType *AstExprTypeVisitor::get_type(ProgramCtx *ctx,
                                            AstExpression &expr) {
  auto id = get_type_id(ctx, expr);
  return ctx->type_db.get_type(id).value();
}
AstTypeId AstExprTypeVisitor::operator()(uptr<IntExprAst> &_) const {
  return INT_TYPE.get_id();
}

AstTypeId AstExprTypeVisitor::operator()(uptr<FloatExprAst> &_) const {
  return FLOAT_TYPE.get_id();
}

AstTypeId AstExprTypeVisitor::operator()(uptr<BoolExprAst> &_) const {
  return BOOL_TYPE.get_id();
}

AstTypeId AstExprTypeVisitor::operator()(uptr<StringExprAst> &_) const {
  return RAW_STRING_TYPE.get_id();
}

AstTypeId AstExprTypeVisitor::operator()(uptr<ArrayExprAst> &node) const {
  return node->type;
}

AstTypeId AstExprTypeVisitor::operator()(uptr<IndexExprAst> &node) const {
  auto base_type_id = std::visit(*this, node->base);
  auto base_type = ctx->type_db.get_type(base_type_id).value();
  if (!base_type->is_array())
    throw std::runtime_error(
        "trying to index an expression that's not an array.");
  return base_type->get_subtype();
}

AstTypeId AstExprTypeVisitor::operator()(uptr<NoOpAst> &_) const {
  return VOID_TYPE.get_id();
}

AstTypeId AstExprTypeVisitor::operator()(uptr<RefExprAst> &node) const {
  auto expr_type_id = std::visit(*this, node->expr);
  auto ref_ty_id = AstType::new_reference(expr_type_id, &ctx->type_db);
  auto ref = ctx->type_db.get_type(ref_ty_id.get_id());
  if (!ref)
    return ctx->type_db.new_ref(expr_type_id);
  return ref.value()->get_id();
}

AstTypeId AstExprTypeVisitor::operator()(uptr<DerefExprAst> &node) const {
  auto expr_ptr_id = std::visit(*this, node->expr);
  auto expr_ptr_type = ctx->type_db.get_type(expr_ptr_id);
  return expr_ptr_type.value()->get_subtype();
}

AstTypeId AstExprTypeVisitor::operator()(uptr<StructExprAst> &node) const {
  return node->type;
}

AstTypeId AstExprTypeVisitor::operator()(uptr<EnumExprAst> &node) const {
  return node->enum_type;
}

AstTypeId
AstExprTypeVisitor::operator()(uptr<MemberAccesorExprAst> &node) const {
  auto base_type_id = std::visit(*this, node->base);
  auto base_type = ctx->type_db.get_type(base_type_id).value();

  if (base_type->is_ref()) {
    base_type_id = base_type->get_subtype();
    base_type = ctx->type_db.get_type(base_type_id).value();
  }

  if (node->field)
    return base_type->get_field_by_name(node->field.value()->name)
        .value()
        ->type;
  throw "no member found on type";
}

AstTypeId AstExprTypeVisitor::operator()(uptr<VarExprAst> &node) const {
  auto def_var = ctx->defined_vars[node->name];
  if (!def_var) {
    std::println("defined var '{}' not found", node->name);
    throw "defined var not found";
  }

  return def_var->type;
}

AstTypeId AstExprTypeVisitor::operator()(uptr<ArgDefAst> &node) const {
  return node->type;
}

AstTypeId AstExprTypeVisitor::operator()(uptr<CallExprAst> &node) const {
  std::vector<AstNamedType> prefix_types;
  for (auto &pre : node->prefix_args) {
    prefix_types.push_back(AstNamedType{"", std::visit(*this, pre), false});
  }
  std::vector<AstNamedType> suffix_types;
  for (auto &suf : node->suffix_args) {
    suffix_types.push_back(AstNamedType{"", std::visit(*this, suf), false});
  }

  auto fn =
      ctx->type_db.get_fn_by_args(node->fn_name, prefix_types, suffix_types);
  if (!fn)
    throw std::runtime_error("no fn found in overloads");

  return ctx->type_db.get_type(*fn).value()->get_return();
}

AstTypeId AstExprTypeVisitor::operator()(uptr<BodyExprAst> &_) const {
  return VOID_TYPE.get_id();
}

AstTypeId AstExprTypeVisitor::operator()(uptr<StatementExprAst> &node) const {
  return std::visit(*this, node->expr);
}

AstTypeId AstExprTypeVisitor::operator()(uptr<GroupExprAst> &node) const {
  return std::visit(*this, node->expr);
}

AstTypeId AstExprTypeVisitor::operator()(uptr<IfExprAst> &node) const {
        return std::visit(*this, node->then_expr);
}

AstTypeId AstExprTypeVisitor::operator()(uptr<SingleMatchExprAst> &node) const {
  return std::visit(*this, node->then_expr);
}

AstTypeId AstExprTypeVisitor::operator()(uptr<ForExprAst> &node) const {
  return std::visit(*this, node->for_body);
}

