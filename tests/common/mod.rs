use std::{fs, path::PathBuf, str::FromStr};

use honeyc_lib::{CompConfig, Compiler};

// FIXME: The first time tests are run, fail because the files are being created all at the same
// time. Some most likely are overlapping.
pub fn assert_src(name: impl Into<String>, src: impl Into<String>, code: i32) {
    let config = CompConfig {
        pretty_print_ast: true,
        build_path: Some(PathBuf::from_str("tests/.hun_build").unwrap()),
        lib_paths: Vec::new(),
        lib_names: Vec::new(),
        includes: Vec::new(),
    };
    let build = Compiler::build_src(src.into(), &name.into(), None, config).unwrap();
    assert_eq!(Compiler::run_build(&build).code().unwrap(), code)
}

pub fn assert_file(file: &str, code: i32) {
    let config = CompConfig {
        pretty_print_ast: true,
        build_path: Some(PathBuf::from_str("tests/.hun_build").unwrap()),
        lib_paths: Vec::new(),
        lib_names: Vec::new(),
        includes: Vec::new(),
    };
    let file = PathBuf::from_str(file).unwrap();
    let build = Compiler::build_file(&file, config).unwrap();
    assert_eq!(Compiler::run_build(&build).code().unwrap(), code)
}
