# Copyright (c) 2024-2025 Zuru Tech HK Limited, All rights reserved.
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

import sys as _sys

from .errors cimport dicey_error, dicey_error_def, dicey_error_infos

class DiceyError(Exception):
    pass

class BadVersionStrError(DiceyError):
    def __init__(self, str badstr):
        self.bad_string = badstr

        super().__init__(f'Bad version string: {badstr}')

_cerrors = {}

cdef generate_c_errors():
    cdef const dicey_error_def *error_defs = NULL
    cdef size_t nerrors = 0

    current_module = _sys.modules[__name__]

    dicey_error_infos(&error_defs, &nerrors)

    for info in error_defs[:nerrors]:
        if info.errnum == 0:
            continue
        
        name = info.name.decode('ASCII') + 'Error'
        message = info.message.decode('ASCII')

        error = type(
            name,
            (DiceyError,),
            {
                '__init__': lambda self, message=message: DiceyError.__init__(self, message),
                '__module__': __name__,
            }
        )

        setattr(current_module, name, error)
        _cerrors[info.errnum] = error

cdef void _check(const dicey_error error):
    if error != 0:
        err_cls = _cerrors.get(error, None) 

        raise err_cls() if err_cls else ValueError(f'Unknown error code: {error}')

generate_c_errors()
