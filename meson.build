project(
    'ghwiki2chm',
    'cpp', 'c',
    version: '0.1.1',
    license: 'MIT',
    default_options: ['warning_level=1', 'cpp_std=c++20', 'c_std=c11', 'default_library=static'],
    meson_version: '>=1.0',
)


conf_data = configuration_data()
conf_data.set('GHWIKI2CHM_VERSION', '"' + meson.project_version() + '"')
configure_file(
    configuration: conf_data,
    output: 'config.hpp',
)

conf_inc = include_directories('.')


deps = [
    dependency('threads'),
    import('cmake').subproject('maddy').dependency('maddy'),
    subproject(
        'curl',
        default_options: [
            'tests=disabled',
            'unittests=disabled',
            'default_library=static',
        ]).get_variable('curl_dep'),
]

inc = [
    conf_inc,
]

ghwiki2chm = executable(
    'ghwiki2chm',
    'src/main.cpp',
    'src/project.cpp',
    'src/compiler.cpp',
    'src/utils.cpp',
    dependencies: deps,
    include_directories: inc,
)
