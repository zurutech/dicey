import os

if os.name == 'nt':
    fdir = os.path.dirname(os.path.realpath(__file__))
    ldir = os.path.join(fdir, os.pardir, 'dicey.deps', 'bin')

    # if the directory exists, add it to the DLL search path
    # if not, assume that this is a static build. I wish all my users for the best of things
    if os.path.isdir(ldir):
        os.add_dll_directory(ldir)

        os.environ['PATH'] += ';' + ldir    

from .core import *
from .ipc  import *