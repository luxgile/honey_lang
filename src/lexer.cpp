#include "lexer.h"

int Lexer::next_char() {
  src_index += 1;
  int c = source[src_index];
  if (capturing)
    current_pos.end += 1;
  else {
    current_pos.start += 1;
    current_pos.end = current_pos.start;
  }

  if (c == '\n') {
    current_pos.line += 1;
    current_pos.end = 0;
    if (!capturing)
      current_pos.start = 0;
  }
  return c;
}

Token Lexer::create_token(TokenKind kind, bool consume) {
  if (temp_id == "") temp_id = last_char;
  Token token = Token{kind, std::string(temp_id), current_pos};
  temp_id = "";
  if (consume)
    last_char = next_char();
  return token;
}

std::expected<Token, std::string> Lexer::get_token() {
  if ( src_index >= (int)source.size())
    return create_token(EoF);

  // Skip whitespace
  while (std::isspace(last_char))
    last_char = next_char();

  // Identifier
  if (std::isalpha(last_char)) {
    temp_id = last_char;

    capturing = true;
    last_char = next_char();
    while (std::isalpha(last_char)) {
      temp_id += last_char;
      last_char = next_char();
    }
    capturing = false;

    return create_token(Id);
  }

  // String
  if (last_char == '"') {
    last_char = next_char();
    temp_id = last_char;

    capturing = true;
    last_char = next_char();
    while (last_char != '"') {
      temp_id += last_char;
      last_char = next_char();
    }
    capturing = false;

    return create_token(String, true);
  }

  // Numbers
  if (std::isdigit(last_char)) {
    temp_id = last_char;
    bool is_float = false;

    capturing = true;
    last_char = next_char();
    while (std::isdigit(last_char) || last_char == '.') {
      if (last_char == '.') {
        if (!is_float)
          is_float = true;
        else
          std::unexpected("multiple '.' in number.");
      }

      temp_id += last_char;
      last_char = next_char();
    }
    capturing = false;

    return create_token(is_float ? Int : Float);
  }

  switch (last_char) {
  case ':':
    return create_token(Colon, true);
  case '=':
    return create_token(Eq, true);
  case '|':
    return create_token(Bar, true);
  case '{':
    return create_token(LBrace, true);
  case '}':
    return create_token(RBrace, true);
  }

  return create_token(Undefined);
}

