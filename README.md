# Common Code of the Main Telescope Rotator and Hexapod Controllers

## Needed Package

- [libyaml](https://github.com/yaml/libyaml): v0.2.5
- [googletest](https://github.com/google/googletest): v1.10.0 (optional)
- [gcovr](https://github.com/gcovr/gcovr) (optional)
- clang-format (optional)
- glib-devel
- glib2-devel.x86_64

## Code Format

The C/C++ code is automatically formatted by `clang-format`.

To enable this with a git pre-commit hook:

- Install the `clang-format` C++ package.
- Run `git config core.hooksPath .githooks` once in this repository.

## Setup the Environment

Set the path variable by:

```bash
export PXI_CNTLR_HOME=$path_to_this_module
```

## Unit Test and Code Coverage

1. Do the following to do the unit tests in docker container:

```bash
cd tests/
make
../bin/test
```

2. To get the code coverage, do:

```bash
cd build/tests/
gcovr -r ../../src/ .
```

If the Cobertura xml report is needed, do the following instead:

```bash
gcovr -r ../../src/ . --xml-pretty > coverageReport.xml
```

3. To clean the test data, do `make clean` in `tests/` directory.

## Command Status

The details can follow [commandStatus](doc/commandStatus.md).
