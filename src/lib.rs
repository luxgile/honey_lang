#![allow(dead_code)]

use ast::ImportStmtAst;
use colored::Colorize;
use ctranspiler::{CTranspilerPass, TranspilerResult};
use std::{
    env,
    fs::{self},
    io::{self, Write},
    path::{Path, PathBuf},
    process::{Command, ExitStatus, Stdio},
    str::FromStr,
};

use ast_printer::AstPrint;
use compiler_pass::CompilerPass;
use meta_fn::*;
use parser::Parser;
use program_ctx::*;
use types::*;

mod ast;
mod ast_pos;
mod ast_printer;
mod ast_typer;
mod compiler_pass;
mod ctranspiler;
mod errors;
mod lexer;
mod meta_fn;
mod parser;
mod program_ctx;
mod type_db;
mod types;

#[derive(Default, Clone, Debug)]
pub struct CompConfig {
    pub pretty_print_ast: bool,
    pub build_path: Option<PathBuf>,
}

#[derive(Debug)]
pub enum BuildError {
    FileDoesNotExist,
    CompilationFailed,
    GccFailed,
}

pub struct FileBuildInfo {
    build_path: PathBuf,
    exe_path: PathBuf,
}

pub struct Compiler;
impl Compiler {
    pub fn build_file(file_path: &Path, config: CompConfig) -> Result<FileBuildInfo, BuildError> {
        let source_code = fs::read_to_string(file_path).unwrap();

        let mut _config = config;
        if _config.build_path.is_none() {
            _config.build_path = Some(file_path.parent().unwrap().join(".hun_build"));
        }

        Compiler::build_src(
            source_code,
            file_path.file_stem().unwrap().to_str().unwrap(),
            _config,
        )
    }

    pub fn build_src(
        src: String,
        exe_name: &str,
        config: CompConfig,
    ) -> Result<FileBuildInfo, BuildError> {
        let mut program_ctx = ProgramCtx::new();
        Compiler::add_meta_definitions(&mut program_ctx);
        let mut errors = Vec::new();

        // Parse core files
        // TODO: Find a way to precompile these and add them as a dependency to reduce compile time
        // (not a priority atm)
        let core_file_paths = &[("core", "honey/core.hun")];
        let core_files: Vec<_> = core_file_paths
            .iter()
            .map(|(name, path)| {
                let std_src = fs::read_to_string(path).expect("could not read core.hun");
                let mut parser = Parser::new(&mut program_ctx);
                let file_parsed = parser.parse_source(name, &std_src, false);
                let core_errors = parser.get_errors();
                if !core_errors.is_empty() {
                    core_errors.iter().for_each(|e| e.print_error(&std_src));
                    panic!("found errors compiling core files");
                }
                (file_parsed, path)
            })
            .collect();

        // Parse source
        let (mut hun_file, hun_errors) = {
            let mut parser = Parser::new(&mut program_ctx);
            (
                parser.parse_source(exe_name, &src, false),
                parser.get_errors(),
            )
        };
        errors.extend_from_slice(&hun_errors);

        // HACK: Insert the core file as an import by default
        for (file, path) in core_files {
            hun_file.statements.insert(
                0,
                ast::AstStatement::Import(Box::new(ImportStmtAst {
                    id: None,
                    path: path.to_string(),
                    ast: file,
                })),
            );
        }

        if config.pretty_print_ast {
            println!();
            hun_file.print_ast(&program_ctx, 0);
        }

        // Print errors if any found
        if !errors.is_empty() {
            println!();
            errors.iter().for_each(|e| e.print_error(&src));
            return Err(BuildError::CompilationFailed);
        }

        // Transpile to C
        let transpiler = CTranspilerPass::default();
        let result = transpiler.run(&mut program_ctx, &hun_file);

        // Ensuring build folder exists
        let build_path = config
            .build_path
            .clone()
            .unwrap_or(env::current_dir().unwrap());

        if !build_path.exists() {
            fs::create_dir(&build_path).expect("issue creating build path");
        }

        // Create C files
        let src_paths = Compiler::create_c_files(&result, &build_path)
            .iter()
            .map(|x| x.to_string_lossy().into())
            .collect::<Vec<String>>();

        Compiler::copy_std_headers_to_build_path(&build_path);

        // Build C source using gcc
        let exe_path = build_path.as_path().join(exe_name);
        let mut args = vec!["-x", "c"];
        src_paths.iter().for_each(|x| args.push(x));
        args.extend_from_slice(&["-L", "std/build"]);
        args.extend_from_slice(&["-l", "honey_std"]);
        args.push("-o");
        args.push(exe_path.to_str().unwrap());

        let gcc = Command::new("gcc")
            .args(&args)
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
            .expect("failed to compile c code with gcc");

        let output = gcc.wait_with_output().unwrap();
        if output.status.success() {
            Ok(FileBuildInfo {
                build_path,
                exe_path,
            })
        } else {
            println!("{}", String::from_utf8_lossy(&output.stdout));
            println!("{}", String::from_utf8_lossy(&output.stderr));
            Err(BuildError::GccFailed)
        }
    }

    pub fn run_build(build: &FileBuildInfo) -> ExitStatus {
        println!("honey compilation - {}", "success".green().bold());
        let run_cmd = Command::new(format!("./{}", build.exe_path.to_str().unwrap()))
            .stdin(Stdio::inherit())
            .stderr(Stdio::inherit())
            .stdout(Stdio::inherit())
            .spawn()
            .unwrap();
        let run = run_cmd.wait_with_output().unwrap();
        let run_str = String::from_utf8_lossy(&run.stdout);
        println!("{}", &run_str);
        run.status
    }

    pub fn create_c_files(
        transpile_result: &TranspilerResult,
        build_path: &PathBuf,
    ) -> Vec<PathBuf> {
        let mut src_paths = Vec::new();
        let src_path = build_path
            .as_path()
            .join(format!("{}.c", transpile_result.name));
        let header_path = build_path
            .as_path()
            .join(format!("{}.h", transpile_result.name));
        fs::write(src_path.clone(), &transpile_result.source).expect("error creating source file");
        fs::write(header_path.clone(), &transpile_result.header)
            .expect("error creating header file");

        src_paths.push(src_path);

        transpile_result
            .imports
            .iter()
            .for_each(|x| src_paths.extend_from_slice(&Compiler::create_c_files(x, build_path)));
        src_paths
    }

    fn copy_std_headers_to_build_path(build_path: &Path) {
        let mut headers = Vec::new();
        for file in fs::read_dir("./std").unwrap() {
            let file = file.unwrap();
            if let Some(extension) = file.path().extension()
                && extension == "h"
            {
                headers.push(file);
            }
        }

        if !fs::exists(build_path.join("std/")).unwrap() {
            fs::create_dir(build_path.join("std/"))
                .expect("error creating std folder in build file");
        }

        for header in headers {
            let header_path =
                build_path.join(format!("std/{}", header.file_name().into_string().unwrap()));
            fs::copy(header.path(), header_path).expect("error copying header");
        }
    }

    fn add_meta_definitions(c: &mut ProgramCtx) {
        let def_bin = |c: &mut ProgramCtx, id: &str, op: BinOpKind| {
            c.define_meta(MetaFn::new(id.to_string(), MetaFnKind::BinOp(op)))
        };

        let def_cmp = |c: &mut ProgramCtx, id: &str, op: CmpOpKind| {
            c.define_meta(MetaFn::new(id.to_string(), MetaFnKind::CmpOp(op)))
        };

        c.define_meta(MetaFn::new("cast", MetaFnKind::Cast));

        c.define_meta(MetaFn::new("include", MetaFnKind::Include));

        // All binary operations
        def_bin(c, "+", BinOpKind::Add);
        def_bin(c, "-", BinOpKind::Minus);
        def_bin(c, "*", BinOpKind::Mult);
        def_bin(c, "/", BinOpKind::Div);
        def_bin(c, "%", BinOpKind::Rem);
        def_cmp(c, "==", CmpOpKind::Eq);
        def_cmp(c, "!=", CmpOpKind::Ne);
        def_cmp(c, "<", CmpOpKind::Less);
        def_cmp(c, "<=", CmpOpKind::LessEq);
        def_cmp(c, ">", CmpOpKind::Greater);
        def_cmp(c, ">=", CmpOpKind::GreaterEq);

        def_bin(c, "&&", BinOpKind::And);
        def_bin(c, "||", BinOpKind::Or);
        def_cmp(c, "==", CmpOpKind::Eq);
        def_cmp(c, "!=", CmpOpKind::Ne);

        def_bin(c, ">>", BinOpKind::LShr);
        def_bin(c, "<<", BinOpKind::Shl);
    }
}
