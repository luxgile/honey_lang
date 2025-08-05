#pragma once

#include <cctype>
#include <cstdio>
#include <expected>
#include <string>

enum TokenKind {
  EoF,
  Dot,
  Comma,
  Colon,
  LBrace,
  RBrace,
  LBracks,
  RBracks,
  LPar,
  RPar,
  Amper,
  Pointy,
  Bar,
  Id,
  Int,
  Float,
  String,
  Bool,
  Extern,
  Struct,
  Return,
  Enum,
  Fn,
  If,
  Else,
  Loop,
  Match,
  NewLine,
  Meta,
  Undefined,
};

std::string token_kind_to_string(TokenKind kind);

struct FilePos {
  uint line;
  uint start;
  uint end;
};

struct Token {
  TokenKind kind;
  std::string value;
  FilePos position;

  void print_token();
};

struct Lexer {
  FilePos current_pos = {0, 0, 0};
  bool capturing;

  std::string source;
  int src_index = -1;

  char last_char = ' ';
  std::string temp_id;

  Lexer() { src_index = -1; }

  void set_source(std::string source) {
    this->source = source;
    src_index = -1;
  }

  int next_char();
  int go_back(int steps);

  Token create_token(TokenKind kind, FilePos pos, bool consume = false);

  Token get_token();

  void start_capturing() {
    current_pos.end = current_pos.start;
    capturing = true;
  }
  FilePos stop_capturing() {
    capturing = false;
    auto pos = current_pos;
    if (pos.end != 0)
      pos.end -= 1;
    current_pos.start = current_pos.end;
    return pos;
  }
};
