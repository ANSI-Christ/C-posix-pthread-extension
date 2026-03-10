# C posix pthread extension

An extension for POSIX pthread: thread pool, channel, group.
  - cross-platform
  - a simple and intuitive API
  - flexible

Look for usage examples in main.c

## 🔧 Possible User Modifications for pthread_pool

The library can be extended or modified in the following ways to suit your specific needs:

1. **Return `int` instead of `void`** — Modify tasks to return an integer value for return code analysis at the execution point. This would allow the caller to decide whether to retain the task's memory or free it based on the result.

2. **Per-task waiting API** — In combination with point 1, implement a platform-dependent futex-like mechanism to wait for individual task completion. This would enable more fine-grained synchronization.

3. **Stackless coroutine support** — Also building on point 1, add an additional API to integrate with stackless coroutines (like C++20 coroutines), allowing tasks to suspend and resume efficiently.
