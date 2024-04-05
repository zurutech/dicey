# Dicey Python bindings

This directory contains an almost fully fledged Python wrapper around the Dicey C API.

## Completion level

Currently, everything is wrapped except the Server and Registry.

This means that these bindings support:

- Creating, serialising and deserialising Dicey packets
- Connecting to a Dicey server using `dicey.Client`. Currently, only the synchronous API is supported.

## Usage

These bindings have been designed to be as simple to use as possible. Here's a simple example of how to connect to a
running Dicey server and set a property, or exec an operation:

```python
import dicey

# this assumes Linux, with the server running at the abstract namespace socket "@/tmp/.uvsock"
# on Windows, this is a named pipe, and the path should be something like r'\\.\pipe\uvsock'
# on Unix (including macOS), this is a Unix domain socket, and the path should be something like "/tmp/.uvsock"
with dicey.connect('@/tmp/.uvsock') as dc:
    dc.set('/sval', ('sval.Sval', 'Value'), 'hello')

    assert dc.get('/sval', ('sval.Sval', 'Value')) == 'hello'

    dc.exec('/dicey/sample_server', ('dicey.sample.Server', 'Halt')) # this will stop the sample server
```

Most of the Dicey API is wrapped using builtin Python types whenever possible.

## API

While we are working hard to get the documentation up to date, you can get a rough idea about how to use the API with the
`help` function, or by looking at the source code.

In general, the client API is built around the `Client` class, which has a handful of methods to interact with the server.
The `Client` class is a context manager, so you can use it in a `with` block to ensure that the connection is closed when
you are done with it.

- `Client.__init__()`:
    Creates a new client object. This does not connect to the server - you must call `connect` to do that.

- `Client.connect(self, path: str) -> None`:
    Connects to the server at the given path. This will raise an exception if the connection fails.

- `Client.disconnect(self) -> None`:
    Disconnects from the server.

- `Client.get(self, path: Path | str, selector: Selector | (str, str), int timeout_ms: int = DEFAULT_TIMEOUT_MS) -> Any`:
    Gets the value of a property on the server. The `path` is the path to the object, and the `selector` is the selector
    for the property to get. The return value is the value of the property, which will be automatically deserialised from
    the Dicey format to a native Python type.

- `Client.set(self, path: Path | str, selector: Selector | (str, str), value: Any, int timeout_ms: int = DEFAULT_TIMEOUT_MS) -> Any`:
    Sets the value of a property on the server. The `path` is the path to the object, and the `selector` is the selector
    for the property to set. The `value` will be automatically serialised and sent to the server - which may reject the
    value if it does not match the expected signature of the property. Currently, only native Python types are supported.

- `Client.exec(self, path: Path | str, selector: Selector | (str, str), arg: Any = None, timeout_ms: int = 1000) -> Any`:
    Execute an operation on the server. The `path` is the path to the object, and the `selector` is the selector for the
    operation to execute. `arg` will be automatically serialised and sent to the server - which may reject the value if
    it does not match the expected signature of the operation. Currently, only native Python types are supported.

- `Client.request(self, Message message: Message, int timeout_ms: int = DEFAULT_TIMEOUT_MS) -> Any`:
    Sends a message to the server, and waits for a response. The set, get and exec methods are all implemented using this
    underneath. Use this method if you need more control on the message being sent.

- `Client.on_event: EventCallback`:
    A `Callable[[Operation, Path, Selector, Optional[Any]], None]` which will be called whenever an event is received from
    the server. This is a property, so you can assign a new callback to it to change the behaviour.
    Careful: this callback will be called from the Dicey client thread, so you should not do any blocking operations in it.

- `Client.running: bool`:
    A boolean property which is true if the client is currently connected to the server, and false otherwise.

In the `dicey` module, there are also a few helper functions:

- `dicey.connect(path: Address | str) -> Client`:
    A helper function which creates a new client object, connects to the server at the given path, and returns the client.
    Useful with `with` blocks.

## Installing

Until pre-built Wheels are available, you can install the bindings by running `build` and building a wheel yourself.
The process is not too complicated, but it does require a few steps.

0. (optional on Linux, mandatory elsewhere) Create a virtual environment and activate it.
1. Build the Dicey C library using a `shared` preset (which is usually the default) - `debug` or `release` are both fine
   depending on whether you want debug symbols or not.
2. Install the Dicey C library using `cmake --install <builddir> --prefix /path/to/bindings/dicey/deps`, where `<builddir>`
   is the build directory of the Dicey C library and `/path/to/bindings` is this directory.
3. Install `build` either from your package manager or from `pip` using `python -m pip install build` (requires a venv).
4. Run `python -m build --wheel` in this directory to build the wheel. This will create a `.whl` file in the `dist`
   directory.
5. Install the wheel using `python -m pip install /path/to/bindings/dist/dicey-*.whl` (only when running in a venv - don't
   mess up your system!).

When using Windows, remember to run everything from a Visual Studio Developer Command Prompt or PowerShell, as the build
process requires the MSVC toolchain to be available.

## Development

If you want to contribute to the Python bindings, it's probably best to build the Cython bindings directly from the
source.

After following the points above up to and including 2., you can run `python setup.py build_ext --inplace` while in the
`bindings/python` directory to build the Cython bindings. This will compile the bindings, which can then be imported as
usual.
