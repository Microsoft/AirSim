// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef common_utils_Semaphore_hpp
#define common_utils_Semaphore_hpp
#include <memory>

namespace mavlink_utils
{
/*
	A semaphore is used to signal an event across threads.  One thread blocks on Wait() until
	the other thread calls Signal.  It is a counting semaphore so if the thread calls Signal
	before the Wait() then the Wait() does not block.
	*/
class Semaphore
{
public:
    Semaphore();
    ~Semaphore();

    // Increment the semaphore count to unblock the next waiter (the next wait caller will return).
    // Throws exception if an error occurs.
    void post();

    // wait indefinitely for one call to post.  If post has already been called then wait returns immediately
    // decrementing the count so the next wait in the queue will block.  Throws exception if an error occurs.
    void wait();

    // wait for a given number of milliseconds for one call to post.  Returns false if a timeout or EINTR occurs.
    // If post has already been called then timed_wait returns immediately decrementing the count so the next
    // wait will block.  Throws exception if an error occurs.
    bool timed_wait(int milliseconds);

private:
    class semaphore_impl;
    std::unique_ptr<semaphore_impl> impl_;
};
}

#endif
