# WORK IN PROGRESS
This is still work in progress. Design is incomplete.

## A copy-on-write value-type for C++
Based on [`polymorphic_value`](https://github.com/jbcoe/polymorphic_value). 

## Submodules
Tests use the 'catch' test framework: <https://github.com/philsquared/Catch.git>

To get the submodule run:

```
git submodule update --init
```

## Building
The build uses cmake driven by a simple Python script. To build and run tests, run the following from the console:

```
./scripts/build.py --tests
```

## Continuous integration
**Build status (on Travis-CI):** [![Build Status](https://travis-ci.org/jbcoe/copy_on_write.svg?branch=master)](https://travis-ci.org/jbcoe/copy_on_write)

