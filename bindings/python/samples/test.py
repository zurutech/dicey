# Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

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
