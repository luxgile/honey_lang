#pragma once

#include "compiler.h"
#include "doctest.h"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

inline std::string cpp_to_hun(std::string og_file, std::string filename) {
  auto file = std::filesystem::path{og_file};
  file.replace_filename(filename);
  file.replace_extension("hun");
  return file.string();
}

inline std::int32_t run_src(std::string src) {
  Compiler c{};
  auto res = c.jit_and_run_source(src);
  if (!res)
    throw std::runtime_error(res.error());
  return *res;
}

inline std::int32_t run_file(std::string filename) {
  std::ifstream honey_file{filename, std::ios::in};
  if (!honey_file.is_open())
    throw std::runtime_error(std::format("error opening {}", filename));

  std::string source{std::istreambuf_iterator<char>(honey_file),
                     std::istreambuf_iterator<char>()};
  return run_src(source);
}

#define FILE(file, val) CHECK(run_file(cpp_to_hun(__FILE__, file)) == val)
#define SOURCE(src, val) CHECK(run_src(src) == val)
