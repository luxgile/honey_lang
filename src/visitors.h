#pragma once

#include "ast.h"
#include "helpers.h"
#include "parser.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Alignment.h"
#include <expected>
#include <functional>
#include <memory>
#include <print>
#include <string>
#include <variant>
#include <vector>

struct LlvmIrGenAstVisitor {
  ProgramCtx &ctx;
  uptr<llvm::LLVMContext> llvm_ctx;
  uptr<llvm::IRBuilder<>> builder;
  uptr<llvm::Module> module;

  std::map<std::string, llvm::Value *> defined_variables;

  LlvmIrGenAstVisitor(ProgramCtx &ctx) : ctx(ctx) {
    llvm_ctx = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("honey jit", *llvm_ctx);
    builder = std::make_unique<llvm::IRBuilder<>>(*llvm_ctx);
  }

  // State
  llvm::Function *current_fn;

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

    auto fn_type = llvm::FunctionType::get(llvm::Type::getVoidTy(*llvm_ctx),
                                           args_types, false);
    auto fn = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage,
                                     header.name, module.get());

    int i = 0;
    for (auto &arg : header.prefix_args) {
      fn->getArg(i)->setName(arg->name);
      i += 1;
    }
    for (auto &arg : header.suffix_args) {
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

    if (!node.is_external) {

      defined_variables.clear();
      current_fn = fn_header.value();
      auto body_ret = std::visit(*this, *node.body);
      current_fn = nullptr;

      if (!body_ret) {
        fn_header.value()->eraseFromParent();
        return std::unexpected(body_ret.error());
      }

      builder->CreateRet(*body_ret);
    } else {
      fn_header.value()->setCallingConv(llvm::CallingConv::C);
    }

    llvm::verifyFunction(*fn_header.value());
    return {};
  }

  std::expected<void, std::string> build_external_fns() {
    for (auto ext_fn : ctx.defined_ext_fns) {
      auto r = build_prototype(*ext_fn);
      if (!r)
        return std::unexpected(r.error());
    }
    return {};
  }

  std::expected<void, std::string> build_statement(AstStatement &statement) {
    return build_fn(*std::get<uptr<FnDefAst>>(statement));
  }

  // Literals
  std::expected<llvm::Value *, std::string> operator()(uptr<IntExprAst> &node) {
    return llvm::ConstantInt::get(*llvm_ctx, llvm::APInt(32, node->value));
  }

  std::expected<llvm::Value *, std::string>
  operator()(uptr<FloatExprAst> &node) {
    return llvm::ConstantFP::get(*llvm_ctx, llvm::APFloat(node->value));
  }

  std::expected<llvm::Value *, std::string> operator()(uptr<VarExprAst> &node) {
    auto var = defined_variables[node->name];
    if (!var)
      return std::unexpected("trying to reference an undefined variable");
    return var;
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
    auto callee_fn = module->getFunction(node->fn_name);

    if (!callee_fn)
      return std::unexpected("tried to call unknown fn");

    if (callee_fn->arg_size() !=
        node->prefix_args.size() + node->suffix_args.size())
      return std::unexpected("incorrect number of arguments used.");

    std::vector<llvm::Value *> args;
    for (auto &arg : node->suffix_args) {
      auto arg_val = std::visit(*this, arg);
      if (!arg_val)
        return std::unexpected(arg_val.error());
      args.push_back(*arg_val);
    }

    return builder->CreateCall(callee_fn, args, "calltmp");
  }

  std::expected<llvm::Value *, std::string>
  operator()(uptr<BodyExprAst> &node) {
    auto bb = llvm::BasicBlock::Create(*llvm_ctx, "entry", current_fn);
    builder->SetInsertPoint(bb);

    if (current_fn != nullptr) {
      for (auto& arg : current_fn->args()) {
        defined_variables[std::string(arg.getName())] = &arg;
      }
    }

    llvm::Value *last_val = nullptr;
    for (auto &expr : node->exprs) {
      auto expr_res = std::visit(*this, expr);
      if (!expr_res)
        return expr_res;
      last_val = *expr_res;
    }

    if (last_val == nullptr) {
      return std::unexpected("body has no returning value");
    }

    return last_val;
  }
};

struct PrettyPrintAstVisitor {
  int indent = 0;

  void print_indent() {
    for (int i = 0; i < indent; i++) {
      std::print("  ");
    }
  }

  // Literals
  void operator()(uptr<IntExprAst> &node) { std::print("{}", node->value); }

  void operator()(uptr<FloatExprAst> &node) { std::print("{}", node->value); }

  void operator()(uptr<StringExprAst> &node) { std::print("{}", node->value); }

  void operator()(uptr<VarExprAst> &node) { std::print("{}", node->name); }

  void operator()(uptr<FieldDefAst> &node) {
    std::println("{}: {}", node->name, node->type);
  }

  // Expressions
  void operator()(uptr<CallExprAst> &node) {
    for (auto &arg : node->prefix_args) {
      std::visit(*this, arg);
    }

    std::print(" |{}| ", node->fn_name);

    for (auto &arg : node->suffix_args) {
      std::visit(*this, arg);
    }
  }

  // Function
  void operator()(uptr<FnHeaderAst> &node) {
    std::print("{} := ", node->name);

    for (auto &arg : node->prefix_args) {
      (*this)(arg);
    }

    std::print("|{}|", node->type);

    for (auto &arg : node->suffix_args) {
      (*this)(arg);
    }
  }

  void operator()(uptr<BodyExprAst> &node) {
    std::println("{{");
    indent += 1;
    for (auto &expr : node->exprs) {
      print_indent();
      std::visit(*this, expr);
    }
    indent -= 1;
    std::println("\n}}");
  }

  void operator()(uptr<FnDefAst> &node) {
    (*this)(node->fn_header);
    if (node->is_external) {
      std::print("\n");
      return;
    }

    std::visit(*this, *node->body);
  }
};
