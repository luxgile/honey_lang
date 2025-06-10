#include "v_llvm_ir.h"
#include "v_expr_type.h"
#include <variant>

std::expected<void, std::string>
LlvmIrGenAstVisitor::build_var(VarDefStmtAst &node) {
  if (!node.assignment && !node.type)
    return std::unexpected("could not deduce type for var definition");

  llvm::Type *var_type;

  if (node.assignment) {
    AstExprTypeVisitor type_visitor = {&ctx};
    auto ast_type = std::visit(type_visitor, *node.assignment);
    var_type = ctx.get_type(ast_type).value();

    if (node.type) {
      auto _ty = ctx.get_type(*node.type);
      if (_ty && var_type != _ty.value())
        return std::unexpected(
            "explicit type and assigment expression type mismatch");
    }

    if (var_type->isVoidTy())
      return std::unexpected("trying to allocate a void type");

    auto alloca = builder->CreateAlloca(var_type, nullptr, node.name);
    defined_variables[node.name] = DefinedVariable{var_type, alloca};

    LlvmStoreAllocaVisitor store_visitor = {builder.get(), alloca, this};
    auto ir_res = std::visit(store_visitor, *node.assignment);
    if (!ir_res)
      return std::unexpected(ir_res.error());

  } else {
    auto _ty = ctx.get_type(*node.type);
    if (!_ty)
      return std::unexpected("type undefined found for var definition");

    var_type = _ty.value();

    if (var_type->isVoidTy())
      return std::unexpected("trying to allocate a void type");

    auto alloca = builder->CreateAlloca(var_type, nullptr, node.name);
    defined_variables[node.name] = DefinedVariable{var_type, alloca};
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
