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

  auto file_path = argv[argc - 1];
  auto com_res = compiler.build_and_run_file(file_path);
  if (!com_res) {
    std::println("{}", com_res.error());
    return 1;
  }

  return 0;
}
