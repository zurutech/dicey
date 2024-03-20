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
