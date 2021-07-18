// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "Semaphore.hpp"
#include "Utils.hpp"

using namespace mavlink_utils;

#ifdef _WIN32
#include <Windows.h>

class Semaphore::semaphore_impl
{
    HANDLE semaphore;

public:
    semaphore_impl()
    {
        this->semaphore = CreateSemaphore(NULL, 0, MAXINT32, NULL);
        if (this->semaphore == NULL) {
            throw std::runtime_error(Utils::stringf("CreateSemaphore failed with errno=%d!\n", GetLastError()));
        }
    }

    ~semaphore_impl()
    {
        CloseHandle(semaphore);
        semaphore = nullptr;
    }

    void post()
    {
        LONG previous;
        if (0 == ReleaseSemaphore(semaphore, 1, &previous)) {
            throw std::runtime_error(Utils::stringf("ReleaseSemaphore failed with errno=%d!\n", GetLastError()));
        }
    }

    // WaitOne indefinitely for one Signal.  If a Signal has already been posted then WaitOne returns immediately
    // decrementing the count so the next WaitOne may block.
    void wait()
    {
        int rc = WaitForSingleObjectEx(semaphore, INFINITE, TRUE);
        if (rc == WAIT_OBJECT_0) {
            return;
        }
        else {
            throw std::runtime_error(Utils::stringf("WaitForSingleObjectEx failed with unexpected errno=%d!\n", GetLastError()));
        }
    }

    bool timed_wait(int milliseconds)
    {
        int rc = WaitForSingleObjectEx(semaphore, milliseconds, TRUE);
        if (rc == WAIT_OBJECT_0) {
            return true;
        }
        else if (rc == WAIT_ABANDONED) {
            throw std::runtime_error("Semaphore was destroyed while we are waiting on it.");
        }
        else if (rc != WAIT_TIMEOUT) {
            // perhaps we have WAIT_IO_COMPLETION interrupt...
            throw std::runtime_error(Utils::stringf("WaitForSingleObjectEx failed with errno=%d!\n", GetLastError()));
        }
        return false;
    }
};

#elif defined(__APPLE__)
#include <signal.h> //SIGALRM
#include <semaphore.h>
#include <mach/mach.h>
#include <mach/task.h>
#include <mach/semaphore.h>

class Semaphore::semaphore_impl
{
    semaphore_t semaphore;
    task_t owner;

public:
    semaphore_impl()
    {
        owner = mach_task_self();
        kern_return_t rc = semaphore_create(owner, &semaphore, /* policy */ SYNC_POLICY_FIFO, /* value*/ 0);
        if (rc != KERN_SUCCESS) {
            throw std::runtime_error(Utils::stringf("OSX semaphore_create failed with errno %d\n", rc));
        }
    }

    ~semaphore_impl()
    {
        semaphore_destroy(owner, semaphore);
    }

    void post()
    {
        kern_return_t rc = semaphore_signal(semaphore);
        if (rc != KERN_SUCCESS) {
            throw std::runtime_error(Utils::stringf("OSX semaphore_signal failed with error %d\n", rc));
        }
    }

    void wait()
    {
        kern_return_t rc = semaphore_wait(semaphore);
        if (rc != KERN_SUCCESS) {
            throw std::runtime_error(Utils::stringf("OSX semaphore_wait failed with unexpected error %d\n", rc));
        }
    }

    bool timed_wait(int milliseconds)
    {
        // convert to absolute time.
        if (milliseconds < 0) {
            throw std::runtime_error("cannot wait for negative milliseconds");
        }
        auto absolute = std::chrono::system_clock::now().time_since_epoch() + std::chrono::milliseconds(milliseconds);
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(absolute);
        absolute = absolute - seconds;
        auto nanoSecondsRemaining = std::chrono::duration_cast<std::chrono::nanoseconds>(absolute);

        // use mach_timespec
        mach_timespec_t mts;
        mts.tv_nsec = nanoSecondsRemaining.count(); // nanoseconds
        mts.tv_sec = seconds.count(); // seconds

        kern_return_t rc = semaphore_timedwait(semaphore, mts);

        switch (rc) {
        case KERN_SUCCESS:
            return true;
        case KERN_OPERATION_TIMED_OUT:
            return false;
        case KERN_ABORTED:
            throw std::runtime_error("OSX semaphore_timedwait was aborted");
        default:
            throw std::runtime_error(Utils::stringf("OSX semaphore_timedwait failed with unexpected error %d", rc));
        }
    }
};
#else // assume posix

#include <semaphore.h>

class Semaphore::semaphore_impl
{
    sem_t semaphore;

public:
    semaphore_impl()
    {
        if (sem_init(&semaphore, /* shared */ 0, /* value*/ 0) == -1) {
            throw std::runtime_error(Utils::stringf("sem_init failed with errno=%d!\n", errno));
        }
    }

    ~semaphore_impl()
    {
        sem_destroy(&semaphore);
    }

    void post()
    {
        int status = sem_post(&semaphore);
        if (status < 0) {
            throw std::runtime_error(Utils::stringf("sem_post failed with errno=%d!\n", errno));
        }
    }

    void wait()
    {
        int rc = sem_wait(&semaphore);
        if (rc != 0) {
            throw std::runtime_error(Utils::stringf("sem_wait failed with unexpected errno=%d!\n", rc));
        }
    }

    bool timed_wait(int milliseconds)
    {
        // convert to absolute time.
        if (milliseconds < 0) {
            throw std::runtime_error("cannot wait for negative milliseconds");
        }
        auto absolute = std::chrono::system_clock::now().time_since_epoch() + std::chrono::milliseconds(milliseconds);
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(absolute);
        absolute = absolute - seconds;
        auto nanoSecondsRemaining = std::chrono::duration_cast<std::chrono::nanoseconds>(absolute);
        struct timespec ts;
        ts.tv_sec = seconds.count(); // seconds
        milliseconds -= (ts.tv_sec * 1000);
        ts.tv_nsec = nanoSecondsRemaining.count(); // nanoseconds
        int rc = sem_timedwait(&semaphore, &ts);
        if (rc != 0) {
            rc = errno;
            if (rc == ETIMEDOUT) {
                return false;
            }
            else if (rc != EINTR) {
                throw std::runtime_error(Utils::stringf("sem_timedwait failed with unexpected errno=%d!\n", rc));
            }
        }
        return rc == 0;
    }
};
#endif

Semaphore::Semaphore()
{

    impl_.reset(new semaphore_impl());
}

Semaphore::~Semaphore()
{
}

void Semaphore::wait()
{
    impl_->wait();
}

void Semaphore::post()
{
    impl_->post();
}

bool Semaphore::timed_wait(int millisecondTimeout)
{
    return impl_->timed_wait(millisecondTimeout);
}
