mod common;
use common::assert_file;

#[test]
fn import_math() {
    assert_file("tests/simple_project/main.hun", 15);
}
