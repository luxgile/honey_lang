#pragma once

#include "ast.h"
#include "helpers.h"
#include "program_ctx.h"
#include "v_expr_type.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
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
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <ctime>
#include <expected>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <variant>
#include <vector>

struct LlvmIrGenAstVisitor {
  ProgramCtx &ctx;
  uptr<llvm::LLVMContext> llvm_ctx;
  uptr<llvm::IRBuilder<>> builder;
  uptr<llvm::Module> module;

  struct DefinedVariable {
    llvm::Type *type;
    llvm::AllocaInst *alloca;
  };

  // State
  std::map<std::string, DefinedVariable> defined_variables;
  llvm::Function *current_fn;

  LlvmIrGenAstVisitor(ProgramCtx &ctx) : ctx(ctx) {
    llvm_ctx = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("honey jit", *llvm_ctx);
    builder = std::make_unique<llvm::IRBuilder<>>(*llvm_ctx);
  }

  std::optional<DefinedVariable *> get_defined_var(std::string name) {
    if (defined_variables.find(name) == defined_variables.end())
      return std::nullopt;
    return &defined_variables[name];
  }

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
      auto type = ctx.get_type(arg->type);

      if (!type)
        return std::unexpected("no prefix found");

      args_types.push_back(*type);
    }

    for (auto &arg : header.suffix_args) {
      auto type = ctx.get_type(arg->type);

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
    auto explicit_ret_type = ctx.get_type(header.ret_type);
    if (explicit_ret_type)
      ret_type = *explicit_ret_type;

    auto fn_type =
        llvm::FunctionType::get(ret_type, args_types, header.is_vararic());

    auto fn_overloads = ctx.get_overloads(header.name);
    if (!fn_overloads)
      return std::unexpected("no overloads found for fn header");

    auto mangled_name = fn_overloads.value()->get_mangled_name(&header);
    if (!mangled_name)
      return std::unexpected("unexpected issue generating mangled name");

    auto fn = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage,
                                     *mangled_name, module.get());

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

  std::expected<void, std::string> build_fn(FnDefAst &node) {
    auto fn_header = build_prototype(*node.fn_header);
    if (!fn_header)
      return std::unexpected("error creating fn header");

    if (!fn_header.value()->empty())
      return std::unexpected("trying to redefine an existing fn");

    if (!node.fn_header->is_external) {
      defined_variables.clear();
      current_fn = fn_header.value();
      auto bb = llvm::BasicBlock::Create(*llvm_ctx, "entry", fn_header.value());
      builder->SetInsertPoint(bb);
      auto body_ret = std::visit(*this, *node.body);
      bb = builder->GetInsertBlock();
      current_fn = nullptr;

      if (!body_ret) {
        fn_header.value()->eraseFromParent();
        return std::unexpected(body_ret.error());
      }

      if (!node.fn_header->ret_type.is_void()) {
        auto expected_ret_type = ctx.get_type(node.fn_header->ret_type);
        if (!expected_ret_type)
          return std::unexpected("undefined return type");

        if (*expected_ret_type != body_ret.value()->getType())
          return std::unexpected(
              "trying to return a value different than specified");

        builder->CreateRet(*body_ret);
      } else {
        builder->CreateRetVoid();
      }

    } else {
      fn_header.value()->setCallingConv(llvm::CallingConv::C);
    }

    llvm::verifyFunction(*fn_header.value());
    return {};
  }

  std::expected<void, std::string> build_var(VarDefStmtAst &node) {
    if (!node.assignment && !node.type)
      return std::unexpected("could not deduce type for var definition");

    llvm::Type *var_type;

    if (node.assignment) {
      auto expr = std::visit(*this, *node.assignment);
      var_type = expr.value()->getType();

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
      builder->CreateStore(*expr, alloca);
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
  build_var_assignment(VarAssignStmtAst &node) {

    auto var = get_defined_var(node.id);
    if (!var)
      return std::unexpected("no variable found for assigment");

    auto expr = std::visit(*this, node.rvalue);
    builder->CreateStore(*expr, var.value()->alloca);
    return {};
  }

  std::expected<void, std::string> build_external_fns() {
    for (auto ext_fn : ctx.get_all_ext_fn()) {
      auto r = build_prototype(*ext_fn);
      if (!r)
        return std::unexpected(r.error());
    }
    return {};
  }

  std::expected<llvm::Value *, std::string>
  build_statement(AstStatement &statement) {
    if (std::holds_alternative<uptr<FnDefAst>>(statement)) {
      auto fn = build_fn(*std::get<uptr<FnDefAst>>(statement));
      if (!fn)
        return std::unexpected(fn.error());
      return nullptr;
    }

    if (std::holds_alternative<uptr<VarDefStmtAst>>(statement)) {
      auto var = build_var(*std::get<uptr<VarDefStmtAst>>(statement));
      if (!var)
        return std::unexpected(var.error());
      return nullptr;
    }

    if (std::holds_alternative<uptr<VarAssignStmtAst>>(statement)) {
      auto var =
          build_var_assignment(*std::get<uptr<VarAssignStmtAst>>(statement));
      if (!var)
        return std::unexpected(var.error());
      return nullptr;
    }

    if (std::holds_alternative<uptr<StatementExprAst>>(statement)) {
      auto res = (*this)(std::get<uptr<StatementExprAst>>(statement));
      if (!res)
        return std::unexpected(res.error());
      return *res;
    }

    return std::unexpected("no statement found");
  }

  // Literals
  std::expected<llvm::Value *, std::string>
  operator()(uptr<BoolExprAst> &node) {
    return llvm::ConstantInt::get(*llvm_ctx, llvm::APInt(1, node->value));
  }

  std::expected<llvm::Value *, std::string> operator()(uptr<IntExprAst> &node) {
    return llvm::ConstantInt::get(*llvm_ctx, llvm::APInt(32, node->value));
  }

  std::expected<llvm::Value *, std::string>
  operator()(uptr<FloatExprAst> &node) {
    auto fp = llvm::ConstantFP::get(*llvm_ctx, llvm::APFloat(node->value));
    return fp;
  }

  std::expected<llvm::Value *, std::string> operator()(uptr<VarExprAst> &node) {
    auto var = get_defined_var(node->name);
    if (!var)
      return std::unexpected("trying to reference an undefined variable");
    return builder->CreateLoad(var.value()->type, var.value()->alloca,
                               node->name);
  }

  std::expected<llvm::Value *, std::string>
  operator()(uptr<StatementExprAst> &node) {
    return std::visit(*this, node->expr);
  }

  std::expected<llvm::Value *, std::string>
  operator()(uptr<GroupExprAst> &node) {
    return std::visit(*this, node->expr);
  }

  std::expected<llvm::Value *, std::string> operator()(uptr<ForExprAst> &node) {

    auto fn = builder->GetInsertBlock()->getParent();
    auto for_check_bb = llvm::BasicBlock::Create(*llvm_ctx, "for_check", fn);
    auto for_body_bb = llvm::BasicBlock::Create(*llvm_ctx, "for_body", fn);
    auto for_merge_bb = llvm::BasicBlock::Create(*llvm_ctx, "for_merge", fn);

    builder->CreateBr(for_check_bb);

    // Emit for check block
    builder->SetInsertPoint(for_check_bb);
    auto for_cond_expr = std::visit(*this, node->condition);
    if (!for_cond_expr)
      return std::unexpected(for_cond_expr.error());

    for_cond_expr = builder->CreateICmpEQ(
        *for_cond_expr, llvm::ConstantInt::get(*llvm_ctx, llvm::APInt(1, 1)),
        "for_cond");
    builder->CreateCondBr(*for_cond_expr, for_body_bb, for_merge_bb);

    // Emit for body block
    builder->SetInsertPoint(for_body_bb);
    auto for_body_expr = std::visit(*this, node->for_body);
    if (!for_cond_expr)
      return std::unexpected(for_body_expr.error());
    builder->CreateBr(for_check_bb);

    // Emit merge block
    builder->SetInsertPoint(for_merge_bb);

    return for_cond_expr;
  }

  std::expected<llvm::Value *, std::string> operator()(uptr<IfExprAst> &node) {
    auto cond_expr = std::visit(*this, node->condition);
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
    auto then_expr = std::visit(*this, node->then_expr);
    if (!then_expr)
      return std::unexpected(then_expr.error());

    builder->CreateBr(merge_bb);
    then_bb = builder->GetInsertBlock();

    // Emit else block
    std::expected<llvm::Value *, std::string> else_expr = nullptr;
    if (node->else_expr) {
      fn->insert(fn->end(), else_bb);
      builder->SetInsertPoint(else_bb);

      else_expr = std::visit(*this, *node->else_expr);
      if (!else_expr)
        return std::unexpected(else_expr.error());
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
  operator()(uptr<StringExprAst> &node) {
    auto str_const =
        llvm::ConstantDataArray::getString(*llvm_ctx, node->value, true);
    auto global_str = new llvm::GlobalVariable(
        *module, str_const->getType(), true, llvm::GlobalValue::PrivateLinkage,
        str_const, ".str");
    global_str->setAlignment(llvm::Align(1));
    return builder->CreateConstGEP2_32(str_const->getType(), global_str, 0, 0);
  }

  // Expressions
  std::expected<llvm::Value *, std::string>
  operator()(uptr<CallExprAst> &node) {
    // TODO: LLVM IR is getting the function from the generated module rather
    // than the context. Get it from the context and, if it has overloads, just
    // add a index to it.

    auto overloads = ctx.get_overloads(node->fn_name);
    if (!overloads)
      return std::unexpected("no overloads found for call");

    AstExprTypeVisitor visitor = {&ctx};
    auto mangled_name =
        overloads.value()->get_mangled_name(node.get(), &visitor);

    if (!mangled_name)
      return std::unexpected("issue getting mangled name for call");

    auto callee_fn = module->getFunction(*mangled_name);

    if (!callee_fn)
      return std::unexpected("tried to call unknown fn");

    if (callee_fn->arg_size() !=
            node->prefix_args.size() + node->suffix_args.size() &&
        !callee_fn->isVarArg())
      return std::unexpected("incorrect number of arguments used.");

    std::vector<llvm::Value *> args;

    for (auto &arg : node->prefix_args) {
      auto arg_val = std::visit(*this, arg);
      if (!arg_val)
        return std::unexpected(arg_val.error());
      args.push_back(*arg_val);
    }

    for (auto &arg : node->suffix_args) {
      auto arg_val = std::visit(*this, arg);
      if (!arg_val)
        return std::unexpected(arg_val.error());
      args.push_back(*arg_val);
    }

    return builder->CreateCall(callee_fn, args, "calltmp");
  }

  std::expected<llvm::Value *, std::string>
  operator()(uptr<MetaDefExprAst> &node) {
    auto meta_fn = ctx.get_meta(node->name);

    if (!meta_fn)
      return std::unexpected("tried to call unknown meta fn");

    if (meta_fn.value()->arg_num() != (int)node->args.size())
      return std::unexpected("incorrect number of arguments used for meta fn.");

    // TODO: As soon as additional meta fn are defined, they might have
    // different number of arguments
    auto lhs = std::visit(*this, node->args[0]);
    if (!lhs)
      return std::unexpected("error generating lhs of int add");
    auto rhs = std::visit(*this, node->args[1]);
    if (!rhs)
      return std::unexpected("error generating rhs of int add");

    if (meta_fn.value()->kind == MetaFunctionKind::AddInt)
      return builder->CreateAdd(*lhs, *rhs, "addi32tmp");
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
  operator()(uptr<BodyExprAst> &node) {

    if (current_fn != nullptr) {
      for (auto &arg : current_fn->args()) {
        auto arg_alloca = build_alloca_at_start(current_fn, arg.getType(),
                                                arg.getName().str());
        builder->CreateStore(&arg, arg_alloca);
        defined_variables[std::string(arg.getName())] =
            DefinedVariable{arg.getType(), arg_alloca};
      }
    }

    llvm::Value *last_val = nullptr;
    for (auto &stmt : node->statements) {
      auto stmt_expr = build_statement(stmt);
      if (!stmt_expr)
        return std::unexpected(stmt_expr.error());

      if (*stmt_expr != nullptr)
        last_val = *stmt_expr;
    }

    return last_val;
  }
};
