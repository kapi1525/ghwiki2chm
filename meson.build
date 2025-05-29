project(
    'ghwiki2chm',
    'cpp', 'c',
    version: '0.1.3',
    license: 'MIT',
    default_options: {
        'cpp_std':          'c++20',
        'c_std':            'c11',
        'warning_level':    '1',
    },
    meson_version: '>=1.2',
)

src = files(
    'src/main.cpp',
    'src/project.cpp',
    'src/compiler.cpp',
)

src += configure_file(
    configuration: configuration_data({
        'GHWIKI2CHM_VERSION': '"' + meson.project_version() + '"',
    }),
    output: 'config.hpp',
)


deps = [
    dependency('threads'),
    dependency('rutils-base', fallback: 'random-utils'),
    dependency('rutils-request', fallback: 'random-utils'),
    import('cmake').subproject('maddy').dependency('maddy'),
]

ghwiki2chm = executable(
    'ghwiki2chm',
    sources: src,
    dependencies: deps,
)
