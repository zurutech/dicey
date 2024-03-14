from setuptools import Extension, setup

from Cython.Build import cythonize

setup(
    ext_modules = cythonize(
        [Extension("*", [
                "dicey/core/*.pyx",
            ],
            libraries=["dicey"],
            library_dirs=["."],
        )],
        gdb_debug=True,
        compiler_directives={'language_level' : "3"},
    ),
    include_dirs=["include"]
)
