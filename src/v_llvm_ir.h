#pragma once

#include "ast.h"
#include "helpers.h"
#include "program_ctx.h"
#include "types.h"
#include "v_expr_type.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <cstdio>
#include <ctime>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <variant>
#include <vector>

struct LlvmIrGenAstVisitor {
  ProgramCtx *ctx;
  uptr<llvm::LLVMContext> llvm_ctx;
  uptr<llvm::IRBuilder<>> builder;
  uptr<llvm::Module> module;

  struct DefinedVariable {
    AstTypeId ty_id;
    llvm::Type *type;
    llvm::Value *alloca;
  };

  struct EnumValues {
    std::vector<llvm::Value *> values;
  };

  // TODO: Pass state as an argument in the visitor. Global state is pretty bad.
  // State
  struct GenCtx {
    std::map<std::string, DefinedVariable> defined_variables;
    volatile bool rvalue_mode = false; // Volatile is needed or the compile
                                       // might optimize this for some reason.
    bool var_lassign_mode = false;
    bool get_ref_mode = false;

    std::optional<DefinedVariable *> get_defined_var(std::string name) {
      if (defined_variables.find(name) == defined_variables.end())
        return std::nullopt;
      return &defined_variables[name];
    }
  };

  LlvmIrGenAstVisitor(ProgramCtx *ctx) : ctx(ctx) {
    llvm_ctx = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("honey jit", *llvm_ctx);
    builder = std::make_unique<llvm::IRBuilder<>>(*llvm_ctx);
  }

  std::expected<void, std::string> store_in_value(GenCtx *gctx,
                                                  llvm::Value *ptr,
                                                  AstTypeId ptr_ty_id,
                                                  AstExpression &expr);

  llvm::AllocaInst *build_alloca_at_start(llvm::Function *fn, llvm::Type *type,
                                          std::string name) {
    auto tmp_builder =
        llvm::IRBuilder<>(&fn->getEntryBlock(), fn->getEntryBlock().begin());
    return tmp_builder.CreateAlloca(type, nullptr, name);
  }

  std::expected<llvm::Function *, std::string>
  build_prototype(FnHeaderAst &header) {
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

    auto fn_overloads = ctx->get_overloads(header.name);
    if (!fn_overloads)
      return std::unexpected("no overloads found for fn header");

    auto fn_id =
        std::get<1>(*fn_overloads.value()->get_fn(&ctx->type_db, &header));
    auto fn_ty = ctx->type_db.get_type(fn_id).value();
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

  std::expected<void, std::string> build_fn(GenCtx *gctx, FnDefAst &node) {
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
        if (!ctx->type_db.get_type(node.fn_header->ret_type)
                 .value()
                 ->is_void()) {
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
    return {};
  }

  std::expected<void, std::string> build_struct(StructDefAst &node) {
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

    return {};
  }

  std::expected<void, std::string> build_enum(GenCtx *gctx, EnumDefAst &node) {
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

  std::expected<void, std::string> build_var(GenCtx *gctx, VarDefStmtAst &node);

  std::expected<void, std::string> build_var_assignment(GenCtx *gctx,
                                                        VarAssignStmtAst &node);

  std::expected<void, std::string> build_return(GenCtx *gctx,
                                                ReturnStmtAst &node) {
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
  build_statement(GenCtx *gctx, AstStatement &statement) {
    if (std::holds_alternative<uptr<FnDefAst>>(statement)) {
      auto fn = build_fn(gctx, *std::get<uptr<FnDefAst>>(statement));
      if (!fn)
        return std::unexpected(fn.error());
      return nullptr;
    }

    if (std::holds_alternative<uptr<StructDefAst>>(statement)) {
      auto stc = build_struct(*std::get<uptr<StructDefAst>>(statement));
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

  std::expected<llvm::Value *, std::string> build_expr(GenCtx *gctx,
                                                       AstExpression &expr) {
    return std::visit(
        [this, gctx](auto &node) -> std::expected<llvm::Value *, std::string> {
          auto expr = visit_expr(gctx, node);
          return expr;
        },
        expr);
  }

  // Literals
  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<BoolExprAst> &node) {
    return llvm::ConstantInt::get(*llvm_ctx, llvm::APInt(1, node->value));
  }

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<IntExprAst> &node) {
    return llvm::ConstantInt::get(*llvm_ctx, llvm::APInt(32, node->value));
  }

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<FloatExprAst> &node) {
    auto fp = llvm::ConstantFP::get(*llvm_ctx, llvm::APFloat(node->value));
    return fp;
  }

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<VarExprAst> &node) {
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
  visit_expr(GenCtx *gctx, uptr<IndexExprAst> &node) {
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

    auto el_ptr = builder->CreateGEP(base_type_llvm, *base,
                                     {builder->getInt64(0), *index});
    if (gctx->rvalue_mode)
      return builder->CreateLoad(el_type_llvm, el_ptr);
    return el_ptr;
  }

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<StatementExprAst> &node) {
    return build_expr(gctx, node->expr);
  }

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<GroupExprAst> &node) {
    return build_expr(gctx, node->expr);
  }

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<RefExprAst> &node) {
    gctx->get_ref_mode = true;
    auto expr = build_expr(gctx, node->expr);
    gctx->get_ref_mode = false;
    if (!expr)
      return std::unexpected(expr.error());
    return expr;
  }

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<DerefExprAst> &node) {
    auto expr = build_expr(gctx, node->expr);
    if (!expr)
      return std::unexpected(expr.error());

    std::println("out - rvalue: {}", gctx->rvalue_mode ? "true" : "false");
    if (gctx->rvalue_mode) {
      std::println("in - rvalue: {}", gctx->rvalue_mode ? "true" : "false");
      auto expr_type = AstExprTypeVisitor::get_type(ctx, node);
      auto llvm_type = ctx->get_llvm_type(expr_type->get_id());
      std::println("type: {} - rvalue: {}", expr_type->get_name(),
                   gctx->rvalue_mode ? "true" : "false");
      expr = builder->CreateLoad(*llvm_type, *expr);
    } else {
      expr = builder->CreateLoad(llvm::PointerType::get(*llvm_ctx, 0), *expr);
    }
    return expr;
  }

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<StructExprAst> &_) {
    struct Unreachable {};
    throw Unreachable{};
  }

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<EnumExprAst> &_) {
    struct Unreachable {};
    throw Unreachable{};
  }

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<ForExprAst> &node) {

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
  visit_expr(GenCtx *gctx, uptr<SingleMatchExprAst> &node) {
    auto enum_expr = build_expr(gctx, node->enum_expr);
    if (!enum_expr)
      return std::unexpected(enum_expr.error());

    auto enum_member_expr_ty =
        ctx->type_db.get_type(node->casted_enum_var->type);
    if (!enum_member_expr_ty)
      return std::unexpected("no type found for match expression");
    auto enum_ty = enum_member_expr_ty.value()->get_parent();
    auto enum_llvm_ty = ctx->get_llvm_type(enum_ty->get_id()).value();

    auto enum_tag_ptr = builder->CreateStructGEP(enum_llvm_ty, *enum_expr, 0);
    auto enum_tag = builder->CreateLoad(
        llvm::IntegerType::getInt32Ty(*llvm_ctx), enum_tag_ptr);
    auto member_idx =
        enum_ty->get_field_index_by_id(node->casted_enum_var->type);

    // Compare tag value to check if it's the right enum
    auto match_res_expr = builder->CreateICmpEQ(
        enum_tag,
        llvm::ConstantInt::get(*llvm_ctx, llvm::APInt(32, *member_idx)),
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
      auto ref_ty = AstType::new_reference(
          enum_member_expr_ty.value()->get_id(), &ctx->type_db);
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

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<IfExprAst> &node) {
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

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<NoOpAst> &node) {
    return nullptr;
  }

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<ArrayExprAst> &node) {
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
  visit_expr(GenCtx *gctx, uptr<StringExprAst> &node) {
    auto str_const =
        llvm::ConstantDataArray::getString(*llvm_ctx, node->value, true);
    auto global_str = new llvm::GlobalVariable(
        *module, str_const->getType(), true, llvm::GlobalValue::PrivateLinkage,
        str_const, ".str");
    global_str->setAlignment(llvm::Align(1));
    return builder->CreateConstGEP2_32(str_const->getType(), global_str, 0, 0);
  }

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<MemberAccesorExprAst> &node) {
    auto base_expr = build_expr(gctx, node->base);
    if (!base_expr)
      return std::unexpected(base_expr.error());

    AstExprTypeVisitor type_visitor = {ctx};
    auto base_expr_type_id = std::visit(type_visitor, node->base);
    auto base_expr_type = ctx->type_db.get_type(base_expr_type_id).value();

    // Found a field:
    auto member_idx = base_expr_type->get_field_index_by_name(node->member);
    if (member_idx) {

      auto member_type = base_expr_type->get_field_by_idx(*member_idx).value();
      auto member_llvm_type = ctx->get_llvm_type(member_type->type);
      if (!member_llvm_type)
        return std::unexpected("no llvm type found for member accessor");

      auto base_expr_llvm_type = ctx->get_llvm_type(base_expr_type_id);
      if (!base_expr_llvm_type)
        return std::unexpected("no type found for base expr");

      auto struct_type = llvm::cast<llvm::StructType>(*base_expr_llvm_type);
      auto member_ptr = builder->CreateStructGEP(struct_type, *base_expr,
                                                 *member_idx, node->member);

      if (gctx->rvalue_mode || gctx->get_ref_mode ||
          member_llvm_type.value()->isStructTy())
        return member_ptr;

      return builder->CreateLoad(*member_llvm_type, member_ptr, node->member);
    }

    return std::unexpected(std::format("no member '{}' found for type '{}'",
                                       node->member,
                                       base_expr_type->get_name()));
  }

  // Expressions
  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<CallExprAst> &node) {
    auto callee_fn = module->getFunction(node->fn_name);

    if (!callee_fn)
      return std::unexpected("tried to call unknown fn");

    if (callee_fn->arg_size() !=
            node->prefix_args.size() + node->suffix_args.size() &&
        !callee_fn->isVarArg())
      return std::unexpected("incorrect number of arguments used.");

    std::vector<llvm::Value *> args;

    for (auto &arg : node->prefix_args) {
      auto arg_val = build_expr(gctx, arg);
      if (!arg_val)
        return std::unexpected(arg_val.error());
      args.push_back(*arg_val);
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
  visit_expr(GenCtx *gctx, uptr<MetaDefExprAst> &node) {
    auto meta_fn = ctx->get_meta(node->name);

    if (!meta_fn)
      return std::unexpected("tried to call unknown meta fn");

    if (meta_fn.value()->arg_num() != (int)node->args.size())
      return std::unexpected("incorrect number of arguments used for meta fn.");

    // TODO: As soon as additional meta fn are defined, they might have
    // different number of arguments
    auto lhs = build_expr(gctx, node->args[0]);
    if (!lhs)
      return std::unexpected("error generating lhs of int add");
    auto rhs = build_expr(gctx, node->args[1]);
    if (!rhs)
      return std::unexpected("error generating rhs of int add");

    if (meta_fn.value()->kind == MetaFunctionKind::AddInt) {
      auto add = builder->CreateAdd(*lhs, *rhs, "addi32tmp");
      return add;
    }
    if (meta_fn.value()->kind == MetaFunctionKind::SubInt)
      return builder->CreateSub(*lhs, *rhs, "subi32tmp");
    if (meta_fn.value()->kind == MetaFunctionKind::MulInt)
      return builder->CreateMul(*lhs, *rhs, "muli32tmp");
    if (meta_fn.value()->kind == MetaFunctionKind::DivInt)
      return builder->CreateSDiv(*lhs, *rhs, "divi32tmp");
    if (meta_fn.value()->kind == MetaFunctionKind::ModInt)
      return builder->CreateSRem(*lhs, *rhs, "modi32tmp");

    if (meta_fn.value()->kind == MetaFunctionKind::AddFloat)
      return builder->CreateFAdd(*lhs, *rhs, "addf32tmp");
    if (meta_fn.value()->kind == MetaFunctionKind::SubFloat)
      return builder->CreateFSub(*lhs, *rhs, "subf32tmp");
    if (meta_fn.value()->kind == MetaFunctionKind::MulFloat)
      return builder->CreateFMul(*lhs, *rhs, "mulf32tmp");
    if (meta_fn.value()->kind == MetaFunctionKind::DivFloat)
      return builder->CreateFDiv(*lhs, *rhs, "divf32tmp");
    if (meta_fn.value()->kind == MetaFunctionKind::ModFloat)
      return builder->CreateFRem(*lhs, *rhs, "modf32tmp");

    if (meta_fn.value()->kind == MetaFunctionKind::EqBool)
      return builder->CreateICmpEQ(*lhs, *rhs, "eqbtmp");
    if (meta_fn.value()->kind == MetaFunctionKind::NotEqBool)
      return builder->CreateICmpNE(*lhs, *rhs, "nebtmp");
    if (meta_fn.value()->kind == MetaFunctionKind::LtBool)
      return builder->CreateICmpSLT(*lhs, *rhs, "ltbtmp");
    if (meta_fn.value()->kind == MetaFunctionKind::GtBool)
      return builder->CreateICmpSGT(*lhs, *rhs, "gtbtmp");
    if (meta_fn.value()->kind == MetaFunctionKind::LtEqBool)
      return builder->CreateICmpSLE(*lhs, *rhs, "lteqbtmp");
    if (meta_fn.value()->kind == MetaFunctionKind::GtEqBool)
      return builder->CreateICmpSGE(*lhs, *rhs, "gteqbtmp");
    if (meta_fn.value()->kind == MetaFunctionKind::AndBool)
      return builder->CreateAnd(*lhs, *rhs, "andbtmp");
    if (meta_fn.value()->kind == MetaFunctionKind::OrBool)
      return builder->CreateOr(*lhs, *rhs, "orbtmp");

    return {};
  }

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<BodyExprAst> &node) {
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
};

struct LlvmStoreAllocaVisitor {
  llvm::IRBuilder<> *builder;
  llvm::Value *alloca;
  LlvmIrGenAstVisitor *llvm_gen;

  std::expected<void, std::string>
  build_alloca(LlvmIrGenAstVisitor::GenCtx *gctx, AstExpression &expr) {
    return std::visit(
        [this, gctx](auto &node) -> std::expected<void, std::string> {
          return visit_expr(gctx, node);
        },
        expr);
  }

  template <class T>
  std::expected<void, std::string>
  simple_alloca(LlvmIrGenAstVisitor::GenCtx *gctx, uptr<T> &node) {
    /* std::println("rvalue start mode: {}", llvm_gen->rvalue_mode); */
    gctx->rvalue_mode = true;
    auto expr = llvm_gen->visit_expr(gctx, node);
    gctx->rvalue_mode = false;

    if (!expr)
      return std::unexpected(expr.error());
    builder->CreateStore(*expr, alloca);
    return {};
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<IntExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<IndexExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<ArrayExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<FloatExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<StringExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<BoolExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<VarExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<CallExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<BodyExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<StatementExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<MetaDefExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<GroupExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<IfExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<NoOpAst> &node) {}

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<SingleMatchExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<ForExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<RefExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<DerefExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string>
  visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
             uptr<MemberAccesorExprAst> &node) {
    return simple_alloca(gctx, node);
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<EnumExprAst> &node) {
    auto enum_llvm_ty = llvm_gen->ctx->get_llvm_type(node->enum_type);
    if (!enum_llvm_ty)
      return std::unexpected("failed to get struct type");

    auto enum_type = llvm_gen->ctx->type_db.get_type(node->enum_type).value();

    // Get member position from enum
    auto value_idx = enum_type->get_field_index_by_id(node->struct_expr->type);
    if (!value_idx)
      return std::unexpected(std::format("'{}' not found in enum '{}'",
                                         node->struct_expr->type,
                                         enum_type->get_name()));

    // Index is always the first value in an enum
    auto field_ptr = builder->CreateStructGEP(*enum_llvm_ty, alloca, 0,
                                              enum_type->get_name() + "_tag");

    // Store the tag value
    auto int_expr = AstExpression{std::make_unique<IntExprAst>(*value_idx)};
    auto ir_res =
        llvm_gen->store_in_value(gctx, field_ptr, node->enum_type, int_expr);
    if (!ir_res)
      return std::unexpected(ir_res.error());

    auto bitcast_enum =
        builder->CreateBitCast(alloca, enum_llvm_ty.value()->getPointerTo());
    auto bitcast_union_ptr =
        builder->CreateStructGEP(*enum_llvm_ty, bitcast_enum, 1);

    auto ref_ty =
        AstType::new_reference(node->struct_expr->type, &llvm_gen->ctx->type_db)
            .get_id();
    auto struct_expr = AstExpression{
        std::move(node->struct_expr)}; // BUG: This will be problematic if the
                                       // AST it reused
    ir_res =
        llvm_gen->store_in_value(gctx, bitcast_union_ptr, ref_ty, struct_expr);
    if (!ir_res)
      return std::unexpected(ir_res.error());

    return {};
  }

  std::expected<void, std::string> visit_expr(LlvmIrGenAstVisitor::GenCtx *gctx,
                                              uptr<StructExprAst> &node) {
    auto struct_ty = llvm_gen->ctx->get_llvm_type(node->type);
    if (!struct_ty)
      return std::unexpected("failed to get struct type");

    for (auto &field : node->fields) {
      auto field_idx = llvm_gen->ctx->type_db.get_type(node->type)
                           .value()
                           ->get_field_index_by_name(field->name);
      if (!field_idx)
        return std::unexpected(field_idx.error());
      auto field_ty = llvm_gen->ctx->type_db.get_type(node->type)
                          .value()
                          ->get_field_by_idx(*field_idx)
                          .value();

      auto field_ptr =
          builder->CreateStructGEP(*struct_ty, alloca, *field_idx, field->name);
      auto ir_res = llvm_gen->store_in_value(gctx, field_ptr, field_ty->type,
                                             field->rvalue);
      if (!ir_res)
        return std::unexpected(ir_res.error());
    }

    return {};
  }
};
