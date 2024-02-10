import sys as _sys

from .errors cimport dicey_error, dicey_error_def, dicey_error_infos

class DiceyError(Exception):
    pass

_errors = {}

cdef generate_all_errors():
    cdef const dicey_error_def *error_defs = NULL
    cdef size_t nerrors = 0

    current_module = _sys.modules[__name__]

    dicey_error_infos(&error_defs, &nerrors)

    for info in error_defs[:nerrors]:
        if info.errnum == 0:
            continue
        
        name = info.name.decode('ASCII')
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
        _errors[info.errnum] = error

def _check(error: int):
    if error != 0:
        error = _errors.get(error, None) 

        raise error() if error else ValueError(f'Unknown error code: {error}')

generate_all_errors()
