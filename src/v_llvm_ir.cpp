#include "v_llvm_ir.h"
#include "ast.h"
#include "types.h"
#include "v_expr_type.h"
#include "llvm/IR/Value.h"
#include <variant>

std::expected<void, std::string>
LlvmIrGenAstVisitor::build_var(GenCtx *gctx, VarDefStmtAst &node) {
  llvm::Type *assignment_llvm_type;

  if (node.assignment) {
    AstExprTypeVisitor type_visitor = {ctx};
    auto ast_type = std::visit(type_visitor, *node.assignment);

    auto assignment_llvm_type_res = ctx->get_llvm_type(ast_type);
    if (!assignment_llvm_type_res)
      return std::unexpected(
          "no llvm type found for assigment in var declaration");
    assignment_llvm_type = assignment_llvm_type_res.value();

    auto _ty = ctx->get_llvm_type(ast_type);
    if (_ty && assignment_llvm_type != _ty.value())
      return std::unexpected(
          "explicit type and assigment expression type mismatch");

    if (assignment_llvm_type->isVoidTy())
      return std::unexpected("trying to allocate a void type");

    auto alloca = builder->CreateAlloca(*_ty, nullptr, node.name);
    gctx->defined_variables[node.name] =
        DefinedVariable{ast_type, assignment_llvm_type, alloca};

    auto ir_res = store_in_value(gctx, alloca, ast_type, *node.assignment);
    if (!ir_res)
      return std::unexpected(ir_res.error());

  } else {
    auto _ty = ctx->get_llvm_type(node.type);
    if (!_ty)
      return std::unexpected("type undefined found for var definition");

    assignment_llvm_type = _ty.value();

    if (assignment_llvm_type->isVoidTy())
      return std::unexpected("trying to allocate a void type");

    auto alloca =
        builder->CreateAlloca(assignment_llvm_type, nullptr, node.name);
    gctx->defined_variables[node.name] =
        DefinedVariable{node.type, assignment_llvm_type, alloca};
  }

  return {};
}
std::expected<void, std::string>
LlvmIrGenAstVisitor::build_var_assignment(GenCtx *gctx,
                                          VarAssignStmtAst &node) {
  gctx->var_lassign_mode = true;
  auto lvalue = build_expr(gctx, node.lvalue);
  gctx->var_lassign_mode = false;
  auto lvalue_type = AstExprTypeVisitor::get_type_id(ctx, node.lvalue);

  auto ir_res = store_in_value(gctx, *lvalue, lvalue_type, node.rvalue);
  if (!ir_res)
    return std::unexpected(ir_res.error());
  return {};
}

std::expected<void, std::string>
LlvmIrGenAstVisitor::store_in_value(GenCtx *gctx, llvm::Value *ptr,
                                    AstTypeId ptr_ty_id, AstExpression &expr) {
  LlvmStoreAllocaVisitor store_visitor = {builder.get(), ptr, this};
  auto ir_res = store_visitor.build_alloca(gctx, expr);
  if (!ir_res)
    return std::unexpected(ir_res.error());
  return {};
}
