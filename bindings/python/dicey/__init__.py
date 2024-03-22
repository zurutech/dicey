# Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

import os
from pathlib import Path

if os.name == 'nt':
    _fdir = Path(__file__).resolve().parent
    _ldir = _fdir.joinpath('deps', 'bin')

    # if the directory exists, add it to the DLL search path
    # if not, assume that this is a static build. I wish all my users for the best of things
    if _ldir.is_dir():
        os.add_dll_directory(_ldir)

        os.environ['PATH'] += ';' + _ldir 

from .core import *
from .ipc  import *