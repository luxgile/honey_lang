![logo](./logo/honey_logo.png)

# Honey Lang
Honey is a fast and simple programming language centered both on flexibility, simplicity and performance. 
It's aimed to those looking for a simple language like Odin with the features of Rust. 

### Work in progress
The language is very much a work in progress still. While a good amount of features are already working, there are a lot of edge cases that are not handled properly and lack of compiler error printing,
so developing with Honey is currently frustrating at best. Use at your own risk.

## Language features
- [x] Defer statement
- [x] Struct and methods
- [x] Tagged unions (Or Rust enums for others)
- [ ] Pattern matching
- [x] Infix functions with any number of prefixed arguments
- [x] Modules to group each part of the program
- [ ] Trait system
- [ ] Manual memory management
- [ ] 'Dynamic' memory management
- [x] C transpiling
- [ ] Meta programming
- [ ] Reflection

## How to use
Create your .hun file and simply run `honeyc run <FILE>`. This will automatically generate the C project on the same folder inside '.hun_build', compile it using gcc and run the executable.

