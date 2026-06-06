#pragma once

// A minimal test-and-set spinlock.
//
// NOTE: on a single CPU, a task preempted while holding the lock will stall
// any spinner until round-robin lets the holder run again and release it.
// Correct but wasteful; a production kernel disables preemption while holding
// a lock. Fine here for short critical sections.
struct spinlock {
    volatile int locked;
};

static inline void spin_lock(spinlock* lock) {
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        asm volatile("pause");
    }
}

static inline void spin_unlock(spinlock* lock) {
    __sync_lock_release(&lock->locked);
}
