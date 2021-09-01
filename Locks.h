//
//  Locks.h
//  SonicPlayer_ios
//
//  Created by Andrija Milovanovic on 10/27/17.
//  Copyright © 2017 mandrijatech. All rights reserved.
//

#ifndef Locks_h
#define Locks_h

#include <atomic>

class SpinLock
{
    std::atomic_flag locked = ATOMIC_FLAG_INIT ;
public:
    void lock() {
        while (locked.test_and_set(std::memory_order_acquire)) { ; }
    }
    void unlock() {
        locked.clear(std::memory_order_release);
    }
};

class Locker
{
public:
    Locker(SpinLock& lock) : m_lock(lock) { m_lock.lock(); }
    ~Locker() { m_lock.unlock(); }
private:
    SpinLock& m_lock;
};

#endif /* Locks_h */
