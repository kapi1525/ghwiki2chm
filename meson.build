project(
    'ghwiki2chm',
    'cpp', 'c',
    version: '0.2.0-dev',
    license: 'MIT',
    default_options: {
        'cpp_std':          'c++20',
        'c_std':            'c11',
        'default_library':  'static',
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
    dependency(
        'libcurl',
        fallback: 'curl',
        default_options: {
            'tool': 'disabled',
            'tests': 'disabled',
            'unittests': 'disabled',

            'bindlocal': 'disabled',
            'brotli': 'auto',
            'cookies': 'disabled',
            'doh': 'disabled',
            'form-api': 'disabled',
            'getoptions': 'auto',
            'gsasl': 'disabled',
            'http2': 'auto',
            'ipv6': 'enabled',
            'libcurl-option': 'auto',
            'libz': 'auto',
            'mime': 'disabled',
            'netrc': 'disabled',
            'parsedate': 'auto',
            'progress-meter': 'auto',
            'proxy': 'enabled',
            'psl': 'disabled',
            'sha512_256': 'auto',
            'shuffle-dns': 'auto',
            'socketpair': 'disabled',
            'sspi': 'disabled',
            'unixsockets': 'auto',
            'verbose-strings': 'enabled',
            'websockets': 'disabled',
            'zstd': 'auto',

            'asynchdns': 'enabled',

            'aws': 'disabled',
            'basic-auth': 'disabled',
            'bearer-auth': 'disabled',
            'digest-auth': 'disabled',
            'kerberos-auth': 'disabled',
            'negotiate-auth': 'disabled',

            'idn': 'disabled',

            'alt-svc': 'enabled',
            'headers-api': 'disabled',
            'hsts': 'disabled',
            'http-auth': 'disabled',

            'ntlm': 'enabled',

            'ssh': 'disabled',

            'ssl': 'auto',

            'dict' : 'disabled',
            'file' : 'disabled',
            'ftp' : 'disabled',
            'gopher' : 'disabled',
            'http' : 'enabled',
            'imap' : 'disabled',
            'ldap' : 'disabled',
            'ldaps' : 'disabled',
            'mqtt' : 'disabled',
            'pop3' : 'disabled',
            'rtmp' : 'disabled',
            'rtsp' : 'disabled',
            'smb' : 'disabled',
            'smtp' : 'disabled',
            'telnet' : 'disabled',
            'tftp' : 'disabled',
        },
    ),
    import('cmake').subproject('maddy').dependency('maddy'),
]

ghwiki2chm = executable(
    'ghwiki2chm',
    sources: src,
    dependencies: deps,
    install: true,
)
