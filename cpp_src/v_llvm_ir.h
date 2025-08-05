#pragma once

#include "ast.h"
#include "helpers.h"
#include "jit.h"
#include "program_ctx.h"
#include "types.h"
#include "v_expr_type.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassInstrumentation.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
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
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

struct LlvmIrGenAstVisitor {
  ProgramCtx *ctx;
  bool should_optimize = false;
  uptr<llvm::LLVMContext> llvm_ctx;
  uptr<llvm::IRBuilder<>> builder;
  uptr<llvm::Module> module;
  uptr<llvm::orc::KaleidoscopeJIT> jit;
  uptr<llvm::FunctionPassManager> fpm;
  uptr<llvm::LoopAnalysisManager> lam;
  uptr<llvm::FunctionAnalysisManager> fam;
  uptr<llvm::CGSCCAnalysisManager> cgam;
  uptr<llvm::ModuleAnalysisManager> mam;
  uptr<llvm::PassInstrumentationCallbacks> pic;
  uptr<llvm::StandardInstrumentations> si;

  LlvmIrGenAstVisitor(ProgramCtx *ctx) : ctx(ctx) {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    std::string triple = llvm::sys::getDefaultTargetTriple();

    llvm_ctx = std::make_unique<llvm::LLVMContext>();
    auto _jit_res = llvm::orc::KaleidoscopeJIT::Create();
    if (!_jit_res)
      throw std::runtime_error("issue creating jit");
    jit = std::move(_jit_res.get());
    module = std::make_unique<llvm::Module>("honey jit", *llvm_ctx);
    module->setDataLayout(jit->getDataLayout());
    module->setTargetTriple(triple);

    builder = std::make_unique<llvm::IRBuilder<>>(*llvm_ctx);

    fpm = std::make_unique<llvm::FunctionPassManager>();
    lam = std::make_unique<llvm::LoopAnalysisManager>();
    fam = std::make_unique<llvm::FunctionAnalysisManager>();
    cgam = std::make_unique<llvm::CGSCCAnalysisManager>();
    mam = std::make_unique<llvm::ModuleAnalysisManager>();
    pic = std::make_unique<llvm::PassInstrumentationCallbacks>();
    si = std::make_unique<llvm::StandardInstrumentations>(*llvm_ctx, true);
    si->registerCallbacks(*pic, mam.get());

    fpm->addPass(llvm::InstCombinePass());
    fpm->addPass(llvm::ReassociatePass());
    fpm->addPass(llvm::GVNPass());
    fpm->addPass(llvm::SimplifyCFGPass());

    auto pb = llvm::PassBuilder{};
    pb.registerModuleAnalyses(*mam);
    pb.registerFunctionAnalyses(*fam);
    pb.crossRegisterProxies(*lam, *fam, *cgam, *mam);
  }

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

    std::optional<llvm::Value *> method_parent;

    std::optional<DefinedVariable *> get_defined_var(std::string name) {
      if (defined_variables.find(name) == defined_variables.end())
        return std::nullopt;
      return &defined_variables[name];
    }
  };

  std::expected<void, std::string> store_in_value(GenCtx *gctx,
                                                  llvm::Value *ptr,
                                                  AstTypeId ptr_ty_id,
                                                  AstExpression &expr);

  llvm::AllocaInst *build_alloca_at_start(llvm::Function *fn, llvm::Type *type,
                                          std::string name);

  std::expected<llvm::Function *, std::string>
  build_prototype(FnHeaderAst &header);

  std::expected<void, std::string> build_fn(GenCtx *gctx, FnDefAst &node);

  std::expected<void, std::string> build_struct(GenCtx *gctx,
                                                StructDefAst &node);

  std::expected<void, std::string> build_enum(GenCtx *gctx, EnumDefAst &node);

  std::expected<void, std::string> build_var(GenCtx *gctx, VarDefStmtAst &node);

  std::expected<void, std::string> build_var_assignment(GenCtx *gctx,
                                                        VarAssignStmtAst &node);

  std::expected<void, std::string> build_return(GenCtx *gctx,
                                                ReturnStmtAst &node);

  std::expected<llvm::Value *, std::string>
  build_statement(GenCtx *gctx, AstStatement &statement);

  std::expected<llvm::Value *, std::string> build_expr(GenCtx *gctx,
                                                       AstExpression &expr);

  // Literals
  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<BoolExprAst> &node);

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<IntExprAst> &node);

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<FloatExprAst> &node);

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<VarExprAst> &node);

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<IndexExprAst> &node);

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<StatementExprAst> &node);

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<GroupExprAst> &node);

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<RefExprAst> &node);

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<DerefExprAst> &node);

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<StructExprAst> &_);

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<EnumExprAst> &_);

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<ForExprAst> &node);

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<SingleMatchExprAst> &node);

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<IfExprAst> &node);

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<NoOpAst> &node);

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<ArrayExprAst> &node);

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<StringExprAst> &node);

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<MemberAccesorExprAst> &node);

  // Expressions
  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<CallExprAst> &node);

  std::expected<llvm::Value *, std::string>
  visit_expr(GenCtx *gctx, uptr<MetaDefExprAst> &node);

  std::expected<llvm::Value *, std::string> visit_expr(GenCtx *gctx,
                                                       uptr<BodyExprAst> &node);
};

struct LlvmStoreAllocaVisitor {
  llvm::IRBuilder<> *builder;
  llvm::Value *alloca;
  LlvmIrGenAstVisitor *llvm_gen;

  std::expected<void, std::string>
  build_alloca(LlvmIrGenAstVisitor::GenCtx *gctx, AstExpression &expr);

  template <class T>
  std::expected<void, std::string>
  simple_alloca(LlvmIrGenAstVisitor::GenCtx *gctx, uptr<T> &node);

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
template <class T>
inline std::expected<void, std::string>
LlvmStoreAllocaVisitor::simple_alloca(LlvmIrGenAstVisitor::GenCtx *gctx,
                                      uptr<T> &node) {
  /* std::println("rvalue start mode: {}", llvm_gen->rvalue_mode); */
  gctx->rvalue_mode = true;
  auto expr = llvm_gen->visit_expr(gctx, node);
  gctx->rvalue_mode = false;

  if (!expr)
    return std::unexpected(expr.error());
  builder->CreateStore(*expr, alloca);
  return {};
}

/// Internal functions that are resolved at compile time
struct MetaFunction {
  int num_args;

  MetaFunction(int num_args) : num_args(num_args) {}
  virtual ~MetaFunction() {}

  int arg_num() { return num_args; }
  virtual std::expected<llvm::Value *, std::string>
  gen_ir(LlvmIrGenAstVisitor *llvm, LlvmIrGenAstVisitor::GenCtx *gctx,
         MetaDefExprAst *node) = 0;
  virtual AstTypeId get_type_id(ProgramCtx* ctx) const = 0;
};

struct MetaBinOp final : public MetaFunction {
  llvm::Instruction::BinaryOps op;

  MetaBinOp(llvm::Instruction::BinaryOps op) : MetaFunction{2}, op(op) {}
  ~MetaBinOp() {}

  std::expected<llvm::Value *, std::string>
  gen_ir(LlvmIrGenAstVisitor *llvm, LlvmIrGenAstVisitor::GenCtx *gctx,
         MetaDefExprAst *node) override;

  virtual AstTypeId get_type_id(ProgramCtx *ctx) const override;
};

struct MetaCmpOp final : public MetaFunction {
  llvm::CmpInst::Predicate op;

  MetaCmpOp(llvm::CmpInst::Predicate op) : MetaFunction{2}, op(op) {}
  ~MetaCmpOp() {}

  std::expected<llvm::Value *, std::string>
  gen_ir(LlvmIrGenAstVisitor *llvm, LlvmIrGenAstVisitor::GenCtx *gctx,
         MetaDefExprAst *node) override;
  virtual AstTypeId get_type_id(ProgramCtx *ctx) const override;
};
