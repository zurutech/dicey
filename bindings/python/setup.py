import os

from setuptools import Extension, setup

from Cython.Build import cythonize

fdir = os.path.dirname(os.path.realpath(__file__))

setup(
    ext_modules = cythonize(
        [Extension("*", [
                "dicey/*/*.pyx",
            ],
            libraries=["dicey"],
            library_dirs=["dicey.deps/lib"],

            # set an absolute rpath - this is a temporary hack 
            runtime_library_dirs = [os.path.join(fdir, 'dicey.deps', 'bin' if os.name == 'nt' else 'lib')]
        )],
        gdb_debug=True,
        compiler_directives={'language_level' : "3"},
    ),
    include_dirs=["dicey.deps/include"],
)
