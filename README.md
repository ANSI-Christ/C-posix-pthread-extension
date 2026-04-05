# C posix pthread extension

pthread_ext.h is an extension for POSIX pthread: thread pool, channel, task group.

## Features

- not a toy
- 2x single-header
- flexible API
- highly optimized
- manual memory control
- minimal resource usage
- small codebase relative to feature set

## Components

- thread pool + attributes
- task group + attributes
- pipe-based channel

## Requirements

- POSIX-compatible environment
- C99 compiler with GNU extensions (GCC 2.95+, Clang 1.0+, MinGW)
- C89 compiler with -DM_PP_C89

## Notes

- examples and benchmark are in [main.c](main.c)
- repository also contains a benchmark against C++ Boost thread pool
