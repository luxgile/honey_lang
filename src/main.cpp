#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "visitors.h"
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
#include <fstream>
#include <iostream>
#include <memory>
#include <ostream>
#include <system_error>
#include <vector>

int main() {
  std::ifstream honey_file{"honey/main.hun", std::ios::in};
  if (!honey_file.is_open())
    return 1;

  std::string honey_source{std::istreambuf_iterator<char>(honey_file),
                           std::istreambuf_iterator<char>()};

  Lexer lexer{honey_source};

  Token token;
  Parser parser;
  PrettyPrintAstVisitor pretty_printer;
  LlvmIrGenAstVisitor llvm_gen{parser.ctx};

  parser.debug_checks = true;
  parser.debug_scan = true;

  // Register print fn
  /* std::vector<uptr<FieldDefAst>> print_args; */
  /* print_args.push_back(std::make_unique<FieldDefAst>("msg", "RawString")); */
  /* auto print_fn = std::make_unique<FnHeaderAst>( */
  /*     "print", "Void", std::vector<uptr<FieldDefAst>>{},
   * std::move(print_args)); */
  /* parser.ctx.defined_ext_fns.push_back(print_fn.get()); */

  // Register basic types
  parser.ctx.define_type("Void", llvm::Type::getVoidTy(*llvm_gen.llvm_ctx));
  parser.ctx.define_type(
      "RawString", llvm::Type::getInt8Ty(*llvm_gen.llvm_ctx)->getPointerTo());

  parser.ctx.define_meta("i+", std::make_unique<MetaFunction>(MetaFunctionKind::AddInt, 2));
  parser.ctx.define_meta("f+", std::make_unique<MetaFunction>(MetaFunctionKind::AddFloat, 2));

  auto ext_fn_res = llvm_gen.build_external_fns();
  if (!ext_fn_res) {
    std::println("error on ext fn gen: ", ext_fn_res.error());
    return 1;
  }

  std::vector<AstStatement> statements;
  do {
    auto tkn_result = lexer.get_token();
    if (!tkn_result) {
      std::cerr << "Failed tokenizing at " << lexer.current_pos.line << " - "
                << lexer.current_pos.start << "\n";
      return 1;
    }

    token = *tkn_result;
    token.print_token();

    auto statement = parser.parse_token(token);
    if (!statement)
      continue;

    std::visit(pretty_printer, *statement);

    statements.push_back(std::move(*statement));

  } while (token.kind != TokenKind::EoF && token.kind != TokenKind::Undefined);

  // Code gen
  for (auto &stmt : statements) {
    auto gen_result = llvm_gen.build_statement(stmt);
    if (!gen_result)
      std::println("gen error: {}", gen_result.error());
  }

  std::println();
  std::println(" ----- GENERATED LLVM IR -----");
  std::println();
  llvm_gen.module->print(llvm::errs(), nullptr);

  // Compilation
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  std::string triple = llvm::sys::getDefaultTargetTriple();
  std::string target_err;
  auto target = llvm::TargetRegistry::lookupTarget(triple, target_err);
  if (!target) {
    std::println("error getting target: {}", target_err);
    return 1;
  }

  auto cpu = "generic";
  auto features = "";
  auto target_machine =
      target->createTargetMachine(triple, cpu, features, {}, llvm::Reloc::PIC_);
  llvm_gen.module->setDataLayout(target_machine->createDataLayout());
  llvm_gen.module->setTargetTriple(triple);

  std::error_code ec;
  llvm::raw_fd_ostream output_file = {"honey.o", ec, llvm::sys::fs::OF_None};
  if (ec) {
    std::println("error creating file 'honey.o': {}", ec.message());
    return 1;
  }

  llvm::legacy::PassManager pass;
  auto file_type = llvm::CodeGenFileType::ObjectFile;
  if (target_machine->addPassesToEmitFile(pass, output_file, nullptr,
                                          file_type)) {
    std::println("target machine can't emit a file of this type");
    return 1;
  }

  pass.run(*llvm_gen.module);
  output_file.flush();

  return 0;
}
