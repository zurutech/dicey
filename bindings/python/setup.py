# Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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

if os.name == 'posix':
    rpath = ['@loader_path/../deps/lib' if sys.platform == 'darwin' else '$ORIGIN/../deps/lib']
else:
    rpath = None

setup(
    ext_modules = cythonize(
        [Extension("*", [
                "dicey/*/*.pyx",
            ],
            libraries=libraries,
            library_dirs=libdirs,

            runtime_library_dirs = rpath,
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
