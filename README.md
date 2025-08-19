<img align="left" style="width:256px" src="https://github.com/luxgile/honey_lang/blob/main/logo/honey_logo.png">

### Honey
Honey is a fast and simple programming language centered both on flexibility, simplicity and performance. 
It's aimed to those looking for a simple language like Odin with the features of Rust. 

> [!WARNING]
> The language is very much still work in progress. While a good amount of features are already working, there are a lot of edge cases that are not handled properly,
> safety checks that are not implemented and a lack of compiler error printing, so working with Honey at the moment can be very frustrating. Use at your own risk!

## Some examples
To see all examples, check [examples](./examples)

**Hello world**
```honey
main :: fn(|) i32 {
  println "hello hun!"
  0
}
```

**Using [Raylib](https://github.com/raysan5/raylib/tree/master)**
```honey
rl :: import "raylib.hun"

main :: fn(|) i32 {
  rl.InitWindow 800 450 "raylib on honey"
  defer rl.CloseWindow

  white := Color .{ .r = 255, .g = 255, .b = 255, .a = 255 }
  lightgray := Color .{ .r = 200, .g = 200, .b = 200, .a = 255 }

  loop ! rl.WindowShouldClose {
    rl.BeginDrawing
    rl.ClearBackground white
    rl.DrawText "Hello from Honey!" 190 200 20 lightgray
    rl.EndDrawing
  }

  0
}
```

## Documentation
[**Language Reference**](./docs/overview.md)

All working, in progress or planned language features are listed here.
