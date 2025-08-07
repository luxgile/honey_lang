use std::fmt::Display;

use lazy_static::lazy_static;

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum TokenKind {
    EoF,
    Dot,
    Comma,
    Colon,
    /// {
    LBrace,
    /// }
    RBrace,
    /// [
    LBracks,
    /// ]
    RBracks,
    /// (
    LPar,
    /// )
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
    Defer,
    Enum,
    Fn,
    If,
    Else,
    Loop,
    Match,
    NewLine,
    Meta,
    Undefined, // Consider removing or making this an error type
}
impl Display for TokenKind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let s = match self {
            TokenKind::Id => "id",
            TokenKind::Colon => ":",
            TokenKind::Dot => ".",
            TokenKind::Comma => ",",
            TokenKind::Bar => "|",
            TokenKind::LPar => "(",
            TokenKind::RPar => ")",
            TokenKind::NewLine => "new line",
            TokenKind::Meta => "@",
            TokenKind::Int => "int",
            TokenKind::Float => "float",
            TokenKind::String => "string",
            TokenKind::Bool => "bool",
            // TokenKind::Eq => "=",
            TokenKind::EoF => "EOF",
            TokenKind::Struct => "struct",
            TokenKind::LBrace => "{",
            TokenKind::RBrace => "}",
            TokenKind::Extern => "extern",
            TokenKind::Fn => "fn",
            TokenKind::Pointy => "^",
            TokenKind::Amper => "&",
            TokenKind::Loop => "loop",
            TokenKind::Match => "match",
            TokenKind::Return => "return",
            TokenKind::Defer => "defer",
            _ => panic!("unimplemented display for token kind"),
        };

        write!(f, "{s}")
    }
}

#[derive(Debug, Default, PartialEq, Clone, Copy)]
pub struct FilePos {
    pub line: usize,
    pub start: usize,
    pub end: usize,
}
impl FilePos {
    pub fn merge(&mut self, fp: FilePos) {
        if self.line != fp.line {
            panic!("unsupported merging of FilePos from different lines");
        }
        self.start = std::cmp::min(self.start, fp.start);
        self.end = std::cmp::min(self.end, fp.end);
    }
}

#[derive(Debug, PartialEq, Clone)]
pub struct Token {
    pub kind: TokenKind,
    pub value: String,
    pub position: FilePos,
}

impl Token {
    pub fn print_token(&self) {
        let mut val = self.value.clone();
        if val == "\n" {
            val = "\\n".to_string();
        }
        println!(
            "[{}:{}-{}] {:?} {}",
            self.position.line, self.position.start, self.position.end, self.kind, val
        );
    }
}
lazy_static! {
    pub static ref ALLOWED_ID_CHARS: Vec<char> = vec![
        '+', '-', '<', '>', '=', '_', '/', '\\', '*', '~', '!', '$', '%', ';', '?',
    ];
}

fn is_allowed_id_char(c: char) -> bool {
    c.is_alphabetic() || ALLOWED_ID_CHARS.contains(&c)
}

fn is_whitespace(c: char) -> bool {
    c != '\n' && c.is_ascii_whitespace()
}

pub struct Lexer {
    source: Vec<char>,
    src_index: usize,
    last_char: char,
    current_pos: FilePos,
    capturing: bool,
    temp_id: String,
}

impl Lexer {
    pub fn new() -> Self {
        Lexer {
            source: Vec::new(),
            src_index: 0,
            last_char: ' ',
            current_pos: FilePos {
                line: 1,
                start: 0,
                end: 0,
            }, // Lines are 1-indexed, cols 0-indexed in C++
            capturing: false,
            temp_id: String::new(),
        }
    }

    pub fn set_source(&mut self, source_code: &str) {
        self.source = source_code.chars().collect();
        self.src_index = 0;
        self.last_char = ' ';
        self.current_pos = FilePos {
            line: 1,
            start: 0,
            end: 0,
        };
        self.capturing = false;
        self.temp_id.clear();

        if !self.source.is_empty() {
            self.last_char = self.next_char().unwrap_or('\0');
        }
    }

    fn next_char(&mut self) -> Option<char> {
        if self.src_index >= self.source.len() {
            self.last_char = '\0';
            return None;
        }

        let c = self.source[self.src_index];
        self.src_index += 1;

        if self.capturing {
            self.current_pos.end += 1;
        } else {
            self.current_pos.start += 1;
            self.current_pos.end = self.current_pos.start;
        }

        if self.last_char == '\n' {
            self.current_pos.line += 1;
            self.current_pos.end = 0;
            if !self.capturing {
                self.current_pos.start = 0;
            }
        }
        self.last_char = c;
        Some(c)
    }

    fn go_back(&mut self, steps: usize) -> Option<char> {
        if self.src_index >= steps {
            self.src_index -= steps;
            if self.capturing {
                self.current_pos.end -= steps;
            } else {
                self.current_pos.start -= steps;
                self.current_pos.end = self.current_pos.start;
            }
            self.last_char = self.source[self.src_index];
            Some(self.last_char)
        } else {
            None // Cannot go back that many steps
        }
    }

    fn create_token(&mut self, kind: TokenKind, pos: FilePos, consume: bool) -> Token {
        let value = if self.temp_id.is_empty() {
            self.last_char.to_string()
        } else {
            self.temp_id.clone()
        };

        let token = Token {
            kind,
            value,
            position: pos,
        };
        self.temp_id.clear(); // Reset temp_id

        if consume {
            self.last_char = self.next_char().unwrap_or('\0'); // Advance last_char
        }
        token
    }

    fn start_capturing(&mut self) {
        self.current_pos.end = self.current_pos.start;
        self.capturing = true;
    }

    fn stop_capturing(&mut self) -> FilePos {
        self.capturing = false;
        let mut pos = self.current_pos;
        if pos.end != 0 {
            pos.end -= 1;
        }
        self.current_pos.start = self.current_pos.end;
        pos
    }

    pub fn get_token(&mut self) -> Token {
        // Skip whitespace
        while is_whitespace(self.last_char) {
            self.next_char(); // last_char is updated by next_char
        }

        // Skip comments
        if self.last_char == '#' {
            self.next_char(); // Consume '#'
            if self.last_char == '+' {
                // Multiline comment
                self.next_char(); // Consume '+'
                while self.src_index < self.source.len() {
                    if self.last_char == '+' {
                        self.next_char(); // Consume '+'
                        if self.last_char == '#' {
                            self.next_char(); // Consume '#'
                            break; // End of multiline comment
                        }
                    }
                    self.next_char(); // Consume next char in comment
                }
                self.next_char();
                return self.get_token();
            } else {
                while self.last_char != '\n' && self.src_index < self.source.len() {
                    self.next_char();
                }
                self.next_char();
                return self.get_token();
            }
        }

        // Don't skip new lines.
        if self.last_char == '\n' {
            let pos = self.current_pos;
            return self.create_token(TokenKind::NewLine, pos, true);
        }

        if self.src_index >= self.source.len() && self.last_char == '\0' {
            return self.create_token(TokenKind::EoF, self.current_pos, false);
        }

        // Identifier/Keywords
        if is_allowed_id_char(self.last_char) {
            self.start_capturing();
            self.temp_id.clear(); // Clear temp_id before building
            self.temp_id.push(self.last_char); // Add the first character

            self.next_char();
            while is_allowed_id_char(self.last_char) || self.last_char.is_numeric() {
                self.temp_id.push(self.last_char);
                self.next_char();
            }
            let pos = self.stop_capturing();

            match self.temp_id.as_str() {
                "true" | "false" => return self.create_token(TokenKind::Bool, pos, false),
                "if" => return self.create_token(TokenKind::If, pos, false),
                "ret" => return self.create_token(TokenKind::Return, pos, false),
                "defer" => return self.create_token(TokenKind::Defer, pos, false),
                "else" => return self.create_token(TokenKind::Else, pos, false),
                "loop" => return self.create_token(TokenKind::Loop, pos, false),
                "match" => return self.create_token(TokenKind::Match, pos, false),
                "extern" => return self.create_token(TokenKind::Extern, pos, false),
                "struct" => return self.create_token(TokenKind::Struct, pos, false),
                "enum" => return self.create_token(TokenKind::Enum, pos, false),
                "fn" => return self.create_token(TokenKind::Fn, pos, false),
                _ => {
                    return self.create_token(TokenKind::Id, pos, false);
                }
            }
        }

        // Meta fn
        if self.last_char == '@' {
            self.start_capturing();
            self.temp_id.clear();
            self.next_char(); // Consume '@'
            while self.last_char != ' ' && self.last_char != '\n' && self.last_char != '\0' {
                self.temp_id.push(self.last_char);
                self.next_char();
            }
            let pos = self.stop_capturing();
            return self.create_token(TokenKind::Meta, pos, false);
        }

        // String
        if self.last_char == '"' {
            self.next_char();
            self.temp_id.clear();

            self.start_capturing();
            while self.last_char != '"' && self.last_char != '\0' {
                self.temp_id.push(self.last_char);
                self.next_char();
            }
            let pos = self.stop_capturing();
            self.next_char();

            // Format escaped characters
            let mut formatted_str = String::new();
            let mut chars = self.temp_id.chars().peekable();
            while let Some(c) = chars.next() {
                if c == '\\' {
                    if let Some(next_c) = chars.next() {
                        match next_c {
                            'n' => formatted_str.push('\n'),
                            't' => formatted_str.push('\t'),
                            '"' => formatted_str.push('"'),
                            '\\' => formatted_str.push('\\'),
                            _ => {
                                formatted_str.push(c);
                                formatted_str.push(next_c);
                            }
                        }
                    } else {
                        formatted_str.push(c);
                    }
                } else {
                    formatted_str.push(c);
                }
            }

            self.temp_id = formatted_str;
            return self.create_token(TokenKind::String, pos, false); // Already consumed closing '"'
        }

        // Numbers
        if self.last_char.is_ascii_digit() {
            self.temp_id.clear();
            self.temp_id.push(self.last_char);

            self.start_capturing();
            let mut is_float = false;

            self.next_char();
            while self.last_char.is_ascii_digit() || self.last_char == '.' {
                if self.last_char == '.' {
                    if is_float {
                        panic!("multiple points found lexing a float");
                    }
                    is_float = true;
                }
                self.temp_id.push(self.last_char);
                self.next_char();
            }
            let pos = self.stop_capturing();

            return self.create_token(
                if is_float {
                    TokenKind::Float
                } else {
                    TokenKind::Int
                },
                pos,
                false,
            );
        }

        let token_kind = match self.last_char {
            ':' => TokenKind::Colon,
            '.' => TokenKind::Dot,
            ',' => TokenKind::Comma,
            '|' => TokenKind::Bar,
            '{' => TokenKind::LBrace,
            '}' => TokenKind::RBrace,
            '(' => TokenKind::LPar,
            ')' => TokenKind::RPar,
            '[' => TokenKind::LBracks,
            ']' => TokenKind::RBracks,
            '^' => TokenKind::Pointy,
            '&' => TokenKind::Amper,
            _ => {
                panic!("token '{}' undefined", self.last_char);
            }
        };

        // For single-character tokens, consume the character.
        // `create_token` with `consume=true` handles the `next_char()` call.
        self.create_token(token_kind, self.current_pos, true)
    }
}

#[cfg(test)]
mod lexer_tests {
    use super::*;

    fn kinds(source: &str) -> Vec<TokenKind> {
        let mut lexer = Lexer::new();
        lexer.set_source(source);
        let mut tokens = Vec::new();
        loop {
            let tk = lexer.get_token();
            if tk.kind == TokenKind::EoF {
                break;
            }
            tokens.push(tk.kind);
        }
        tokens
    }

    fn tokens(source: &str) -> Vec<Token> {
        let mut lexer = Lexer::new();
        lexer.set_source(source);
        let mut tokens = Vec::new();
        loop {
            let tk = lexer.get_token();
            if tk.kind == TokenKind::EoF {
                break;
            }
            tokens.push(tk);
        }
        tokens
    }

    #[test]
    fn read_new_lines() {
        assert_eq!(kinds("\n"), vec![TokenKind::NewLine]);
        assert_eq!(kinds("\n\n"), vec![TokenKind::NewLine, TokenKind::NewLine]);
        assert_eq!(kinds("\n \n"), vec![TokenKind::NewLine, TokenKind::NewLine]);
        assert_eq!(
            kinds("\n \n "),
            vec![TokenKind::NewLine, TokenKind::NewLine]
        );
        assert_eq!(
            kinds(" \n \t \n "),
            vec![TokenKind::NewLine, TokenKind::NewLine]
        );
    }

    #[test]
    fn read_single_tokens() {
        assert_eq!(kinds(":"), vec![TokenKind::Colon]);
        assert_eq!(kinds("."), vec![TokenKind::Dot]);
        assert_eq!(kinds(","), vec![TokenKind::Comma]);
        assert_eq!(kinds("|"), vec![TokenKind::Bar]);
        assert_eq!(kinds("("), vec![TokenKind::LPar]);
        assert_eq!(kinds(")"), vec![TokenKind::RPar]);
        assert_eq!(kinds("{"), vec![TokenKind::LBrace]);
        assert_eq!(kinds("}"), vec![TokenKind::RBrace]);
        assert_eq!(kinds("["), vec![TokenKind::LBracks]);
        assert_eq!(kinds("]"), vec![TokenKind::RBracks]);
    }

    #[test]
    fn ignore_comments() {
        assert_eq!(kinds("#This is a comment"), vec![]);
        assert_eq!(kinds(": #comment , #: #. #[] "), vec![TokenKind::Colon]);
        assert_eq!(
            kinds(": #+comment+# , "),
            vec![TokenKind::Colon, TokenKind::Comma]
        );
        assert_eq!(
            kinds(": #+comment \n comment 2 \n comment 3+#"),
            vec![TokenKind::Colon]
        );
        assert_eq!(
            kinds(": #+comment \n #inner comment \n comment 2+# . "),
            vec![TokenKind::Colon, TokenKind::Dot]
        );
    }

    #[test]
    fn keywords() {
        assert_eq!(kinds("true"), vec![TokenKind::Bool]);
        assert_eq!(kinds("false"), vec![TokenKind::Bool]);
        assert_eq!(kinds("if"), vec![TokenKind::If]);
        assert_eq!(kinds("ret"), vec![TokenKind::Return]);
        assert_eq!(kinds("else"), vec![TokenKind::Else]);
        assert_eq!(kinds("loop"), vec![TokenKind::Loop]);
        assert_eq!(kinds("match"), vec![TokenKind::Match]);
        assert_eq!(kinds("extern"), vec![TokenKind::Extern]);
        assert_eq!(kinds("struct"), vec![TokenKind::Struct]);
        assert_eq!(kinds("enum"), vec![TokenKind::Enum]);
        assert_eq!(kinds("fn"), vec![TokenKind::Fn]);
    }

    #[test]
    fn identifiers() {
        assert_eq!(kinds("myvar"), vec![TokenKind::Id]);
        assert_eq!(kinds("my_var"), vec![TokenKind::Id]);
        assert_eq!(kinds("my_var2"), vec![TokenKind::Id]);
        assert_eq!(kinds("my-var"), vec![TokenKind::Id]);
        assert_eq!(kinds("+/-*"), vec![TokenKind::Id]);
    }

    #[test]
    fn pointy() {
        let tks = tokens("^my_var");
        assert_eq!(tks.len(), 2);
        assert_eq!(tks[0].kind, TokenKind::Pointy);
        assert_eq!(tks[1].kind, TokenKind::Id);
        assert_eq!(tks[1].value, "my_var");
    }

    #[test]
    fn ampersand() {
        let tks = tokens("&my_var");
        assert_eq!(tks.len(), 2);
        assert_eq!(tks[0].kind, TokenKind::Amper);
        assert_eq!(tks[1].kind, TokenKind::Id);
        assert_eq!(tks[1].value, "my_var");
    }

    #[test]
    fn strings() {
        let tks = tokens("\"my string\"");
        assert_eq!(tks.len(), 1);
        assert_eq!(tks[0].kind, TokenKind::String);
        assert_eq!(tks[0].value, "my string");
    }

    #[test]
    fn numbers() {
        assert_eq!(kinds("10"), vec![TokenKind::Int]);
        assert_eq!(kinds("10.5"), vec![TokenKind::Float]);
    }

    #[test]
    #[should_panic]
    fn float_double_pointer_error() {
        tokens("10..5");
    }
}
