import os
from pathlib import Path
import sys

from setuptools import Extension, setup, find_namespace_packages

from Cython.Build import cythonize

fdir = Path(__file__).resolve().parent
depsdir = fdir.joinpath('dicey', 'deps')

bindir = depsdir.joinpath('bin')
incdir = depsdir.joinpath('include')
libdir = depsdir.joinpath('lib')

if os.name == 'nt':
    dicey_dll = bindir.joinpath('dicey.dll')
    
    if dicey_dll.is_file():
        # assume shared
        libraries = ['dicey']
    else:
        # assume static
        libraries = ['dicey', 'libuv', 'wsock32', 'ws2_32', 'iphlpapi', 'user32', 'advapi32', 'dbghelp', 'userenv', 'shell32', 'ole32']
else:
    libraries = ['dicey', 'uv']

libdirs = [str(libdir)]

setup(
    ext_modules = cythonize(
        [Extension("*", [
                "dicey/*/*.pyx",
            ],
            libraries=libraries,
            library_dirs=libdirs,

            # set an absolute rpath - this is a temporary hack 
            runtime_library_dirs = libdirs if os.name == 'posix' else None,
        )],
        gdb_debug=True,
        compiler_directives={
            'embedsignature' : True,
            'language_level' : "3"
        },
    ),
    packages=find_namespace_packages(),
    include_dirs=[incdir],

    include_package_data=True,
    package_data={
        'dicey': ['deps/bin/*.dll', 'deps/lib/*.dylib', 'deps/lib/*.so']
    },
)
