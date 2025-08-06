use core::fmt;

use colored::Colorize;
use thiserror::Error;

use crate::{
    lexer::*,
    type_db::AstTypeDb,
    types::{AstType, AstTypeId},
};

#[derive(Debug, Error, Clone)]
pub enum ParserErrorKind {
    #[error("use of undeclared type '{type_name}'")]
    TypeNotFound { type_name: String },

    #[error("{expected} was expected, but {found_kind} was found instead")]
    UnexpectedTokenMultiple {
        expected: String,   // Pre-formatted string of expected tokens
        found_kind: String, // String representation of found token kind
    },

    #[error("{expected} was expected, but {found_kind} was found instead")]
    UnexpectedTokenString {
        expected: String,
        found_kind: String,
    },

    #[error("{expected_kind:?} was expected, but {found_kind} was found instead")]
    UnexpectedTokenKind {
        expected_kind: Vec<TokenKind>,
        found_kind: TokenKind,
    },

    #[error("'{id}' is not defined")]
    UndefinedIdentifier { id: String },

    #[error("enum '{enum_name}' does not contain member '{member}'")]
    UndefinedEnumMember { enum_name: String, member: String },

    #[error(
        "unexpected statement found while parsing an enum variant; only structs and enums are supported"
    )]
    UnsupportedEnumVariant,

    #[error(
        "statement found while parsing an enum is not a struct; only structs and enums are supported"
    )]
    NonStructEnumVariant,

    #[error("function is missing a body definition")]
    FnMissingBody,

    #[error("an argument was expected, but {found_kind} was found instead")]
    ArgumentExpected { found_kind: String },

    #[error("an enum type was expected, but {found_type_name} was found instead")]
    ExpectedTypeEnum { found_type_name: String },

    #[error("'{name}' is not a valid meta function")]
    UndefinedMetaFn { name: String },

    #[error("'{name}' is not a valid function")]
    UndefinedCall { name: String },

    #[error(
        "no overload found for function '{fn_name}' with the given arguments\navailable overloads:\n{overloads:?}"
    )]
    NoOverloadCallMatched {
        fn_name: String,
        overloads: Vec<AstTypeId>,
    },

    #[error("'{name}' is not a valid struct")]
    UndefinedStruct { name: String },

    #[error("an expression was expected here")]
    ExpressionExpected,

    #[error("undefined statement found")]
    UndefinedStatement,

    #[error("unexpectedly reached end of file")]
    EofFound,

    #[error(
        "array with type '{array_type_name}' cannot contain an expression of a different type '{unexpected_type_name}'"
    )]
    MultitypedArray {
        array_type_name: String,
        unexpected_type_name: String,
    },

    #[error("no member '{member}' found for type '{type_name}'")]
    MemberNotFound { member: String, type_name: String },
}

#[derive(Debug, Clone)]
pub struct CompilerError {
    pub pos: FilePos,
    pub kind: ParserErrorKind,
}

impl fmt::Display for CompilerError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.kind) // Delegate formatting to ParserErrorKind
    }
}

impl std::error::Error for CompilerError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        // Delegate to the inner ParserErrorKind's source, if it has one
        self.kind.source()
    }
}

impl CompilerError {
    pub fn new(pos: FilePos, kind: ParserErrorKind) -> Self {
        CompilerError { pos, kind }
    }

    pub fn from_token(tk: &Token, kind: ParserErrorKind) -> Self {
        CompilerError {
            pos: tk.position,
            kind,
        }
    }

    pub fn from_kind(kind: ParserErrorKind) -> Self {
        CompilerError {
            pos: FilePos::default(),
            kind,
        }
    }

    pub fn print_error(&self, source: &str) {
        let pos = &self.pos;
        let lines: Vec<&str> = source.lines().collect();

        let line_idx = pos.line.saturating_sub(1);

        // Get the specific line from the source
        let line_content = lines.get(line_idx).unwrap();

        let line_content_str = line_content.to_string(); // Copy to iterate over chars

        eprintln!("{}{}: {}", "error".bold().red(), "".clear(), self.kind);
        eprintln!(
            "{}",
            format!("---[{} {}-{}]", pos.line, pos.start, pos.end).dimmed()
        );

        // Print the relevant line of code
        eprintln!("{line_content_str}");

        // Print error marker (^)
        eprint!("{}", "".red());
        let mut on_bounds = false;

        // Iterate over characters (not bytes) for correct column calculation with Unicode
        for (i, c) in line_content_str.chars().enumerate() {
            let start_col_0_based = pos.start.saturating_sub(1);
            let end_col_0_based_inclusive = pos.end.saturating_sub(1);

            if i == start_col_0_based {
                on_bounds = true;
            }
            if i > end_col_0_based_inclusive {
                on_bounds = false;
            }

            if on_bounds {
                eprint!("^");
            } else {
                eprint!("{}", if c == '\t' { '\t' } else { ' ' });
            }
        }
        eprintln!("{}", "".clear());
        eprintln!(); // Blank line at the end
    }

    pub fn expression_expected(tk: &Token) -> Self {
        Self {
            pos: tk.position,
            kind: ParserErrorKind::ExpressionExpected,
        }
    }

    pub fn type_not_found(tk: &Token, ty_name: String) -> CompilerError {
        Self {
            pos: tk.position,
            kind: ParserErrorKind::TypeNotFound { type_name: ty_name },
        }
    }

    pub fn expected_type_enum(tk: &Token, ty: &AstType) -> CompilerError {
        Self {
            pos: tk.position,
            kind: ParserErrorKind::ExpectedTypeEnum {
                found_type_name: ty.get_name().to_string(),
            },
        }
    }

    pub fn member_not_found(tk: &Token, ty: &AstType, member: String) -> CompilerError {
        Self {
            pos: tk.position,
            kind: ParserErrorKind::MemberNotFound {
                member,
                type_name: ty.get_name().to_string(),
            },
        }
    }

    pub fn unexpected_token(tk: &Token, expected: &[TokenKind]) -> CompilerError {
        Self {
            pos: tk.position,
            kind: ParserErrorKind::UnexpectedTokenKind {
                expected_kind: expected.to_vec(),
                found_kind: tk.kind,
            },
        }
    }

    pub fn undefined_enum_member(tk: &Token, enum_name: String, member: String) -> CompilerError {
        Self {
            pos: tk.position,
            kind: ParserErrorKind::UndefinedEnumMember { enum_name, member },
        }
    }

    pub fn argument_expected(tk: &Token) -> CompilerError {
        Self {
            pos: tk.position,
            kind: ParserErrorKind::ArgumentExpected {
                found_kind: tk.kind.to_string(),
            },
        }
    }

    pub fn no_overload_call_matched(tk: &Token, type_db: &AstTypeDb, fns: Vec<AstTypeId>) -> Self {
        let first_fn_ty = type_db.get_type(fns[0]).unwrap();
        CompilerError {
            pos: tk.position,
            kind: ParserErrorKind::NoOverloadCallMatched {
                fn_name: first_fn_ty.get_name().to_string(),
                overloads: fns,
            },
        }
    }

    pub fn multityped_array(tk: &Token, array_type: &AstType, unexpected_type: &AstType) -> Self {
        CompilerError {
            pos: tk.position,
            kind: ParserErrorKind::MultitypedArray {
                array_type_name: array_type.get_name().to_string(),
                unexpected_type_name: unexpected_type.get_name().to_string(),
            },
        }
    }

    pub fn undefined_statement(tk: &Token) -> CompilerError {
        Self {
            pos: tk.position,
            kind: ParserErrorKind::UndefinedStatement,
        }
    }
}
