import os
import sys

from setuptools import Extension, setup

from Cython.Build import cythonize

fdir = os.path.dirname(os.path.realpath(__file__))
depsdir = os.path.join(fdir, 'dicey.deps')

bindir = os.path.join(depsdir, 'bin')
incdir = os.path.join(depsdir, 'include')
libdir = os.path.join(depsdir, 'lib')

if os.name == 'nt':
    dicey_dll = os.path.join(bindir, 'dicey.dll')
    
    if os.path.isfile(dicey_dll):
        # assume shared
        libraries = ['dicey']
    else:
        # assume static
        libraries = ['dicey', 'libuv', 'wsock32', 'ws2_32', 'iphlpapi', 'user32', 'advapi32', 'dbghelp', 'userenv', 'shell32', 'ole32']
else:
    libraries = ['dicey', 'uv']

setup(
    ext_modules = cythonize(
        [Extension("*", [
                "dicey/*/*.pyx",
            ],
            libraries=libraries,
            library_dirs=[libdir],

            # set an absolute rpath - this is a temporary hack 
            runtime_library_dirs = [libdir] if os.name == 'posix' else None,
        )],
        gdb_debug=True,
        compiler_directives={'language_level' : "3"},
    ),
    include_dirs=[incdir],
)
