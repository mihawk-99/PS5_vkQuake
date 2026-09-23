# Parked: a lock-free SDL semaphore

`semaphore.patch` (against port 7a789e9) makes the shim's semaphore count atomic:
try-wait is a CAS with no mutex, a post takes the mutex and signals only when a
waiter has announced itself as a sleeper, and a wait spins ~20 us (TSC) before
it sleeps. The host test gains a 400,000-unit multi-producer/consumer stress and
a slow-post phase that forces real sleeps; it passes normally and under
ThreadSanitizer, and a mutant whose post never wakes a sleeper hangs it.

Why parked, not landed: on the console (evidence `m6-r39-semaphore`, VRR on,
against `m6-r38-marker-spin`) it changed E1M1 55.76 -> 55.92 FPS (work 17.74
-> 17.69 ms) and the start map 33.08 -> 33.43 FPS (work 30.00 -> 29.69), within
run-to-run noise. Contended locks fell 153 -> 113 a frame, but the 81 blocking
waits a frame stayed: they are idle workers waiting for the next frame's tasks,
not the frame's critical path. Not worth the concurrency risk for no measured
gain. Apply with `git apply parked/sdl-semaphore/semaphore.patch` if a later
measurement puts the task system on the critical path.
