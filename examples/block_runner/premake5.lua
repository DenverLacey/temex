workspace 'block_runner'
configurations { 'debug', 'release' }

project 'block_runner'
    kind 'ConsoleApp'
    language 'C'
    cdialect 'C17'

    files {
        'src/**.h',
        'src/**.c',
        'src/**.hpp',
        'src/**.cpp',
        'src/**.hxx',
        'src/**.cxx',
        'src/**.cc',
    }

    includedirs {
        'src',
        '../..',
    }

    links {
        'ApplicationServices.framework',
        'Carbon.framework',
    }

    filter 'action:gmake2'
        buildoptions {
            '-Wpedantic',
            '-Wall',
            '-Wextra',
            '-Werror',
        }

    filter 'configurations:debug'
        defines { 'DEBUG' }
        targetdir 'bin/debug'
        symbols 'On'
        optimize 'Debug'

    filter 'configurations:release'
        defines { 'NDEBUG' }
        targetdir 'bin/release'
        optimize 'Full'

