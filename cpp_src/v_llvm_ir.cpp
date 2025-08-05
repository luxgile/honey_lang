#include "v_llvm_ir.h"
#include "ast.h"
#include "types.h"
#include "v_expr_type.h"
#include "llvm/IR/Instruction.h"
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

std::expected<llvm::Value *, std::string>
MetaBinOp::gen_ir(LlvmIrGenAstVisitor *llvm, LlvmIrGenAstVisitor::GenCtx *gctx,
                  MetaDefExprAst *node) {
  LlvmIrGenAstVisitor::GenCtx gctx_temp = *gctx;
  gctx_temp.rvalue_mode = false;
  auto lhs = llvm->build_expr(&gctx_temp, node->args[0]);
  if (!lhs)
    return std::unexpected("error generating lhs of int add");
  auto rhs = llvm->build_expr(&gctx_temp, node->args[1]);
  if (!rhs)
    return std::unexpected("error generating rhs of int add");

  auto bin_op = llvm->builder->CreateBinOp(op, *lhs, *rhs);
  return bin_op;
}

std::expected<llvm::Value *, std::string>
MetaCmpOp::gen_ir(LlvmIrGenAstVisitor *llvm, LlvmIrGenAstVisitor::GenCtx *gctx,
                  MetaDefExprAst *node) {
  LlvmIrGenAstVisitor::GenCtx gctx_temp = *gctx;
  gctx_temp.rvalue_mode = false;
  auto lhs = llvm->build_expr(&gctx_temp, node->args[0]);
  if (!lhs)
    return std::unexpected("error generating lhs of int add");
  auto rhs = llvm->build_expr(&gctx_temp, node->args[1]);
  if (!rhs)
    return std::unexpected("error generating rhs of int add");

  auto bin_op = llvm->builder->CreateCmp(op, *lhs, *rhs);
  return bin_op;
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<MetaDefExprAst> &node) {
  auto meta_fn = ctx->get_meta(node->name);

  if (!meta_fn)
    return std::unexpected("tried to call unknown meta fn");

  if (meta_fn.value()->arg_num() != (int)node->args.size())
    return std::unexpected("incorrect number of arguments used for meta fn.");

  return meta_fn.value()->gen_ir(this, gctx, node.get());
}

AstTypeId MetaBinOp::get_type_id(ProgramCtx *ctx) const {
  using BinOp = llvm::Instruction::BinaryOps;
  switch (op) {
  case BinOp::Add:
  case BinOp::Sub:
  case BinOp::Mul:
  case BinOp::SDiv:
  case BinOp::SRem:
    return INT_TYPE.get_id();

  case BinOp::FAdd:
  case BinOp::FSub:
  case BinOp::FMul:
  case BinOp::FDiv:
  case BinOp::FRem:
    return FLOAT_TYPE.get_id();

  case BinOp::And:
  case BinOp::Or:
    return BOOL_TYPE.get_id();

  default:
    throw std::unexpected("couldn't get type id for meta bin op");
  }
}

AstTypeId MetaCmpOp::get_type_id(ProgramCtx *ctx) const {
  return BOOL_TYPE.get_id();
}
llvm::AllocaInst *LlvmIrGenAstVisitor::build_alloca_at_start(llvm::Function *fn,
                                                             llvm::Type *type,
                                                             std::string name) {
  auto tmp_builder =
      llvm::IRBuilder<>(&fn->getEntryBlock(), fn->getEntryBlock().begin());
  return tmp_builder.CreateAlloca(type, nullptr, name);
}

std::expected<llvm::Function *, std::string>
LlvmIrGenAstVisitor::build_prototype(FnHeaderAst &header) {
  auto args_types = std::vector<llvm::Type *>{};

  for (auto &arg : header.prefix_args) {
    auto type = ctx->get_llvm_type(arg->type);

    if (!type)
      return std::unexpected("no prefix found");

    args_types.push_back(*type);
  }

  for (auto &arg : header.suffix_args) {
    auto type = ctx->get_llvm_type(arg->type);

    if (!type)
      return std::unexpected("no suffix found");

    args_types.push_back(*type);
  }

  // Varadic arguments are internally stored as 'Void' but llvm generator
  // automatically adds it as another argument, so we need to drop the last
  // one.
  if (header.is_vararic()) {
    args_types.pop_back();
  }

  llvm::Type *ret_type = llvm::Type::getVoidTy(*llvm_ctx);
  auto explicit_ret_type = ctx->get_llvm_type(header.ret_type);
  if (explicit_ret_type)
    ret_type = *explicit_ret_type;

  auto fn_type =
      llvm::FunctionType::get(ret_type, args_types, header.is_vararic());

  auto fn_id =
      ctx->type_db.get_fn_by_args(header.name, header.get_prefix_named_ids(),
                                  header.get_suffix_named_ids());
  if (!fn_id)
    return std::unexpected("no fn id found for fn header");

  auto fn_ty = ctx->type_db.get_type(*fn_id).value();
  auto fn = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage,
                                   fn_ty->get_fullname(), module.get());

  int i = 0;
  for (auto &arg : header.prefix_args) {
    fn->getArg(i)->setName(arg->name);
    i += 1;
  }
  for (auto &arg : header.suffix_args) {
    // Skip last argument if it's varadic
    if (fn->isVarArg() && fn->arg_size() - 1 == (size_t)i)
      break;

    fn->getArg(i)->setName(arg->name);
    i += 1;
  }

  return fn;
}

std::expected<void, std::string> LlvmIrGenAstVisitor::build_fn(GenCtx *gctx,
                                                               FnDefAst &node) {
  auto fn_header = build_prototype(*node.fn_header);
  if (!fn_header)
    return std::unexpected("error creating fn header");

  if (!fn_header.value()->empty())
    return std::unexpected("trying to redefine an existing fn");

  if (!node.fn_header->is_external) {
    gctx->defined_variables.clear();
    auto bb = llvm::BasicBlock::Create(*llvm_ctx, "entry", fn_header.value());
    builder->SetInsertPoint(bb);

    // Generate arguments at the beggining of the function
    int i = 0;
    for (auto &arg : fn_header.value()->args()) {
      auto arg_ty_id = node.fn_header->get_arg_linear(i)->type;
      /* auto arg_ty = ctx->type_db.get_type(arg_ty_id).value(); */
      llvm::Value *arg_val = &arg;
      arg_val = build_alloca_at_start(fn_header.value(), arg.getType(),
                                      arg.getName().str());
      builder->CreateStore(&arg, arg_val);
      gctx->defined_variables[std::string(arg.getName())] =
          DefinedVariable{arg_ty_id, arg.getType(), arg_val};
      i += 1;
    }

    auto body_ret = build_expr(gctx, *node.body);
    bb = builder->GetInsertBlock();

    if (!body_ret) {
      fn_header.value()->eraseFromParent();
      return std::unexpected(body_ret.error());
    }

    // Check if the fn already generated a return statement
    if (bb->getTerminator() == nullptr) {
      if (!ctx->type_db.get_type(node.fn_header->ret_type).value()->is_void()) {
        auto expected_ret_type = ctx->get_llvm_type(node.fn_header->ret_type);
        if (!expected_ret_type)
          return std::unexpected("undefined return type");

        if (*expected_ret_type != body_ret.value()->getType())
          return std::unexpected(
              "trying to return a value different than specified");

        builder->CreateRet(*body_ret);
      } else {
        builder->CreateRetVoid();
      }
    }
  } else {
    fn_header.value()->setCallingConv(llvm::CallingConv::C);
  }

  llvm::verifyFunction(*fn_header.value());
  if (should_optimize)
    fpm->run(*fn_header.value(), *fam);
  return {};
}

std::expected<void, std::string>
LlvmIrGenAstVisitor::build_struct(GenCtx *gctx, StructDefAst &node) {
  std::vector<llvm::Type *> field_types;
  for (auto &field : node.fields) {
    auto type = ctx->get_llvm_type(field->type);
    if (!type)
      return std::unexpected("undefined type in struct");
    field_types.push_back(*type);
  }

  auto struct_name = ctx->type_db.get_type(node.type).value()->get_fullname();
  auto struct_type =
      llvm::StructType::create(*llvm_ctx, field_types, struct_name);
  ctx->define_llvm_type(node.type, struct_type);

  for (auto &method : node.methods) {
    auto fn = build_fn(gctx, *method.get());
    if (!fn)
      return std::unexpected(fn.error());
  }

  return {};
}

std::expected<void, std::string>
LlvmIrGenAstVisitor::build_enum(GenCtx *gctx, EnumDefAst &node) {
  // Generate inner structs members
  for (auto &stmt : node.values) {
    auto stmt_res = build_statement(gctx, stmt);
    if (!stmt_res)
      return std::unexpected(stmt_res.error());
    if (*stmt_res != nullptr)
      std::println("ignored llmv value generated while creating an enum");
  }

  auto node_type = ctx->type_db.get_type(node.type).value();

  // Get the biggest member size
  auto data_layout = llvm::DataLayout{module.get()};
  uint union_size = 0;
  for (auto &field : node_type->get_fields()) {
    auto llvm_type = ctx->get_llvm_type(field.type);
    if (!llvm_type)
      return std::unexpected(
          std::format("no llvm type '{}' found",
                      ctx->type_db.get_type(field.type).value()->get_name()));
    auto size = data_layout.getTypeAllocSize(*llvm_type);
    if (size > union_size)
      union_size = size;
  }

  // Create the llvm type
  std::vector<llvm::Type *> field_types;
  field_types.push_back(llvm::IntegerType::getInt32Ty(*llvm_ctx));
  field_types.push_back(
      llvm::ArrayType::get(llvm::IntegerType::get(*llvm_ctx, 8), union_size));
  auto enum_type = llvm::StructType::create(*llvm_ctx, field_types,
                                            node_type->get_fullname());
  ctx->define_llvm_type(node.type, enum_type);
  return {};
}

std::expected<void, std::string>
LlvmIrGenAstVisitor::build_return(GenCtx *gctx, ReturnStmtAst &node) {
  if (node.expr) {
    auto expr = build_expr(gctx, *node.expr);
    if (!expr)
      return std::unexpected(expr.error());
    builder->CreateRet(*expr);
  } else {
    builder->CreateRetVoid();
  }

  return {};
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::build_statement(GenCtx *gctx, AstStatement &statement) {
  if (std::holds_alternative<uptr<FnDefAst>>(statement)) {
    auto fn = build_fn(gctx, *std::get<uptr<FnDefAst>>(statement));
    if (!fn)
      return std::unexpected(fn.error());
    return nullptr;
  }

  if (std::holds_alternative<uptr<StructDefAst>>(statement)) {
    auto stc = build_struct(gctx, *std::get<uptr<StructDefAst>>(statement));
    if (!stc)
      return std::unexpected(stc.error());
    return nullptr;
  }

  if (std::holds_alternative<uptr<EnumDefAst>>(statement)) {
    auto stc = build_enum(gctx, *std::get<uptr<EnumDefAst>>(statement));
    if (!stc)
      return std::unexpected(stc.error());
    return nullptr;
  }

  if (std::holds_alternative<uptr<VarDefStmtAst>>(statement)) {
    auto var = build_var(gctx, *std::get<uptr<VarDefStmtAst>>(statement));
    if (!var)
      return std::unexpected(var.error());
    return nullptr;
  }

  if (std::holds_alternative<uptr<VarAssignStmtAst>>(statement)) {
    auto var = build_var_assignment(
        gctx, *std::get<uptr<VarAssignStmtAst>>(statement));
    if (!var)
      return std::unexpected(var.error());
    return nullptr;
  }

  if (std::holds_alternative<uptr<ReturnStmtAst>>(statement)) {
    auto ret = build_return(gctx, *std::get<uptr<ReturnStmtAst>>(statement));
    if (!ret)
      return std::unexpected(ret.error());
    return nullptr;
  }

  if (std::holds_alternative<uptr<StatementExprAst>>(statement)) {
    auto res =
        build_expr(gctx, std::get<uptr<StatementExprAst>>(statement)->expr);
    if (!res)
      return std::unexpected(res.error());
    return *res;
  }

  return std::unexpected("no statement found");
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::build_expr(GenCtx *gctx, AstExpression &expr) {
  return std::visit(
      [this, gctx](auto &node) -> std::expected<llvm::Value *, std::string> {
        auto expr = visit_expr(gctx, node);
        return expr;
      },
      expr);
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<BoolExprAst> &node) {
  return llvm::ConstantInt::get(*llvm_ctx, llvm::APInt(1, node->value));
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<IntExprAst> &node) {
  return llvm::ConstantInt::get(*llvm_ctx, llvm::APInt(32, node->value));
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<FloatExprAst> &node) {
  auto fp = llvm::ConstantFP::get(*llvm_ctx, llvm::APFloat(node->value));
  return fp;
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<VarExprAst> &node) {
  auto var = gctx->get_defined_var(node->name);
  if (!var)
    return std::unexpected(
        std::format("variable '{}' is undefined", node->name));

  auto var_type = AstExprTypeVisitor::get_type(ctx, node);
  if (gctx->get_ref_mode || gctx->var_lassign_mode)
    return var.value()->alloca;

  if (!gctx->rvalue_mode && !var_type->is_primitive() && !var_type->is_ref())
    return var.value()->alloca;

  return builder->CreateLoad(var.value()->type, var.value()->alloca,
                             node->name);
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<IndexExprAst> &node) {
  GenCtx _gctx = *gctx;
  _gctx.rvalue_mode =
      false; // Make sure we get the array pointer rather than the value
  auto base = build_expr(&_gctx, node->base);
  if (!base)
    return std::unexpected(base.error());

  auto base_type = AstExprTypeVisitor::get_type_id(ctx, node->base);
  auto base_type_llvm = ctx->get_llvm_type(base_type).value();

  auto el_type = AstExprTypeVisitor::get_type_id(ctx, node);
  auto el_type_llvm = ctx->get_llvm_type(el_type).value();

  auto index = build_expr(gctx, node->index);
  if (!index)
    return std::unexpected(index.error());

  auto el_ptr =
      builder->CreateGEP(base_type_llvm, *base, {builder->getInt64(0), *index});
  if (gctx->rvalue_mode)
    return builder->CreateLoad(el_type_llvm, el_ptr);
  return el_ptr;
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<StatementExprAst> &node) {
  return build_expr(gctx, node->expr);
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<GroupExprAst> &node) {
  return build_expr(gctx, node->expr);
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<RefExprAst> &node) {
  gctx->get_ref_mode = true;
  auto expr = build_expr(gctx, node->expr);
  gctx->get_ref_mode = false;
  if (!expr)
    return std::unexpected(expr.error());
  return expr;
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<DerefExprAst> &node) {
  auto expr = build_expr(gctx, node->expr);
  if (!expr)
    return std::unexpected(expr.error());

  if (gctx->var_lassign_mode) {
    expr = builder->CreateLoad(llvm::PointerType::get(*llvm_ctx, 0), *expr);
  } else {
    auto expr_type = AstExprTypeVisitor::get_type(ctx, node);
    auto llvm_type = ctx->get_llvm_type(expr_type->get_id());
    expr = builder->CreateLoad(*llvm_type, *expr);
  }
  return expr;
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<StructExprAst> &_) {
  struct Unreachable {};
  throw Unreachable{};
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<EnumExprAst> &_) {
  struct Unreachable {};
  throw Unreachable{};
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<ForExprAst> &node) {

  auto fn = builder->GetInsertBlock()->getParent();
  auto for_check_bb = llvm::BasicBlock::Create(*llvm_ctx, "for_check", fn);
  auto for_body_bb = llvm::BasicBlock::Create(*llvm_ctx, "for_body", fn);
  auto for_merge_bb = llvm::BasicBlock::Create(*llvm_ctx, "for_merge", fn);

  builder->CreateBr(for_check_bb);

  // Emit for check block
  builder->SetInsertPoint(for_check_bb);
  auto for_cond_expr = build_expr(gctx, node->condition);
  if (!for_cond_expr)
    return std::unexpected(for_cond_expr.error());

  for_cond_expr = builder->CreateICmpEQ(
      *for_cond_expr, llvm::ConstantInt::get(*llvm_ctx, llvm::APInt(1, 1)),
      "for_cond");
  builder->CreateCondBr(*for_cond_expr, for_body_bb, for_merge_bb);

  // Emit for body block
  builder->SetInsertPoint(for_body_bb);
  auto for_body_expr = build_expr(gctx, node->for_body);
  if (!for_cond_expr)
    return std::unexpected(for_body_expr.error());

  builder->CreateBr(for_check_bb);

  // Emit merge block
  builder->SetInsertPoint(for_merge_bb);

  return for_cond_expr;
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<SingleMatchExprAst> &node) {
  auto enum_expr = build_expr(gctx, node->enum_expr);
  if (!enum_expr)
    return std::unexpected(enum_expr.error());

  auto enum_member_expr_ty = ctx->type_db.get_type(node->casted_enum_var->type);
  if (!enum_member_expr_ty)
    return std::unexpected("no type found for match expression");
  auto enum_ty = enum_member_expr_ty.value()->get_parent();
  auto enum_llvm_ty = ctx->get_llvm_type(enum_ty->get_id()).value();

  auto enum_tag_ptr = builder->CreateStructGEP(enum_llvm_ty, *enum_expr, 0);
  auto enum_tag = builder->CreateLoad(llvm::IntegerType::getInt32Ty(*llvm_ctx),
                                      enum_tag_ptr);
  auto member_idx = enum_ty->get_field_index_by_id(node->casted_enum_var->type);

  // Compare tag value to check if it's the right enum
  auto match_res_expr = builder->CreateICmpEQ(
      enum_tag, llvm::ConstantInt::get(*llvm_ctx, llvm::APInt(32, *member_idx)),
      "match_cond");

  auto fn = builder->GetInsertBlock()->getParent();
  auto then_bb = llvm::BasicBlock::Create(*llvm_ctx, "match_then", fn);
  auto merge_bb = llvm::BasicBlock::Create(*llvm_ctx, "match_merge", fn);
  builder->CreateCondBr(match_res_expr, then_bb, merge_bb);

  // Emit then block

  // If variable has a name, create new var and cast the value to it
  builder->SetInsertPoint(then_bb);

  if (node->casted_enum_var->name != "") {
    auto enum_member_expr_llvm_ty =
        ctx->get_llvm_type(enum_member_expr_ty.value()->get_id()).value();
    auto enum_member_ptr =
        builder->CreateStructGEP(enum_llvm_ty, *enum_expr, 1);
    auto member_llvm_ptr_ty = enum_member_expr_llvm_ty->getPointerTo();
    auto casted_enum_member =
        builder->CreateBitCast(enum_member_ptr, member_llvm_ptr_ty);
    auto ref_ty = AstType::new_reference(enum_member_expr_ty.value()->get_id(),
                                         &ctx->type_db);
    gctx->defined_variables[node->casted_enum_var->name] = DefinedVariable{
        ref_ty.get_id(), member_llvm_ptr_ty, casted_enum_member};
  }

  auto then_expr = build_expr(gctx, node->then_expr);
  if (!then_expr)
    return std::unexpected(then_expr.error());

  builder->CreateBr(merge_bb);
  then_bb = builder->GetInsertBlock();

  builder->SetInsertPoint(merge_bb);

  return then_expr;
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<IfExprAst> &node) {
  auto cond_expr = build_expr(gctx, node->condition);
  if (!cond_expr)
    return std::unexpected(cond_expr.error());

  cond_expr = builder->CreateICmpEQ(
      *cond_expr, llvm::ConstantInt::get(*llvm_ctx, llvm::APInt(1, 1)),
      "if_cond");

  auto fn = builder->GetInsertBlock()->getParent();
  auto then_bb = llvm::BasicBlock::Create(*llvm_ctx, "if_then", fn);
  llvm::BasicBlock *else_bb = nullptr;
  if (node->else_expr)
    else_bb = llvm::BasicBlock::Create(*llvm_ctx, "if_else", fn);
  auto merge_bb = llvm::BasicBlock::Create(*llvm_ctx, "if_merge", fn);

  if (node->else_expr)
    builder->CreateCondBr(*cond_expr, then_bb, else_bb);
  else
    builder->CreateCondBr(*cond_expr, then_bb, merge_bb);

  // Emit then block
  builder->SetInsertPoint(then_bb);
  auto then_expr = build_expr(gctx, node->then_expr);
  if (!then_expr)
    return std::unexpected(then_expr.error());

  then_bb = builder->GetInsertBlock();
  if (then_bb->getTerminator() == nullptr)
    builder->CreateBr(merge_bb);
  then_bb = builder->GetInsertBlock();

  // Emit else block
  std::expected<llvm::Value *, std::string> else_expr = nullptr;
  if (node->else_expr) {
    // fn->insert(fn->end(), else_bb);
    builder->SetInsertPoint(else_bb);

    else_expr = build_expr(gctx, *node->else_expr);
    if (!else_expr)
      return std::unexpected(else_expr.error());

    else_bb = builder->GetInsertBlock();
    if (else_bb->getTerminator() == nullptr)
      builder->CreateBr(merge_bb);

    else_bb = builder->GetInsertBlock();
  }

  builder->SetInsertPoint(merge_bb);

  if (node->else_expr) {
    auto ty = then_expr.value()->getType();
    auto phi = builder->CreatePHI(ty, 2, "iftmp");
    phi->addIncoming(*then_expr, then_bb);
    phi->addIncoming(*else_expr, else_bb);
    return phi;
  }

  // An if expression that does not have an else counterpart, cannot return a
  // value as it would be undefined.
  return then_expr;
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<NoOpAst> &node) {
  return nullptr;
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<ArrayExprAst> &node) {
  auto array_type = ctx->type_db.get_type(node->type).value();
  auto llvm_element_type =
      ctx->get_llvm_type(array_type->get_subtype()).value();
  auto llvm_array_type =
      llvm::ArrayType::get(llvm_element_type, array_type->get_array_size());
  auto array = builder->CreateAlloca(llvm_array_type, nullptr);

  // Populate array with elements.
  auto zero = builder->getInt64(0);
  for (int i = 0; i < (int)node->elements.size(); i += 1) {
    auto el_val = build_expr(gctx, node->elements[i]);
    if (!el_val)
      return std::unexpected(el_val.error());
    auto idx = builder->getInt64(i);
    auto el_ptr = builder->CreateGEP(llvm_array_type, array, {zero, idx});
    builder->CreateStore(*el_val, el_ptr);
  }

  return builder->CreateLoad(llvm_array_type, array);
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<StringExprAst> &node) {
  auto str_const =
      llvm::ConstantDataArray::getString(*llvm_ctx, node->value, true);
  auto global_str = new llvm::GlobalVariable(
      *module, str_const->getType(), true, llvm::GlobalValue::PrivateLinkage,
      str_const, ".str");
  global_str->setAlignment(llvm::Align(1));
  return builder->CreateConstGEP2_32(str_const->getType(), global_str, 0, 0);
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx,
                                uptr<MemberAccesorExprAst> &node) {
  GenCtx _gctx = *gctx;
  _gctx.var_lassign_mode = false;
  auto base_expr = build_expr(&_gctx, node->base);
  if (!base_expr)
    return std::unexpected(base_expr.error());

  AstExprTypeVisitor type_visitor = {ctx};
  auto base_expr_type_id = std::visit(type_visitor, node->base);
  auto base_expr_type = ctx->type_db.get_type(base_expr_type_id).value();
  if (base_expr_type->is_ref()) {
    base_expr_type_id = base_expr_type->get_subtype();
    base_expr_type =
        ctx->type_db.get_type(base_expr_type->get_subtype()).value();
  }

  // Found a field:
  if (node->field) {
    auto member_idx =
        base_expr_type->get_field_index_by_name(node->field.value()->name);
    auto member_type = base_expr_type->get_field_by_idx(*member_idx).value();
    auto member_llvm_type = ctx->get_llvm_type(member_type->type);
    if (!member_llvm_type)
      return std::unexpected("no llvm type found for member accessor");

    auto base_expr_llvm_type = ctx->get_llvm_type(base_expr_type_id);
    if (!base_expr_llvm_type)
      return std::unexpected("no type found for base expr");

    auto struct_type = llvm::cast<llvm::StructType>(*base_expr_llvm_type);
    auto member_ptr = builder->CreateStructGEP(
        struct_type, *base_expr, *member_idx, node->field.value()->name);

    if (gctx->rvalue_mode || gctx->get_ref_mode || gctx->var_lassign_mode ||
        member_llvm_type.value()->isStructTy())
      return member_ptr;

    return builder->CreateLoad(*member_llvm_type, member_ptr,
                               node->field.value()->name);
  }

  // Found a method:
  if (node->method) {
    auto ref_expr = std::make_unique<RefExprAst>(std::move(node->base));
    auto base_ref_val = visit_expr(gctx, ref_expr);
    if (!base_ref_val)
      return std::unexpected(base_ref_val.error());
    gctx->method_parent = *base_ref_val;
    return visit_expr(gctx, *node->method);
  }

  return std::unexpected(
      std::format("no member found for type '{}'", base_expr_type->get_name()));
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<CallExprAst> &node) {
  auto fn_ty = ctx->type_db.get_type(node->fn_id).value();

  auto callee_fn = module->getFunction(fn_ty->get_fullname());

  if (!callee_fn)
    return std::unexpected("tried to call unknown fn");

  auto arg_number = node->prefix_args.size() + node->suffix_args.size();
  if (gctx->method_parent)
    arg_number += 1;

  if (callee_fn->arg_size() != arg_number && !callee_fn->isVarArg())
    return std::unexpected("incorrect number of arguments used.");

  std::vector<llvm::Value *> args;

  for (auto &arg : node->prefix_args) {
    auto arg_val = build_expr(gctx, arg);
    if (!arg_val)
      return std::unexpected(arg_val.error());
    args.push_back(*arg_val);
  }

  if (gctx->method_parent) {
    args.push_back(*gctx->method_parent);
    gctx->method_parent.reset();
  }

  for (auto &arg : node->suffix_args) {
    auto arg_val = build_expr(gctx, arg);
    if (!arg_val)
      return std::unexpected(arg_val.error());
    args.push_back(*arg_val);
  }

  return builder->CreateCall(callee_fn, args, "calltmp");
}

std::expected<llvm::Value *, std::string>
LlvmIrGenAstVisitor::visit_expr(GenCtx *gctx, uptr<BodyExprAst> &node) {
  llvm::Value *last_val = nullptr;
  for (auto &stmt : node->statements) {
    auto stmt_expr = build_statement(gctx, stmt);
    if (!stmt_expr)
      return std::unexpected(stmt_expr.error());

    if (*stmt_expr != nullptr)
      last_val = *stmt_expr;
  }

  return last_val;
}

std::expected<void, std::string>
LlvmStoreAllocaVisitor::build_alloca(LlvmIrGenAstVisitor::GenCtx *gctx,
                                     AstExpression &expr) {
  return std::visit(
      [this, gctx](auto &node) -> std::expected<void, std::string> {
        return visit_expr(gctx, node);
      },
      expr);
}

