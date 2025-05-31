#include "lexer.h"
#include <fstream>
#include <iostream>

int main() {
  std::ifstream honey_file{"honey/main.hun", std::ios::in};
  if (!honey_file.is_open()) return 1;
  
  std::string honey_source{std::istreambuf_iterator<char>(honey_file), std::istreambuf_iterator<char>()};

  Lexer lexer{honey_source};


  Token token;
  do {
    auto tkn_result = lexer.get_token();
    if (!tkn_result) {
      std::cerr << "Failed tokenizing at " << lexer.current_pos.line << " - " << lexer.current_pos.start << "\n";
      return 1;
    }
    token = *tkn_result;
    
    std::cout << "[" << token.position.line << ":" << token.position.start << "] " << token.kind << " " << token.value << "\n";

  } while (token.kind != TokenKind::EoF && token.kind != TokenKind::Undefined);

  return 0;
}
