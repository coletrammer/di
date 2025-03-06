# di Library

A free-standing alternative C++ standard library, using bleeding edge C++ features (C++ 26).

## Features

- Free-standing support (can build with no C++ or C standard header files, and replaces exceptions with result types)
- Header only, trivially installable
- Platform independent
  - Certain functionality, like allocation, depends on a default platform allocator. This is suitable for normal
    applications as well as Operating System kernels.
    - [Iros](https://github.com/coletrammer/iros) is an minimal kernel built using di.
    - [dius](https://github.com/coletrammer/dius) is a user space library built using di.
- Wide coverage of C++ library functionality, including:
  - Containers (Vector, String, Ring buffer, Linked list)
  - Vocabulary types (Optional (supports references), Expected, Result, Tuple, Variant, Function)
  - Supports C++ 20 standard range based algorithms and views
  - Formatting (std::format like)
  - Parsing strings
  - Async Execution (p2300) and coroutine support that handles allocation failures
  - CLI argument parser
  - Mini unit test library
  - Verbose static reflection library (hopefully will be obsoleted by C++ someday)
  - Type-erasure library (Any) support using tag_invoke() magic.

## Library Component Overview

See [here](docs/pages/library_component_overview.md).

## Building

See [here](docs/pages/build.md).

## Developing

See [here](docs/pages/developing.md).
