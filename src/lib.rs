#![allow(dead_code)]

use std::{
    io::Write,
    process::{Command, Stdio},
};

use ast_printer::AstPrint;
use compiler_pass::{CTranspilerPass, CompilerPass};
use meta_fn::*;
use parser::Parser;
use program_ctx::*;
use types::*;

mod ast;
mod ast_printer;
mod ast_typer;
mod compiler_pass;
mod errors;
mod lexer;
mod meta_fn;
mod parser;
mod program_ctx;
mod type_db;
mod types;

pub struct Compiler;
impl Compiler {
    pub fn run_file(filename: String) {
        let source_code: String = std::fs::read_to_string(filename).expect("error reading file");
        Compiler::run_src(source_code);
    }

    pub fn run_src(src: String) {
        let mut program_ctx = ProgramCtx::new();
        Compiler::add_meta_definitions(&mut program_ctx);
        let (file, errors) = {
            let mut parser = Parser::new(&mut program_ctx);
            (parser.parse_file(&src, false), parser.errors.clone())
        };

        // println!();
        // file.print_ast(&program_ctx, 0);

        if !errors.is_empty() {
            println!();
            errors.iter().for_each(|e| e.print_error(src.as_str()));
            return;
        }

        let transpiler = CTranspilerPass::default();
        let c_src = transpiler.run(&program_ctx, &file);
        let mut line = -1;
        let c_src_debug : String = c_src
            .lines()
            .map(|x| {
                line += 1;
                format!("{}  ", line) + x + "\n"
            })
            .collect();
        println!();
        println!("{}", c_src_debug);

        let mut gcc = Command::new("gcc")
            .args(["-x", "c", "-", "-o", "honey"])
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
            .expect("failed to compile c code with gcc");

        if let Some(mut stdin) = gcc.stdin.take() {
            stdin.write_all(c_src.as_bytes()).unwrap();
        }

        let output = gcc.wait_with_output().unwrap();
        if output.status.success() {
            println!("\nhun code compiled successfully");
            println!("\nrunning code...");
            let run_cmd = Command::new("./honey")
                .stdin(Stdio::inherit())
                .stderr(Stdio::inherit())
                .stdout(Stdio::inherit())
                .spawn()
                .unwrap();
            let run = run_cmd.wait_with_output().unwrap();
            println!("{}", String::from_utf8_lossy(&run.stdout));

            let _rm = Command::new("rm").args(["honey"]).output().unwrap();
        } else {
            println!("\ncompilation failed:");
            println!("{}", String::from_utf8_lossy(&output.stdout));
            println!("{}", String::from_utf8_lossy(&output.stderr));
        }
    }

    fn add_meta_definitions(c: &mut ProgramCtx) {
        let def_bin = |c: &mut ProgramCtx, id: &str, op: BinOpKind, ret: &AstType| {
            c.define_meta(MetaFn::new(
                id.to_string(),
                MetaFnKind::BinOp(op),
                ret.get_id(),
            ))
        };

        let def_cmp = |c: &mut ProgramCtx, id: &str, op: CmpOpKind, ret: &AstType| {
            c.define_meta(MetaFn::new(
                id.to_string(),
                MetaFnKind::CmpOp(op),
                ret.get_id(),
            ))
        };

        def_bin(c, "i+", BinOpKind::Add, &INT_TYPE);
        def_bin(c, "i-", BinOpKind::Minus, &INT_TYPE);
        def_bin(c, "i*", BinOpKind::Mult, &INT_TYPE);
        def_bin(c, "i/", BinOpKind::Div, &INT_TYPE);
        def_bin(c, "i%", BinOpKind::Rem, &INT_TYPE);
        def_cmp(c, "i==", CmpOpKind::Eq, &INT_TYPE);
        def_cmp(c, "i!=", CmpOpKind::Ne, &INT_TYPE);
        def_cmp(c, "i<", CmpOpKind::Less, &INT_TYPE);
        def_cmp(c, "i<=", CmpOpKind::LessEq, &INT_TYPE);
        def_cmp(c, "i>", CmpOpKind::Greater, &INT_TYPE);
        def_cmp(c, "i>=", CmpOpKind::GreaterEq, &INT_TYPE);

        def_bin(c, "f+", BinOpKind::Add, &FLOAT_TYPE);
        def_bin(c, "f-", BinOpKind::Minus, &FLOAT_TYPE);
        def_bin(c, "f*", BinOpKind::Mult, &FLOAT_TYPE);
        def_bin(c, "f/", BinOpKind::Div, &FLOAT_TYPE);
        def_bin(c, "f%", BinOpKind::Rem, &FLOAT_TYPE);
        def_cmp(c, "f==", CmpOpKind::Eq, &FLOAT_TYPE);
        def_cmp(c, "f!=", CmpOpKind::Ne, &FLOAT_TYPE);
        def_cmp(c, "f<", CmpOpKind::Less, &FLOAT_TYPE);
        def_cmp(c, "f<=", CmpOpKind::LessEq, &FLOAT_TYPE);
        def_cmp(c, "f>", CmpOpKind::Greater, &FLOAT_TYPE);
        def_cmp(c, "f>=", CmpOpKind::GreaterEq, &FLOAT_TYPE);

        def_bin(c, "b&&", BinOpKind::And, &BOOL_TYPE);
        def_bin(c, "b||)", BinOpKind::Or, &BOOL_TYPE);
        def_cmp(c, "b==", CmpOpKind::Eq, &BOOL_TYPE);
        def_cmp(c, "b!=", CmpOpKind::Ne, &BOOL_TYPE);

        def_bin(c, ">>", BinOpKind::LShr, &INT_TYPE);
        def_bin(c, "<<", BinOpKind::Shl, &INT_TYPE);
    }
}
