# cgen: A C/C++ Project Manager

cgen manages c/c++ projects and build tasks. cgen only supports Linux.

## Features

- simple project description file format: mas, project model see [`ProjectModel.hpp`](src/ProjectModel.hpp)
- language: C, C++
- target type: exectuable, library (supports both static and dynamic)
- phony target, for transporting dependancies
- template target, for templating familiar targets
- export compile database file

### Not yet

- versioning support

## Demos

[demos](demos/) directory contains some demo projects.

## Build from source

A prebuilt binary cgen is needed. Enter cgen source folder and fire:

```bash
cgen build --profile=release
```

### Dependancies

- [Com](https://github.com/mandaldev/com) library
- gcc: you need compilers, currently only supports gcc.
- ninja: cgen use ninja to drive compilers.
- libpkg-conf: to use external dependancies with pkg-config.

## License

GPL 3.0
