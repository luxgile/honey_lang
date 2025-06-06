#include "lexer.h"
#include <cctype>
#include <iostream>
#include <string>

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
  if (temp_id == "")
    temp_id = last_char;
  Token token = Token{kind, std::string(temp_id), current_pos};
  temp_id = "";
  if (consume)
    last_char = next_char();
  return token;
}

auto ALLOWED_ID_CHARS = std::string("+-<>\\/~!$%^;?");
bool is_allowed_id_char(char c) {
  return std::isalpha(c) || ALLOWED_ID_CHARS.contains(c);
}

std::expected<Token, std::string> Lexer::get_token() {
  if (src_index >= (int)source.size() - 1)
    return create_token(EoF);

  // Don't skip new lines.
  if (last_char == '\n')
    return create_token(NewLine, true);

  // Skip whitespace
  while (std::isspace(last_char))
    last_char = next_char();
  
  // Ignore comments
  // TODO: Worth to return comments as tokens for documentation
  if (last_char == '#') {
    last_char = next_char();
    while (last_char != '\n')
      last_char = next_char();
    return create_token(NewLine, true);
  }

  // Identifier
  if (is_allowed_id_char(last_char)) {
    temp_id = last_char;

    capturing = true;
    last_char = next_char();
    while (std::isalpha(last_char) || last_char == '_') {
      temp_id += last_char;
      last_char = next_char();
    }
    capturing = false;

    if (temp_id == "extern")
      return create_token(Extern);

    return create_token(Id);
  }

  // Meta fn
  if (last_char == '@') {
    capturing = true;
    last_char = next_char();
    while (last_char != ' ' && last_char != '\n') {
      temp_id += last_char;
      last_char = next_char();
    }
    capturing = false;

    return create_token(Meta);
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

    // Format temp_id so we replace escaping characters to C equivalents
    std::string formatted_str;
    for (int i = 0; i < (int)temp_id.size(); i++) {
      auto c = temp_id[i];
      if (c == '\\' && i < (int)temp_id.size() - 1) {
        i += 1;
        auto nc = temp_id[i];

        switch (nc) {
        case 'n':
          formatted_str += "\x0A";
          break;

        case 't':
          formatted_str += "\x09";
          break;

        case '"':
          formatted_str += '"';
          break;

        case '\\':
          formatted_str += '\\';
          break;

        default:
          formatted_str += c;
          formatted_str += nc;
          break;
        }
      } else {
        formatted_str += c;
      }
    }

    temp_id = formatted_str;
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

    return create_token(is_float ? Float : Int);
  }

  switch (last_char) {
  case ':':
    return create_token(Colon, true);
  case '.':
    return create_token(Dot, true);
  case ',':
    return create_token(Comma, true);
  case '=':
    return create_token(Eq, true);
  case '|':
    return create_token(Bar, true);
  case '{':
    return create_token(LBrace, true);
  case '}':
    return create_token(RBrace, true);
  case '(':
    return create_token(LPar, true);
  case ')':
    return create_token(RPar, true);
  }

  return create_token(Undefined);
}

std::string token_kind_to_string(TokenKind kind) {
  switch (kind) {
  case EoF:
    return "EoF";
  case Colon:
    return "Colon";
  case Dot:
    return "Dot";
  case Comma:
    return "Comma";
  case Eq:
    return "Eq";
  case LBrace:
    return "LBrace";
  case RBrace:
    return "RBrace";
  case LPar:
    return "LPar";
  case RPar:
    return "RBar";
  case Bar:
    return "Bar";
  case Id:
    return "Identifier";
  case Int:
    return "Int";
  case Float:
    return "Float";
  case String:
    return "String";
  case Extern:
    return "Extern";
  case NewLine:
    return "NewLine";
  case Meta:
    return "Meta";
  case Undefined:
  default:
    return "Undefined";
  }
}

void Token::print_token() {
  std::cout << "[" << position.line << ":" << position.start << "-"
            << position.end << "] " << token_kind_to_string(kind) << " "
            << value << "\n";
}
