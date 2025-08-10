use common::assert_src;

mod common;

#[test]
fn alloc_int() {
    assert_src("alloc int", "
        main :: fn(|) i32 {
            ptr := malloc @cast u32 8
            i := @cast ^i32 ptr
            ^i = 7
            t := ^i
            free ptr
            t
        }
        ", 7);
}
