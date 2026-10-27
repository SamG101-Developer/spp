---
name: spp-green-task-detach-uaf
description: "detaching a green task and freeing its result cell is a guaranteed use-after-free, unlike the same shape on a pthread"
---

`Fut::drop` and `JoinHandle::drop` look identical - detach the task, release the cell it writes its result into - but
they are not equally safe, and the difference is the scheduler.

A pthread starts running the moment `pthread_create` returns, so by the time a handle is dropped the thread has
usually already written the cell; the shape is racy but rarely bites. **A green task does not begin until the
scheduler reaches it**, and nothing between `spawn` and `drop` ever yields, so a future dropped without being awaited
is *guaranteed* to run afterwards and write its result through a pointer that was freed. That is a use-after-free on
every execution, not occasionally.

It shows up far from the cause. `spp test --lib std --filter future` aborted with
`malloc(): unaligned tcache chunk detected` inside an unrelated later test, and the corrupting test passed. Each test
passed when run alone - the harness even says so ("The suite failed as a whole but every test passes alone") - which
is the tell that it is heap state carried between tests rather than any one of them.

**So a dropped future awaits.** `Fut::drop` waits for the task, drops the value it produced, then releases the cell.
That also stops the value leaking, which matters as soon as `T` owns anything. `sppc_async_detach` still exists and
is correct at the c level; it is simply not usable by anything holding a caller-allocated cell.

Worth remembering when reaching for valgrind on generated code: it does not understand the stack switching, warns
"client switching stacks?" and then reports the runtime's own stack priming as invalid writes. Those are false
positives; the glibc message is the real signal.

Related: [[sppc-sibling-repo]], [[spp-test-harness-stale-std]].
