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

## Quick Benchmark

| Pool | Runtime (ms) | Peak Memory (KB) |
|------|---------------|---------------|
| **pthread_pool_t (noalloc)** | 64.80 | 4.2 |
| **pthread_pool_t (batch)** | 82.60 | 4.2 |
| **pthread_pool_t (default)** | 102.90 | 5.8 |
| boost::asio::thread_pool | 145.82 | 84.4 |
| BS::thread_pool | 144.74 | 84 |
| TBB | 21.72 | 80 |
> See [BENCHMARK.md](BENCHMARK.md) for full details (code, CPU instructions, cache misses, allocations, etc.)

## Notes

- examples are in [main.c](main.c)
- on Windows native `pthread_kill` is broken — so here it's replaced with thread context manipulation
