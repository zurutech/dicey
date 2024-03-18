import os
import sys

from setuptools import Extension, setup

from Cython.Build import cythonize

fdir = os.path.dirname(os.path.realpath(__file__))
depsdir = os.path.join(fdir, 'dicey.deps')

incdir = os.path.join(depsdir, 'include')
libdir = os.path.join(depsdir, 'lib')

setup(
    ext_modules = cythonize(
        [Extension("*", [
                "dicey/*/*.pyx",
            ],
            libraries=["dicey"],
            library_dirs=[libdir],

            # set an absolute rpath - this is a temporary hack 
            runtime_library_dirs = [libdir] if os.name == 'posix' else None,
        )],
        gdb_debug=True,
        compiler_directives={'language_level' : "3"},
    ),
    include_dirs=[incdir],
)
