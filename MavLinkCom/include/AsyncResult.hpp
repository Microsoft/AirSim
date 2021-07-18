// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef MavLinkCom_AsyncResult_hpp
#define MavLinkCom_AsyncResult_hpp

#include "Semaphore.hpp"
#include <functional>
#include <chrono>
#include <memory>
#include <string>
#include <future>

namespace mavlinkcom
{

template <class T>
class AsyncResultState
{
public:
    typedef std::function<void(int state)> CompletionHandler;
    mavlink_utils::Semaphore resultReceived_;
    T result_;
    CompletionHandler owner_;
    int state_ = 0;
    bool completed_ = false;

    AsyncResultState(CompletionHandler owner)
    {
        owner_ = owner;
    }

    ~AsyncResultState()
    {
        complete();
    }

    int getState()
    {
        return state_;
    }
    void setState(int s)
    {
        state_ = s;
    }
    T getResult()
    {
        resultReceived_.wait();
        return result_;
    }
    void setResult(T result)
    {
        result_ = result;
        resultReceived_.post();
        complete();
    }
    bool getResult(int millisecondTimeout, T* r)
    {
        if (!resultReceived_.timed_wait(millisecondTimeout)) {
            // timeout on wait, so complete the task in this case too.
            complete();
            return false;
        }
        *r = result_;
        return true;
    }
    void complete()
    {
        CompletionHandler rh = owner_;
        owner_ = nullptr;

        completed_ = true;
        if (rh != nullptr) {
            rh(state_);
        }
    }
    bool isCompleted() { return completed_; }
};

template <class T>
class AsyncResult
{
public:
    typedef std::function<void(T result)> ResultHandler;
    typedef std::function<void(int state)> CompletionHandler;

    AsyncResult(CompletionHandler owner)
    {
        state_ = std::make_shared<AsyncResultState<T>>(owner);
    }
    ~AsyncResult()
    {
        state_ = nullptr;
    }

    static AsyncResult<T> Completed(T state)
    {
        AsyncResult<T> r(nullptr);
        r.setResult(state);
        return r;
    }

    void then(ResultHandler handler)
    {
        // keep state alive while we wait for async result.
        std::shared_ptr<AsyncResultState<T>> safe(state_);
        T result = std::async(std::launch::async, getResult, safe).get();
        handler(result);
    }

    int getState()
    {
        return state_->getState();
    }

    void setState(int s) const
    {
        state_->setState(s);
    }
    void setResult(T result) const
    {
        if (state_ != nullptr) {
            state_->setResult(result);
        }
    }

    AsyncResult(const AsyncResult<T>& other)
    {
        this->state_ = other.state_;
    }

    AsyncResult<T>& operator=(const AsyncResult<T>& other)
    {
        if (this != &other) {
            this->state_ = other.state_;
        }
        return *this;
    }

    AsyncResult(AsyncResult<T>&& other)
    {
        this->state_ = other.state_;
        other.state_ = nullptr;
    }

    AsyncResult<T>& operator=(AsyncResult<T>&& other)
    {
        if (this != &other) {
            this->state_ = other.state_;
            other.state_ = nullptr;
        }
        return *this;
    }

    bool wait(int millisecondTimeout, T* r)
    {
        // keep the state alive while we wait.
        std::shared_ptr<AsyncResultState<T>> safe(state_);
        return state_->getResult(millisecondTimeout, r);
    }

    bool isCompleted()
    {
        return state_->isCompleted();
    }

private:
    static T getResult(std::shared_ptr<AsyncResultState<T>> state)
    {
        return state->getResult();
    }

    std::shared_ptr<AsyncResultState<T>> state_;
};
}

#endif
