from dataclasses import dataclass as _dataclass

from .errors import DiceyError

@_dataclass
class ErrorMessage(DiceyError):
    """An error message is an error that can be sent or received via the Dicey protocol"""
    # 16-bit error code
    code: int
    message: str

    def __init__(self, code: int, message: str):
        if code < 0 or code > 0xFFFF:
            raise ValueError("code must be a 16-bit unsigned integer")

        self.code = code
        self.message = message

@_dataclass
class Path:
    """A path is an ASCII string that identifies an object residing on the server"""
    value: str

    def __init__(self, value: str):
        if not value:
            raise ValueError("paths can't be empty")

        if not value.isascii():
            raise ValueError("paths must be ASCII strings")

        # TODO: validate path format further

        self.value = value

    def __str__(self):
        return self.value

@_dataclass
class Selector:
    """A selector is a pair of two ASCII trings identifying a specific element in a given trait"""
    trait: str
    elem: str

    def __str__(self):
        return f"{self.trait}:{self.elem}"