#pragma once

#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "program_ctx.h"
#include "v_llvm_ir.h"
#include "llvm/IR/DerivedTypes.h"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class Compiler {
  ProgramCtx ctx;
  Parser parser;
  LlvmIrGenAstVisitor llvm_gen;

  void add_internal_types();
  uptr<FileStmtAst> parse_file(std::string source);

  std::expected<void, std::string> gen_llvm_ir(uptr<FileStmtAst> &statements);

  std::expected<void, std::string> compile_to_obj_file(std::string file_name);

public:
  bool print_tokens = false;
  bool print_parsed_statements = false;
  bool print_llvm_ir = false;

  Compiler();

  /// Runs the source on the go, returning the program exit code.
  std::expected<std::int32_t, std::string>
  jit_and_run_source(std::string source);

  /// Will build a honey file into an executable. The source file needs to
  /// provide a valid main function to work.
  std::expected<void, std::string> build_and_run_file(std::string file_path);

  // TODO(Luxgile): This might not be needed.
  std::expected<void, std::string>
  build_and_run_source(std::string source, std::string file_name,
                       std::filesystem::path build_path);
};
