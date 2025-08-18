use std::{fs, path::PathBuf, str::FromStr};

use honeyc_lib::{CompConfig, Compiler};

// FIXME: The first time tests are run, fail because the files are being created all at the same
// time. Some most likely are overlapping.
pub fn assert_src(name: impl Into<String>, src: impl Into<String>, code: i32) {
    let config = CompConfig {
        pretty_print_ast: true,
        build_path: Some(PathBuf::from_str("tests/.hun_build").unwrap()),
        gcc_args: Vec::new(),
    };
    let build = Compiler::build_src(src.into(), &name.into(), None, config).unwrap();
    assert_eq!(Compiler::run_build(&build).code().unwrap(), code)
}

pub fn assert_file(file: &str, code: i32) {
    assert_src(
        PathBuf::from_str(file)
            .unwrap()
            .file_name()
            .unwrap()
            .to_string_lossy()
            .into_owned(),
        fs::read_to_string(file).unwrap(),
        code,
    );
}
