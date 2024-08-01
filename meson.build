project(
    'ghwiki2chm',
    'cpp', 'c',
    version: '0.1.0',
    license: 'MIT',
    default_options: ['warning_level=1', 'cpp_std=c++17', 'c_std=c11'],
    meson_version: '>=1.0',
)


deps = [
    dependency('threads'),
    import('cmake').subproject('maddy').dependency('maddy')
]

ghwiki2chm = executable(
    'ghwiki2chm',
    'src/main.cpp',
    'src/args.cpp',
    'src/project.cpp',
    'src/compiler.cpp',
    dependencies: deps,
)
