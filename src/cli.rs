#![allow(dead_code)]

use std::path::Path;

use clap::{Parser, Subcommand, command};
use hunc_lib::{CompConfig, Compiler};

#[derive(Parser)]
#[command(version, about, long_about = None)]
struct Args {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand, Clone)]
enum Commands {
    Build {
        filename: String,
    },
    Run {
        file_path: String,
        
        #[arg(long)]
        print_c: bool,

        #[arg(long)]
        pretty_print: bool,
    },
}

fn main() {
    let args = Args::parse();
    match args.command {
        Commands::Build { filename: _ } => {
            todo!();
        }
        Commands::Run { file_path, print_c, pretty_print } => {
            Compiler::run_file(Path::new(&file_path), &CompConfig { print_c, pretty_print }).unwrap();
        }
    }
}
