#pragma once

#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "program_ctx.h"
#include "v_llvm_ir.h"
#include "llvm/IR/DerivedTypes.h"
#include <expected>
#include <string>
#include <vector>

class Compiler {
  ProgramCtx ctx;
  Parser parser;
  LlvmIrGenAstVisitor llvm_gen;

  bool print_tokens = false;
  bool print_parsed_statements = false;
  bool print_llvm_ir = true;

  void add_internal_types();
  uptr<FileStmtAst> parse_file(std::string source);

  std::expected<void, std::string> gen_llvm_ir(uptr<FileStmtAst> &statements);

  std::expected<void, std::string> compile_to_obj_file(std::string file_name);

public:
  Compiler() : parser(&ctx), llvm_gen(&ctx) {
    parser.debug_checks = false;
    parser.debug_scan = false;
    ctx.ptr_llvm_ty = llvm::PointerType::get(*llvm_gen.llvm_ctx, 0);
    add_internal_types();
  }

  std::expected<void, std::string> compile_source(std::string source);
};
