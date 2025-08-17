mod common;
use common::assert_src;

#[test]
fn break_loop() {
    assert_src(
        "break_loop",
        "main :: fn(|) i32 {
            i := 0
            loop true {
                &i += 1
                if i == 10 {
                    break
                }
            }
            i
        }",
        10,
    );
}
