honey_src = ['lexer.cpp', 'v_expr_type.cpp', 'parser.cpp', 'v_pretty_print.cpp', 'program_ctx.cpp', 'ast.cpp', 'v_llvm_ir.cpp', 'compiler.cpp', 'types.cpp']
honey_lib = shared_library('honey_btsp', honey_src, include_directories: inc, dependencies: llvm_dep)

cli_src = ['main.cpp']
cli_exe = executable('honey', cli_src, install : true, dependencies: [ llvm_dep ], include_directories: inc, link_with: honey_lib)
