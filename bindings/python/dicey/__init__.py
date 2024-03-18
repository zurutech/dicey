import os

if os.name == 'nt':
    fdir = os.path.dirname(os.path.realpath(__file__))
    ldir = os.path.join(fdir, os.pardir, 'dicey.deps', 'bin')

    os.add_dll_directory(ldir)

    os.environ['PATH'] += ';' + ldir    

from .core import *
from .ipc  import *