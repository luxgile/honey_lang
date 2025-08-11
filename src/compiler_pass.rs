use std::str::{self, FromStr};

use crate::{
    ast::*,
    ast_typer::AstTyped,
    lexer::{FileRange, ALLOWED_ID_CHARS},
    meta_fn::{self, BinOpKind, CmpOpKind, MetaFnKind},
    program_ctx::ProgramCtx,
    types::{AstTypeId, VOID_TYPE},
};

pub trait CompilerPass<T> {
    fn run(self, ctx: &mut ProgramCtx, file: &FileStmtAst) -> T;
}

