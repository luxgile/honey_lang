#include "lexer.h"
#include <cctype>
#include <iostream>
#include <print>
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

  if (last_char == '\n') {
    current_pos.line += 1;
    current_pos.end = 0;
    if (!capturing)
      current_pos.start = 0;
  }
  return c;
}

int Lexer::go_back(int steps) {
  src_index -= steps;
  if (capturing)
    current_pos.end -= steps;
  else {
    current_pos.start -= steps;
    current_pos.end = current_pos.start;
  }
  return source[src_index];
}

Token Lexer::create_token(TokenKind kind, FilePos pos, bool consume) {
  if (temp_id == "")
    temp_id = last_char;
  Token token = Token{kind, std::string(temp_id), pos};
  temp_id = "";
  if (consume)
    last_char = next_char();
  return token;
}

auto ALLOWED_ID_CHARS = std::string("+-<>=_\\/*~!$%^;?&");
bool is_allowed_id_char(char c) {
  return std::isalpha(c) || ALLOWED_ID_CHARS.contains(c);
}

bool is_whitespace(char c) { return c != '\n' && std::isspace(c); }

Token Lexer::get_token() {
  if (src_index >= (int)source.size() - 1)
    return create_token(EoF, current_pos);
  //
  // Skip whitespace
  while (is_whitespace(last_char))
    last_char = next_char();

  // Don't skip new lines.
  if (last_char == '\n')
    return create_token(NewLine, current_pos, true);

  // Ignore comments
  // TODO: Worth to return comments as tokens for documentation
  if (last_char == '#') {
    last_char = next_char();
    if (last_char == '+') { // Multiline command
      last_char = next_char();
      while (src_index < (int)source.size() - 1) {
        if (last_char == '+') {
          last_char = next_char();
          if (last_char == '#')
            break;
        }
        last_char = next_char();
      }
      return create_token(NewLine, current_pos, true);
    } else { // Line command
      while (last_char != '\n')
        last_char = next_char();
      return create_token(NewLine, current_pos, true);
    }
  }

  // Identifier
  if (is_allowed_id_char(last_char)) {
    temp_id = last_char;

    start_capturing();
    last_char = next_char();
    while (is_allowed_id_char(last_char)) {
      temp_id += last_char;
      last_char = next_char();
    }
    auto pos = stop_capturing();

    if (temp_id == "true" || temp_id == "false")
      return create_token(Bool, pos);

    if (temp_id == "if")
      return create_token(If, pos);

    if (temp_id == "ret")
      return create_token(Return, pos);

    if (temp_id == "else")
      return create_token(Else, pos);

    if (temp_id == "loop")
      return create_token(Loop, pos);

    if (temp_id == "match")
      return create_token(Match, pos);

    if (temp_id == "extern")
      return create_token(Extern, pos);

    if (temp_id == "struct")
      return create_token(Struct, pos);

    if (temp_id == "enum")
      return create_token(Enum, pos);

    if (temp_id == "fn")
      return create_token(Fn, pos);

    if (temp_id.starts_with('&')) {
      last_char = go_back(temp_id.size() - 1);
      temp_id = temp_id[0];
      return create_token(Amper, pos);
    }

    if (temp_id.starts_with('^')) {
      last_char = go_back(temp_id.size() - 1);
      temp_id = temp_id[0];
      return create_token(Pointy, pos);
    }

    return create_token(Id, pos);
  }

  // Meta fn
  if (last_char == '@') {
    start_capturing();
    last_char = next_char();
    while (last_char != ' ' && last_char != '\n') {
      temp_id += last_char;
      last_char = next_char();
    }
    auto pos = stop_capturing();

    return create_token(Meta, pos);
  }

  // String
  if (last_char == '"') {
    last_char = next_char();
    temp_id = last_char;

    start_capturing();
    last_char = next_char();
    while (last_char != '"') {
      temp_id += last_char;
      last_char = next_char();
    }
    auto pos = stop_capturing();

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
    return create_token(String, pos, true);
  }

  // Numbers
  if (std::isdigit(last_char)) {
    temp_id = last_char;
    bool is_float = false;

    start_capturing();
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
    auto pos = stop_capturing();

    return create_token(is_float ? Float : Int, pos);
  }

  switch (last_char) {
  case ':':
    return create_token(Colon, current_pos, true);
  case '.':
    return create_token(Dot, current_pos, true);
  case ',':
    return create_token(Comma, current_pos, true);
  /* case '=': */
  /*   return create_token(Eq, true); */
  case '|':
    return create_token(Bar, current_pos, true);
  case '{':
    return create_token(LBrace, current_pos, true);
  case '}':
    return create_token(RBrace, current_pos, true);
  case '(':
    return create_token(LPar, current_pos, true);
  case ')':
    return create_token(RPar, current_pos, true);
  case '[':
    return create_token(LBracks, current_pos, true);
  case ']':
    return create_token(RBracks, current_pos, true);
  }

  std::println("undefined token found: '{}'", last_char);
  throw "undefined";
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
  /* case Eq: */
  /*   return "Eq"; */
  case LBrace:
    return "LBrace";
  case RBrace:
    return "RBrace";
  case LPar:
    return "LPar";
  case RPar:
    return "RBar";
  case LBracks:
    return "LBracks";
  case RBracks:
    return "RBracks";
  case Bar:
    return "Bar";
  case Amper:
    return "Amper";
  case Pointy:
    return "Pointy";
  case Id:
    return "Identifier";
  case Int:
    return "Int";
  case Float:
    return "Float";
  case Bool:
    return "Bool";
  case String:
    return "String";
  case Extern:
    return "Extern";
  case Struct:
    return "Struct";
  case Enum:
    return "Enum";
  case Fn:
    return "Fn";
  case Match:
    return "Match";
  case Return:
    return "Return";
  case If:
    return "If";
  case Else:
    return "Else";
  case Loop:
    return "Loop";
  case NewLine:
    return "NewLine";
  case Meta:
    return "Meta";
  case Undefined:
  default:
    throw "Undefined";
  }
}

void Token::print_token() {
  auto val = value;
  if (val == "\n")
    val = "\\n";
  std::println("[{}:{}-{}] {} {}", position.line, position.start, position.end,
               token_kind_to_string(kind), val);
}
