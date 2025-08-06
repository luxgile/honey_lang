#![allow(dead_code)]

use std::{
    io::Write, process::{Command, ExitStatus, Stdio}
};

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
    pub fn run_file(filename: String) -> Result<ExitStatus, ()> {
        let source_code: String =
            std::fs::read_to_string(filename.clone()).expect("error reading file");
        Compiler::run_src(source_code, filename)
    }

    pub fn run_src(src: String, exe_name: String) -> Result<ExitStatus, ()> {
        let std_src = String::from_utf8_lossy(include_bytes!("std.hun")).into_owned();
        let hun_src = std_src + &src;

        let mut program_ctx = ProgramCtx::new();
        Compiler::add_meta_definitions(&mut program_ctx);
        let (file, errors) = {
            let mut parser = Parser::new(&mut program_ctx);
            (parser.parse_file(&hun_src, false), parser.errors.clone())
        };

        // println!();
        // file.print_ast(&program_ctx, 0);

        if !errors.is_empty() {
            println!();
            errors.iter().for_each(|e| e.print_error(&hun_src));
            return Err(());
        }

        let transpiler = CTranspilerPass::default();
        let c_src = transpiler.run(&program_ctx, &file);
        let mut line = -1;
        let c_src_debug: String = c_src
            .lines()
            .map(|x| {
                line += 1;
                format!("{line}  ") + x + "\n"
            })
            .collect();
        println!();
        println!("{c_src_debug}");

        let mut gcc = Command::new("gcc")
            .args(["-x", "c", "-", "-o", &exe_name])
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
            let run_cmd = Command::new(format!("./{exe_name}"))
                .stdin(Stdio::inherit())
                .stderr(Stdio::inherit())
                .stdout(Stdio::inherit())
                .spawn()
                .unwrap();
            let run = run_cmd.wait_with_output().unwrap();
            let run_str = String::from_utf8_lossy(&run.stdout);
            println!("{}", &run_str);

            let _rm = Command::new("rm").args([exe_name]).output().unwrap();
            Ok(run.status)
        } else {
            println!("\ncompilation failed:");
            println!("{}", String::from_utf8_lossy(&output.stdout));
            println!("{}", String::from_utf8_lossy(&output.stderr));
            Err(())
        }
    }

    fn add_meta_definitions(c: &mut ProgramCtx) {
        let def_bin = |c: &mut ProgramCtx, id: &str, op: BinOpKind, ret: MetaReturnType| {
            c.define_meta(MetaFn::new(
                id.to_string(),
                MetaFnKind::BinOp(op),
                ret,
            ))
        };

        let def_cmp = |c: &mut ProgramCtx, id: &str, op: CmpOpKind, ret: MetaReturnType| {
            c.define_meta(MetaFn::new(
                id.to_string(),
                MetaFnKind::CmpOp(op),
                ret,
            ))
        };

        let int = MetaReturnType::Int;
        let float = MetaReturnType::Float;
        let bool = MetaReturnType::Type(BOOL_TYPE.get_id());

        def_bin(c, "i+", BinOpKind::Add, int);
        def_bin(c, "i-", BinOpKind::Minus, int);
        def_bin(c, "i*", BinOpKind::Mult, int);
        def_bin(c, "i/", BinOpKind::Div, int);
        def_bin(c, "i%", BinOpKind::Rem, int);
        def_cmp(c, "i==", CmpOpKind::Eq, int);
        def_cmp(c, "i!=", CmpOpKind::Ne, int);
        def_cmp(c, "i<", CmpOpKind::Less, int);
        def_cmp(c, "i<=", CmpOpKind::LessEq, int);
        def_cmp(c, "i>", CmpOpKind::Greater, int);
        def_cmp(c, "i>=", CmpOpKind::GreaterEq, int);

        def_bin(c, "f+", BinOpKind::Add, float);
        def_bin(c, "f-", BinOpKind::Minus, float);
        def_bin(c, "f*", BinOpKind::Mult, float);
        def_bin(c, "f/", BinOpKind::Div, float);
        def_bin(c, "f%", BinOpKind::Rem, float);
        def_cmp(c, "f==", CmpOpKind::Eq, float);
        def_cmp(c, "f!=", CmpOpKind::Ne, float);
        def_cmp(c, "f<", CmpOpKind::Less, float);
        def_cmp(c, "f<=", CmpOpKind::LessEq, float);
        def_cmp(c, "f>", CmpOpKind::Greater, float);
        def_cmp(c, "f>=", CmpOpKind::GreaterEq, float);

        def_bin(c, "b&&", BinOpKind::And, bool);
        def_bin(c, "b||)", BinOpKind::Or, bool);
        def_cmp(c, "b==", CmpOpKind::Eq, bool);
        def_cmp(c, "b!=", CmpOpKind::Ne, bool);

        def_bin(c, ">>", BinOpKind::LShr, int);
        def_bin(c, "<<", BinOpKind::Shl, int);
    }
}
