#include "compiler.h"
#include <fstream>
#include <iostream>

int main() {
  std::ifstream honey_file{"honey/main.hun", std::ios::in};
  if (!honey_file.is_open())
    return 1;

  std::string honey_source{std::istreambuf_iterator<char>(honey_file),
                           std::istreambuf_iterator<char>()};

  Compiler compiler;
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
