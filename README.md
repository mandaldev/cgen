# cgen: A C/C++ Project Manager

cgen manages c/c++ projects and build task.

## Features

- simple project description file format: mas
- language: C, C++
- target type: exectuable, static/dynamic library
- phony target, for transporting dependancies
- template target, for templating familiar targets
- export compile database file

## Demos

[demos](demos/) directory contains demo projects.

## Build from source

A prebuilt binary cgen is needed.

Enter repo root directory and fire command: `cgen build`.

### Dependancies

- gcc: you need compilers, currently only supports gcc.
- ninja: cgen use ninja to drive compilers.
- libpkg-conf: to use external dependancies with pkg-config.

## License

GPL 3.0
