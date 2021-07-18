// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef commn_utils_AsyncTasker_hpp
#define commn_utils_AsyncTasker_hpp

#include "ctpl_stl.h"
#include <functional>

class AsyncTasker
{
public:
    AsyncTasker(unsigned int thread_count = 4)
        : threads_(thread_count), error_handler_([](std::exception e) { unused(e); })
    {
    }

    void setErrorHandler(std::function<void(std::exception&)> errorHandler)
    {
        error_handler_ = errorHandler;
    }

    void execute(std::function<void()> func, unsigned int iterations = 1)
    {
        if (iterations < 1)
            return;

        if (iterations == 1) {
            threads_.push([=](int i) {
                unused(i);
                try {
                    func();
                }
                catch (std::exception& e) {
                    error_handler_(e);
                };
            });
        }
        else {
            threads_.push([=](int i) {
                unused(i);
                try {
                    for (unsigned int itr = 0; itr < iterations; ++itr) {
                        func();
                    }
                }
                catch (std::exception& e) {
                    // if task failed we shouldn't try additional iterations.
                    error_handler_(e);
                };
            });
        }
    }

private:
    ctpl::thread_pool threads_;
    std::function<void(std::exception&)> error_handler_;
};

#endif
