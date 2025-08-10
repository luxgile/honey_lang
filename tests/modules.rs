mod common;
use common::assert_src;

#[test]
fn module_def() {
    assert_src(
        "module def",
        "
math :: module {
  add :: fn (lhs: i32 | rhs: i32) i32 {
    @+ lhs rhs
  }
}

main :: fn(|) i32 {
    0
}
    ",
        0,
    );
}

#[test]
fn module_access() {
    assert_src(
        "module access",
        "
math :: module {
  add :: fn (lhs: i32 | rhs: i32) i32 {
    @+ lhs rhs
  }
}

main :: fn(|) i32 {
  x := 10 math.add 5
  x
}
    ",
        15,
    );
}
