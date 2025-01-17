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

import sys
from io import BytesIO

import dicey

def load_path(path: str) -> dicey.Packet:
    with open(path, 'rb') as file:
        return load_from(file)

def load_from(file: BytesIO) -> dicey.Packet:
    data = file.read()
    return dicey.loads(data)

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} FILE", file=sys.stderr)
        sys.exit(-1)

    print(load_path(sys.argv[1]))
