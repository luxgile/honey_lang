use std::{path::PathBuf, str::FromStr};

use hunc_lib::{CompConfig, Compiler};

// FIXME: The first time tests are run, fail because the files are being created all at the same
// time. Some most likely are overlapping.
pub fn assert_src(name: impl Into<String>, src: impl Into<String>, code: i32) {
    let config = CompConfig {
        pretty_print_ast: true,
        print_c: true,
        build_path: Some(PathBuf::from_str("tests/.hun_build").unwrap()),
    };
    let build = Compiler::build_src(src.into(), &name.into(), config).unwrap();
    assert_eq!(Compiler::run_build(&build).code().unwrap(), code)
}
