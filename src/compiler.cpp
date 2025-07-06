#include "compiler.h"
#include "parser.h"
#include "v_llvm_ir.h"
#include "v_pretty_print.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#include <optional>

void Compiler::add_internal_types() {
  ctx.define_llvm_type(VOID_TYPE.get_id(),
                       llvm::Type::getVoidTy(*llvm_gen.llvm_ctx));
  ctx.define_llvm_type(
      RAW_STRING_TYPE.get_id(),
      llvm::Type::getInt8Ty(*llvm_gen.llvm_ctx)->getPointerTo());
  ctx.define_llvm_type(FLOAT_TYPE.get_id(),
                       llvm::Type::getDoubleTy(*llvm_gen.llvm_ctx));
  ctx.define_llvm_type(INT_TYPE.get_id(),
                       llvm::Type::getInt32Ty(*llvm_gen.llvm_ctx));
  ctx.define_llvm_type(BOOL_TYPE.get_id(),
                       llvm::Type::getInt1Ty(*llvm_gen.llvm_ctx));

  ctx.define_meta("i+",
                  std::make_unique<MetaFunction>(MetaFunctionKind::AddInt, 2));
  ctx.define_meta("i-",
                  std::make_unique<MetaFunction>(MetaFunctionKind::SubInt, 2));
  ctx.define_meta("i*",
                  std::make_unique<MetaFunction>(MetaFunctionKind::MulInt, 2));
  ctx.define_meta("i/",
                  std::make_unique<MetaFunction>(MetaFunctionKind::DivInt, 2));
  ctx.define_meta("i%",
                  std::make_unique<MetaFunction>(MetaFunctionKind::ModInt, 2));
  ctx.define_meta(
      "f+", std::make_unique<MetaFunction>(MetaFunctionKind::AddFloat, 2));
  ctx.define_meta(
      "f-", std::make_unique<MetaFunction>(MetaFunctionKind::SubFloat, 2));
  ctx.define_meta(
      "f*", std::make_unique<MetaFunction>(MetaFunctionKind::MulFloat, 2));
  ctx.define_meta(
      "f/", std::make_unique<MetaFunction>(MetaFunctionKind::DivFloat, 2));
  ctx.define_meta(
      "f%", std::make_unique<MetaFunction>(MetaFunctionKind::ModFloat, 2));
  ctx.define_meta("b==",
                  std::make_unique<MetaFunction>(MetaFunctionKind::EqBool, 2));
  ctx.define_meta(
      "b!=", std::make_unique<MetaFunction>(MetaFunctionKind::NotEqBool, 2));
  ctx.define_meta("b<",
                  std::make_unique<MetaFunction>(MetaFunctionKind::LtBool, 2));
  ctx.define_meta("b>",
                  std::make_unique<MetaFunction>(MetaFunctionKind::GtBool, 2));
  ctx.define_meta(
      "b<=", std::make_unique<MetaFunction>(MetaFunctionKind::LtEqBool, 2));
  ctx.define_meta(
      "b>=", std::make_unique<MetaFunction>(MetaFunctionKind::GtEqBool, 2));
  ctx.define_meta("b&&",
                  std::make_unique<MetaFunction>(MetaFunctionKind::AndBool, 2));
  ctx.define_meta("b||",
                  std::make_unique<MetaFunction>(MetaFunctionKind::OrBool, 2));
}

uptr<FileStmtAst> Compiler::parse_file(std::string source) {
  /* if (print_tokens) { */
  /*   std::println(); */
  /*   std::println(" ----- LEXER RESULTS -----"); */
  /*   std::println(); */
  /* } */

  return parser.parse_file(source);
  /**/
  /* do { */
  /*   auto tkn_res = lexer.get_token(); */
  /*   if (!tkn_res) { */
  /*     return std::unexpected(std::format("failed tokenizing at {} - {}", */
  /*                                        lexer.current_pos.line, */
  /*                                        lexer.current_pos.start)); */
  /*   } */
  /**/
  /*   last_token = *tkn_res; */
  /*   if (print_tokens) */
  /*     last_token.print_token(); */
  /**/
  /*   auto statement = parser.parse_token(last_token); */
  /*   if (!statement) */
  /*     continue; */
  /*   statements.push_back(std::move(*statement)); */
  /* } while (last_token.kind != EoF && last_token.kind != Undefined); */
  /* return statements; */
}

std::expected<void, std::string>
Compiler::gen_llvm_ir(uptr<FileStmtAst> &file) {
  PrettyPrintAstVisitor pretty_printer = {&ctx};
  std::println();
  std::println(" ----- PARSED CODE -----");
  std::println();

  LlvmIrGenAstVisitor::GenCtx gctx;
  for (auto &stmt : file->statements) {
    if (print_parsed_statements) {
      std::visit(pretty_printer, stmt);
      std::println("");
    }

    auto gen_result = llvm_gen.build_statement(&gctx, stmt);

    if (!gen_result)
      return std::unexpected(std::format("gen error: {}", gen_result.error()));
  }

  if (print_llvm_ir) {
    std::println();
    std::println(" ----- GENERATED LLVM IR -----");
    std::println();
    llvm_gen.module->print(llvm::errs(), nullptr);
  }

  return {};
}
std::expected<void, std::string>
Compiler::compile_to_obj_file(std::string file_name) {
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  std::string triple = llvm::sys::getDefaultTargetTriple();
  std::string target_err;
  auto target = llvm::TargetRegistry::lookupTarget(triple, target_err);
  if (!target)
    return std::unexpected(std::format("error getting target: {}", target_err));

  auto cpu = "generic";
  auto features = "";
  auto target_machine =
      target->createTargetMachine(triple, cpu, features, {}, llvm::Reloc::PIC_);
  llvm_gen.module->setDataLayout(target_machine->createDataLayout());
  llvm_gen.module->setTargetTriple(triple);

  std::error_code ec;
  llvm::raw_fd_ostream output_file = {file_name, ec, llvm::sys::fs::OF_None};
  if (ec)
    return std::unexpected(
        std::format("error creating file '{}': {}", file_name, ec.message()));

  llvm::legacy::PassManager pass;
  auto file_type = llvm::CodeGenFileType::ObjectFile;
  if (target_machine->addPassesToEmitFile(pass, output_file, nullptr,
                                          file_type))
    return std::unexpected(
        std::format("target machine can't emit a file of this type"));

  pass.run(*llvm_gen.module);
  output_file.flush();
  return {};
}

std::expected<void, std::string> Compiler::compile_source(std::string source) {
  auto file = parse_file(source);
  if (parser.has_parsing_errors()) {
    parser.print_parsing_errors();
    return std::unexpected("Compilation failed, see previous errors.");
  }

  auto gen_res = gen_llvm_ir(file);
  if (!gen_res)
    return std::unexpected(gen_res.error());

  auto com_res = compile_to_obj_file("honey.o");
  if (!com_res)
    return std::unexpected(com_res.error());

  return {};
}
