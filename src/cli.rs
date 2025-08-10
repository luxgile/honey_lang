#![allow(dead_code)]

use std::path::Path;

use clap::{Args, Parser, Subcommand, command};
use honeyc_lib::{CompConfig, Compiler};

#[derive(Parser)]
#[command(version, about, long_about = None)]
struct CompilerArgs {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Args, Clone)]
struct BuildArgs {
    pub file_path: String,

    #[arg(long)]
    pub print_c: bool,

    #[arg(long)]
    pub pretty_print: bool,
}

#[derive(Subcommand, Clone)]
enum Commands {
    Build {
        #[command(flatten)]
        args: BuildArgs,
    },
    Run {
        #[command(flatten)]
        args: BuildArgs,
    },
}

fn main() {
    let args = CompilerArgs::parse();
    match args.command {
        Commands::Build { args } => {
            Compiler::build_file(
                Path::new(&args.file_path),
                CompConfig {
                    pretty_print_ast: args.pretty_print,
                    build_path: None,
                },
            )
            .expect("issue building file");
        }
        Commands::Run { args } => {
            let build = Compiler::build_file(
                Path::new(&args.file_path),
                CompConfig {
                    pretty_print_ast: args.pretty_print,
                    build_path: None,
                },
            )
            .unwrap();
            Compiler::run_build(&build);
        }
    }
}
