#include "compiler.h"
#include <fstream>
#include <iostream>
#include <print>

int main(int argc, char *argv[]) {
  if (argc == 1) {
    std::println("error: no input file");
    return 1;
  }

  Compiler compiler;
  for (int i = 1; i < argc - 1; i++) {
    auto arg = argv[i];
    if (strcmp(arg, "--print_ir") == 0)
      compiler.print_llvm_ir = true;
    else if (strcmp(arg, "--print_parse") == 0)
      compiler.print_parsed_statements = true;
    else if (strcmp(arg, "--print_tokens") == 0)
      compiler.print_tokens = true;
    else {
      std::println("unrecognized argument '{}'", arg);
      std::println("allowed arguments:");
      std::println(" --print_ir : prints the compiled llvm ir");
      std::println(" --print_parse : pretty prints the ast");
      std::println(" --print_tokens : prints all tokens found");
      return 1;
    }
  }

  std::ifstream honey_file{argv[argc - 1], std::ios::in};
  if (!honey_file.is_open())
    return 1;

  std::string honey_source{std::istreambuf_iterator<char>(honey_file),
                           std::istreambuf_iterator<char>()};

  auto com_res = compiler.compile_source(honey_source);
  if (!com_res) {
    std::println("{}", com_res.error());
    return 1;
  }

  system("clang honey.o -o main");
  std::println("\ncompiled object file to executable");

  std::println("\nhoney program output:");
  system("./main");

  return 0;
}
