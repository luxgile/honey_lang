use common::assert_src;

mod common;

#[test]
fn main_func() {
    assert_src("main0", "main :: fn(|) i32 { 0 }", 0);
    assert_src("main1", "main :: fn(|) i32 { 1 }", 1);
}

#[test]
fn overloading_fn() {
    assert_src("overloading_fn", "
        foo :: fn(|a: i32) i32 { a }
        foo :: fn(|a: f32) f32 { a }
        main :: fn(|) i32 { 0 }
        ", 0);
}
