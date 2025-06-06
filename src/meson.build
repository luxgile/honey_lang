sources = ['main.cpp', 'lexer.cpp', 'parser.cpp', 'v_pretty_print.cpp', 'program_ctx.cpp']

llvm_dep = dependency('llvm', required: true)

honey_exe = executable('honey', sources, install : true, dependencies: llvm_dep)
