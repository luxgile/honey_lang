project(
  'honey-lang',
  'cpp',
  version : '0.1',
  meson_version : '>= 1.3.0',
  default_options : ['warning_level=3', 'cpp_std=c++23'],
)

inc = include_directories('src')
llvm_dep = dependency('llvm', required: true)

subdir('src')
subdir('tests')
