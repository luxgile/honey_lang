#include "v_llvm_ir.h"
#include "v_expr_type.h"
#include <variant>

std::expected<void, std::string>
LlvmIrGenAstVisitor::build_var(VarDefStmtAst &node) {
  if (!node.assignment && !node.type)
    return std::unexpected("could not deduce type for var definition");

  llvm::Type *assignment_llvm_type;

  if (node.assignment) {
    AstExprTypeVisitor type_visitor = {ctx};
    auto ast_type = std::visit(type_visitor, *node.assignment);

    auto assignment_llvm_type_res = ctx->get_llvm_type(ast_type);
    if(!assignment_llvm_type_res)
      return std::unexpected("no llvm type found for assigment in var declaration");
    assignment_llvm_type = assignment_llvm_type_res.value();

    if (node.type) {
      auto _ty = ctx->get_llvm_type(*node.type);
      if (_ty && assignment_llvm_type != _ty.value())
        return std::unexpected(
            "explicit type and assigment expression type mismatch");
    }

    if (assignment_llvm_type->isVoidTy())
      return std::unexpected("trying to allocate a void type");

    auto alloca =
        builder->CreateAlloca(assignment_llvm_type, nullptr, node.name);
    defined_variables[node.name] =
        DefinedVariable{assignment_llvm_type, alloca};

    LlvmStoreAllocaVisitor store_visitor = {builder.get(), alloca, this};
    auto ir_res = std::visit(store_visitor, *node.assignment);
    if (!ir_res)
      return std::unexpected(ir_res.error());

  } else {
    auto _ty = ctx->get_llvm_type(*node.type);
    if (!_ty)
      return std::unexpected("type undefined found for var definition");

    assignment_llvm_type = _ty.value();

    if (assignment_llvm_type->isVoidTy())
      return std::unexpected("trying to allocate a void type");

    auto alloca =
        builder->CreateAlloca(assignment_llvm_type, nullptr, node.name);
    defined_variables[node.name] =
        DefinedVariable{assignment_llvm_type, alloca};
  }

  return {};
}
std::expected<void, std::string>
LlvmIrGenAstVisitor::build_var_assignment(VarAssignStmtAst &node) {
  auto var = get_defined_var(node.id);
  if (!var)
    return std::unexpected("no variable found for assigment");

  LlvmStoreAllocaVisitor alloca_visitor = {builder.get(), var.value()->alloca,
                                           this};
  auto ir_res = std::visit(alloca_visitor, node.rvalue);
  if (!ir_res)
    return std::unexpected(ir_res.error());
  return {};
}
