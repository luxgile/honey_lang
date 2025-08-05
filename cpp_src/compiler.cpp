#include "compiler.h"
#include "parser.h"
#include "v_llvm_ir.h"
#include "v_pretty_print.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
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
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <print>
#include <variant>

#define BIN_META(id, op) ctx.define_meta(id, new MetaBinOp(op))
#define CMP_META(id, op) ctx.define_meta(id, new MetaCmpOp(op))

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

  using BinOp = llvm::Instruction::BinaryOps;
  using CmpOp = llvm::CmpInst::Predicate;
  BIN_META("i+", BinOp::Add);
  BIN_META("i-", BinOp::Sub);
  BIN_META("i*", BinOp::Mul);
  BIN_META("i/", BinOp::SDiv);
  BIN_META("i%", BinOp::SRem);
  CMP_META("i==", CmpOp::ICMP_EQ);
  CMP_META("i!=", CmpOp::ICMP_NE);
  CMP_META("i<", CmpOp::ICMP_SLT);
  CMP_META("i<=", CmpOp::ICMP_SLE);
  CMP_META("i>", CmpOp::ICMP_SGT);
  CMP_META("i>=", CmpOp::ICMP_SGE);

  BIN_META("f+", BinOp::FAdd);
  BIN_META("f-", BinOp::FSub);
  BIN_META("f*", BinOp::FMul);
  BIN_META("f/", BinOp::FDiv);
  BIN_META("f%", BinOp::FRem);
  CMP_META("fo", CmpOp::FCMP_OEQ);
  CMP_META("fu", CmpOp::FCMP_UEQ);
  CMP_META("fo==", CmpOp::FCMP_OEQ);
  CMP_META("fu==", CmpOp::FCMP_UEQ);
  CMP_META("fo!=", CmpOp::FCMP_ONE);
  CMP_META("fu!=", CmpOp::FCMP_UNE);
  CMP_META("fo>", CmpOp::FCMP_OGT);
  CMP_META("fu>", CmpOp::FCMP_UGT);
  CMP_META("fo>=", CmpOp::FCMP_OGE);
  CMP_META("fu>=", CmpOp::FCMP_UGE);
  CMP_META("fo<", CmpOp::FCMP_OLT);
  CMP_META("fu<", CmpOp::FCMP_ULT);
  CMP_META("fo<=", CmpOp::FCMP_OLE);
  CMP_META("fu<=", CmpOp::FCMP_ULE);

  BIN_META("b==", BinOp::And);
  BIN_META("b||", BinOp::Or);
  BIN_META(">>", BinOp::LShr);
  BIN_META("<<", BinOp::Shl);
}

uptr<FileStmtAst> Compiler::parse_file(std::string source) {
  auto file = parser.parse_file(source, print_tokens);
  // TODO: Resolve syntactic sugar
  return file;
}

std::expected<void, std::string>
Compiler::gen_llvm_ir(uptr<FileStmtAst> &file) {
  PrettyPrintAstVisitor pretty_printer = {&ctx};
  if (print_parsed_statements) {
    std::println();
    std::println(" ----- PARSED CODE -----");
    std::println();
  }

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

  std::error_code ec;
  llvm::raw_fd_ostream output_file = {file_name, ec, llvm::sys::fs::OF_None};
  if (ec)
    return std::unexpected(
        std::format("error creating file '{}': {}", file_name, ec.message()));

  std::string triple = llvm::sys::getDefaultTargetTriple();
  std::string target_err;
  auto target = llvm::TargetRegistry::lookupTarget(triple, target_err);
  if (!target)
    throw std::unexpected(std::format("error getting target: {}", target_err));

  auto cpu = "generic";
  auto features = "";
  auto target_machine =
      target->createTargetMachine(triple, cpu, features, {}, llvm::Reloc::PIC_);

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

std::expected<void, std::string>
Compiler::build_and_run_file(std::string file_path) {
  std::println("compiling {}...", file_path);
  auto source_path = std::filesystem::path{file_path};

  std::ifstream honey_file{source_path.string(), std::ios::in};
  if (!honey_file.is_open())
    return std::unexpected(
        std::format("error opening {}", source_path.string()));

  std::string source{std::istreambuf_iterator<char>(honey_file),
                     std::istreambuf_iterator<char>()};

  auto build_dir = std::filesystem::path{source_path};
  build_dir = build_dir.parent_path();
  return build_and_run_source(source, source_path.filename().string(),
                              build_dir);
}

std::expected<void, std::string>
Compiler::build_and_run_source(std::string source, std::string file_name,
                               std::filesystem::path build_path) {
  uptr<FileStmtAst> file;
  try {
    file = parse_file(source);
  } catch (const ParserError &err) {
    ParserError::print_error(err, source);
    parser.print_parsing_errors();
    return std::unexpected("compilation failed, see previous errors.");
  }

  if (parser.has_parsing_errors()) {
    parser.print_parsing_errors();
    return std::unexpected("compilation failed, see previous errors.");
  }

  // Generate IR code
  auto gen_res = gen_llvm_ir(file);
  if (!gen_res)
    return std::unexpected(gen_res.error());

  const std::string BUILD_DIR = ".build";

  // Build object file
  auto build_dir = std::filesystem::path{build_path};
  build_dir = build_dir.append(BUILD_DIR);
  std::filesystem::create_directory(build_dir);
  auto object_path = build_dir.append(file_name).replace_extension("o");
  try {
    auto com_res = compile_to_obj_file(object_path.string());
    if (!com_res)
      return std::unexpected(com_res.error());
  } catch (const std::exception &e) {
    return std::unexpected(e.what());
  }

  // Link to executable and run it
  auto exe_path = object_path;
  exe_path = exe_path.replace_extension("");
  system(std::format("clang {} -o {}", object_path.string(), exe_path.string())
             .c_str());
  system(exe_path.string().c_str());
  return {};
}

std::expected<std::int32_t, std::string>
Compiler::jit_and_run_source(std::string source) {
  uptr<FileStmtAst> file;
  try {
    file = parse_file(source);
  } catch (const ParserError &err) {
    ParserError::print_error(err, source);
    parser.print_parsing_errors();
    return std::unexpected("compilation failed, see previous errors.");
  }

  if (parser.has_parsing_errors()) {
    parser.print_parsing_errors();
    return std::unexpected("compilation failed, see previous errors.");
  }

  // Generate IR code
  auto gen_res = gen_llvm_ir(file);
  if (!gen_res)
    return std::unexpected(gen_res.error());

  // JIT to run the code
  auto rt = llvm_gen.jit->getMainJITDylib().createResourceTracker();
  auto tsm = llvm::orc::ThreadSafeModule(std::move(llvm_gen.module),
                                         std::move(llvm_gen.llvm_ctx));
  auto err = llvm_gen.jit->addModule(std::move(tsm), rt);
  if (err)
    return std::unexpected("issue adding module to jit");

  auto main_fn = llvm_gen.jit->lookup("main");
  using MainFnPtr = std::int32_t (*)();
  auto main_res = main_fn->getAddress().toPtr<MainFnPtr>()();
  err = rt->remove();
  if (err)
    return std::unexpected("issue removing resources from ResourceTracker");
  return main_res;
}
Compiler::Compiler() : parser(&ctx), llvm_gen(&ctx) {
  parser.debug_checks = false;
  parser.debug_scan = false;
  ctx.ptr_llvm_ty = llvm::PointerType::get(*llvm_gen.llvm_ctx, 0);
  add_internal_types();
}
