mod common;
use common::assert_src;

#[test]
fn defer_basic() {
    assert_src(
        "defer basic",
        "main :: fn(|) i32 {
        i := 10
        defer i = 0
        i
    }",
        0,
    );
}

#[test]
fn defer_before_assign() {
    assert_src(
        "defer before assign",
        "main :: fn(|) i32 {
        i := 10
        defer i = 0
        i = 5
        i
    }",
        0,
    );
}

#[test]
fn defer_before_if() {
    assert_src(
        "defer before if",
        "main :: fn(|) i32 {
        i := 10
        defer i = 0
        if true {
            i = 5
            ret i
        }
        1
    }",
        0,
    );
}

#[test]
fn defer_call() {
    assert_src(
        "defer call",
        "
        ret_zero :: fn(|) i32 { 0 } 
        main :: fn(|) i32 {
        i := 10
        defer i = ret_zero
        i = 7
        i
    }",
        0,
    );
}
