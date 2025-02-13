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

import os
from pathlib import Path

if os.name == 'nt':
    _fdir = Path(__file__).resolve().parent
    _ldir = _fdir.joinpath('deps', 'bin')

    # if the directory exists, add it to the DLL search path
    # if not, assume that this is a static build. I wish all my users for the best of things
    if _ldir.is_dir():
        os.add_dll_directory(_ldir)

        os.environ['PATH'] += ';' + str(_ldir) 

from .core import *
from .ipc  import *