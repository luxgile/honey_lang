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
  /* Eq, */
  LBrace,
  RBrace,
  LPar,
  RPar,
  Bar,
  Id,
  Int,
  Float,
  String,
  Bool,
  Extern,
  Struct,
  If,
  Else,
  Loop,
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

  int last_char = ' ';
  std::string temp_id;

  Lexer(std::string source) {
    this->source = source;
    src_index = -1;
  }

  int next_char();

  Token create_token(TokenKind kind, bool consume = false);

  std::expected<Token, std::string> get_token();
};
