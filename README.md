# dicey

Dicey is a WIP library that strives to provide an easy yet extensible way to implement 1:N IPC between a central
application and multiple observer processes.

Dicey allows applications to expose a fast, asynchronous entry point to a set of well-defined objects, over either UDS or
named pipes (Windows).

## Supported platforms

Dicey is written in strict C11 with little to no extensions, and has been tested on the following configurations:

- GNU/Linux x86_64
- GNU/Linux AArch64
- macOS AArch64
- FreeBSD x86_64
- Windows x86_64

## Building

In order to build Dicey, you need:

- a recent C11-compliant compiler. The code has been thourougly tested with GCC 12+, Clang 15+ and CL 19.38+ (MSVC 2022)
- a recent version of CMake  - at least 3.26
- (optional) Ninja, for presets.

If you have `ninja` installed, you can use one of the many pre-defined presets to build Dicey:

```
$ cmake --list-presets
Available configure presets:

  "debug"                                - Debug (shared)
  "debug-vendored"                       - Debug (shared, with vendored libraries)
  "debug-static"                         - Debug (static)
  "debug-static-vendored"                - Debug (static, with vendored libraries)
  "debug-min"                            - Debug (shared, no samples)
  "debug-min-vendored"                   - Debug (shared, no samples, with vendored libraries)
  "debug-min-static"                     - Debug (static, no samples)
  "debug-min-static-vendored"            - Debug (static, no samples, with vendored libraries)
  "debug-gcc"                            - Debug (shared, with GCC)
  "debug-gcc-analyzer"                   - Debug (shared, with GCC, with GCC Static analyzer)
  "debug-llvm-unix"                      - Debug (shared, with LLVM)
  "debug-llvm-fuzzer"                    - Debug (shared, with LLVM, with libFuzzer sample)
  "debug-llvm-asan"                      - Debug (shared, with LLVM, with AddressSanitizer)
  "debug-llvm-msan"                      - Debug (shared, with LLVM, with MemorySanitizer)
  "debug-llvm-tsan"                      - Debug (shared, with LLVM, with ThreadSanitizer)
  "debug-llvm-ubsan"                     - Debug (shared, with LLVM, with UndefinedBehaviorSanitizer)
  "release"                              - Release (shared)
  "release-vendored"                     - Release (shared, with vendored libraries)
  "release-with-samples"                 - Release (shared, with samples)
  "release-with-samples-vendored"        - Release (shared, with samples, with vendored libraries)
  "release-static"                       - Release (static)
  "release-static-vendored"              - Release (static, with vendored libraries)
  "release-static-with-samples"          - Release (static, with samples)
  "release-static-with-samples-vendored" - Release (static, with samples, with vendored libraries)
  "release-gcc"                          - Release (shared, with GCC)
  "release-llvm-unix"                    - Release (shared, with LLVM)
```

If you just want to test the library, build a debug static version:

```
$ cmake --preset debug-static
$ cmake --build --preset debug-static
```

This will build all libraries as static libraries and all samples.

More info about how to build different builds, including for release, will come in due time.

## Using the `samples`

`samples` contains several useful tools that can be used to test Dicey.
In particular, after building with BUILD_SAMPLES=ON (i.e., a debug config or a `with-samples` release preset) you should
find the following sample executables under `<buildir>/samples`:

- **load**: loads a Dicey packet from either a binary representation or an XML representation, following the schema defined
  at `schemas/packet.xsd`. Prints a full textual representation of the packet on stdout.
  You can use load to compile XML files to binary using the `-o` parameter:

  ```
  $ build/samples/load -o packet.bin schemas/packet.xml
  $ build/samples/load packet.bin
  #0 = message {
    kind = SET
    path = "/foo/bar"
    selector = (a.street.trait.named.Bob:Bobbable)
    value = tuple(
        [0] = array[bool]{
            [0] = bool:true
            [1] = bool:false
        }
        [1] = bool:true
        [2] = array[float]{
            [0] = float:3.140000
            [1] = float:2.718000
        }
        [3] = error:(code = 10)
        [4] = error:(code = 20, message = "Oh no!")
        [5] = byte:88
        [6] = unit:()
        [7] = pair{
            first = str:"key"
            second = array[tuple]{
                [0] = tuple(
                    [0] = i32:1
                    [1] = i32:-2
                )
                [1] = tuple(
                    [0] = str:"foo"
                    [1] = str:"bar"
                    [2] = bytes:[ 62 61 73 65 36 34 ] (6 bytes)
                )
            }
        }
    )
  }
  ```

- **server**: a simple server for Dicey. Run it as `server` with no arguments. It will listen on a pre-defined socket or
  named pipe, whose name will be printed on stdout:

  ```
  $  build/samples/server
    starting Dicey sample server on @/tmp/.uvsock...
  ```

  You can stop the server with `Ctrl-C`, or by performing `EXEC /dicey/sample_server#dicey.sample.Server:Halt` with
  signature `(u) -> u`.

  The server will print all incoming messages on stdout, and errors on stderr.

- **sval**: a simple client for the `sval.Sval` service exposed by `server`.
  Run it as

  ```
  $ sval SOCKETPATH
  /sval#sval.Sval:Value = "hello"
  ```

  to perform a simple `GET` operation on the `/sval#sval.Sval:Value` string property, or

  ```
  $ sval SOCKETPATH "world"
  ```

  to perform a `SET` instead, with the given string.

  `SOCKETPATH` is the name of either an UDS or a Windows named pipe. On Linux, you can specify an abstract socket using
  the `@` notation, like `@/tmp/.uvsock`.

Alongside these samples, you will find the following internal tools which may looks somewhat cryptic, but are good references
for how to perform more complex tasks with `libdicey`:

- *client*: a simple client for Dicey which sends an arbitrarily complex payload.
  Run it as `client SOCKETPATH PAYLOAD`, where `SOCKETPATH` is the name of either an UDS or a Windows named pipe.
  `PAYLOAD` can can be either a binary payload or an XML file.

- *dump*: generates a sample (builtin for now) payload to a file, or prints it on stdout as hex.

- *base64*: a clone of the classic UNIX `base64` utility. It can encode and decode base64 payloads.
