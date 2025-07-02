project(
    'ghwiki2chm',
    'cpp', 'c',
    version: '0.2.0-rc.2',
    license: 'MIT',
    default_options: {
        'cpp_std':          'c++20',
        'c_std':            'c11',
        'default_library':  'static',
        'b_vscrt':          'static_from_buildtype',
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



if host_machine.system() != 'windows'
    # static C++ standard library
    add_project_link_arguments(['-static-libstdc++', '-static-libgcc'], language: 'cpp')
endif


deps = [
    dependency('rutils-base', fallback: 'random-utils'),
    # Its better to build curl and all its dependencies from source manualy. Take a look at .github/scripts/static_deps_from_source.sh.
    dependency('libcurl', fallback: 'curl'),
    import('cmake').subproject('maddy').dependency('maddy'),
]

ghwiki2chm = executable(
    'ghwiki2chm',
    sources: src,
    dependencies: deps,
    install: true,
)
