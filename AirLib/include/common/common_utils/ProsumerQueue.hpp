// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef commn_utils_ProsumerQueue_hpp
#define commn_utils_ProsumerQueue_hpp

#include <queue>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace common_utils
{

/*
    This queue can support multiple producers consumers, but it should be used carefully 
    because its a *blocking* queue after all. Do not treat as black box, read the code for
    what and how its doing so you know what will happen. There are non-blocking versions available
    out there but more often than not they are buggy. This one is simpler and effortlessly cross-platform.

    In multi-consumer scenario, ideally the consumer
    thread that does the pop, should also spend time on working on poped item instead of
    handing over item to some other thread and going back to another pop right away. If you
    do that then other consumer threads might starve and become pointless. Similarly
    in multi-producer scenario, the thread doing the push should not immediately come back to
    push. So ideally, producer and consumer threads should also perform any time consuming tasks
    such as I/O so overall throughput of multi-producer/multi-consumer is maximized. You can tune 
    the number of thread so queue size doesn't grow out of bound. If you have
    only one producer and oner consumer than it might be better idea to do time consuming stuff
    such as I/O on sepratae threads so queue doesn't become too large.
*/

template <typename T>
class ProsumerQueue
{
public:
    ProsumerQueue()
    {
        is_done_ = false;
    }

    T pop()
    {
        std::unique_lock<std::mutex> global_lock(mutex_);
        while (queue_.empty()) {
            //this may be spurious wake-up
            //in multi-consumer scenario
            cond_.wait(global_lock);
        }
        auto val = queue_.front();
        queue_.pop();
        return val;
    }

    bool tryPop(T& item)
    {
        std::unique_lock<std::mutex> global_lock(mutex_);
        if (queue_.empty())
            return false;

        item = queue_.front();
        queue_.pop();
        return true;
    }

    void clear()
    {
        std::unique_lock<std::mutex> global_lock(mutex_);
        return queue_.clear();
    }

    size_t size() const
    {
        std::unique_lock<std::mutex> global_lock(mutex_);
        return queue_.size();
    }

    bool empty() const
    {
        std::unique_lock<std::mutex> global_lock(mutex_);
        return queue_.empty();
    }

    void push(const T& item)
    {
        std::unique_lock<std::mutex> global_lock(mutex_);
        queue_.push(item);
        global_lock.unlock();
        cond_.notify_one();
    }

    //is_done_ flag is just convinience flag for external use
    //its not used by this class
    bool getIsDone()
    {
        return is_done_;
    }
    void setIsDone(bool val)
    {
        is_done_ = val;
    }

    // non-copiable
    ProsumerQueue(const ProsumerQueue&) = delete;
    ProsumerQueue& operator=(const ProsumerQueue&) = delete;

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> is_done_;
};
}
#endif
