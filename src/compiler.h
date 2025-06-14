#pragma once

#include "lexer.h"
#include "parser.h"
#include "program_ctx.h"
#include "v_llvm_ir.h"
#include <expected>
#include <string>
#include <vector>

class Compiler {
  ProgramCtx ctx;
  Lexer lexer;
  Parser parser;
  LlvmIrGenAstVisitor llvm_gen;

  bool print_tokens = true;
  bool print_parsed_statements = true;
  bool print_llvm_ir = true;

  void add_internal_types();
  std::expected<std::vector<AstStatement>, std::string>
  parse_statements(std::string source);

  std::expected<void, std::string>
  gen_llvm_ir(std::vector<AstStatement> &statements);

  std::expected<void, std::string> compile_to_obj_file(std::string file_name);

public:
  Compiler() : parser(&ctx), llvm_gen(&ctx) {
    parser.debug_checks = true;
    parser.debug_scan = true;
    add_internal_types();
  }

  std::expected<void, std::string> compile_source(std::string source);
};
