### Instructions
To update the compiler:
`meson compile -C build` 

To compile .hun files (Will look for 'honey/main.hun'):
`build/src/honey`

To link object file with c bindings:
`clang -g bindings.c honey.o -o main`

To run honey code:
`./main`
