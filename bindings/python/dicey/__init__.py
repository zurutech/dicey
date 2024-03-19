import os

if os.name == 'nt':
    _fdir = os.path.dirname(os.path.realpath(__file__))
    _ldir = os.path.join(_fdir, 'deps', 'bin')

    # if the directory exists, add it to the DLL search path
    # if not, assume that this is a static build. I wish all my users for the best of things
    if os.path.isdir(_ldir):
        os.add_dll_directory(_ldir)

        os.environ['PATH'] += ';' + _ldir    

from .core import *
from .ipc  import *