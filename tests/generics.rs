mod common;
use common::assert_src;

#[test]
fn generic_def() {
    assert_src(
        "generic def",
        "
Foo :: struct T {}

main :: fn(|) i32 {
    0
}
    ",
        0,
    );
}

