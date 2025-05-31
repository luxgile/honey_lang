#pragma once

#include <cctype>
#include <cstdio>
#include <expected>
#include <string>

enum TokenKind {
  EoF,
  Colon,
  Eq,
  LBrace,
  RBrace,
  Bar,
  Id,
  Int,
  Float,
  String,
  Undefined,
};

struct FilePos {
  uint line;
  uint start;
  uint end;
};

struct Token {
  TokenKind kind;
  std::string value;
  FilePos position;
};

struct Lexer {
  FilePos current_pos;
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
