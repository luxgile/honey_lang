#![allow(dead_code)]

use std::env;

use honey_bootstrapper_lib::Compiler;

fn main() {
    let args = env::args().collect::<Vec<String>>();
    if args.len() == 1 {
        eprintln!("error: no file input");
        return;
    }

    Compiler::run_file(args[1].clone()).unwrap();
}
