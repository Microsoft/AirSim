#ifndef msr_AirLibUnitTests_WorkerThreadThreadTest_hpp
#define msr_AirLibUnitTests_WorkerThreadThreadTest_hpp

#include "common/WorkerThread.hpp"
#include "common/common_utils/Timer.hpp"
#include <chrono>
#include "TestBase.hpp"

namespace msr
{
namespace airlib
{

    class WorkerThreadTest : public TestBase
    {
        class WorkItem : public CancelableAction
        {
        public:
            double runTime = 2; // seconds

            virtual void executeAction() override
            {
                common_utils::Timer timer;
                timer.start();
                while (!this->isCancelled()) {
                    counter_++;
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                    if (timer.seconds() > runTime) {
                        break;
                    }
                }
            }
            void reset(bool reset_counter = true)
            {
                // reset for next test
                if (reset_counter)
                    counter_ = 0;
                CancelableAction::reset();
            }
            unsigned int getCount()
            {
                return counter_;
            }

        private:
            std::atomic<unsigned int> counter_;
        };

    public:
        virtual void run() override
        {
            WorkerThread thread;

            std::shared_ptr<WorkItem> item1 = std::make_shared<WorkItem>();
            std::shared_ptr<WorkItem> item2 = std::make_shared<WorkItem>();

            thread.enqueue(item1);

            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            unsigned int item1count = item1->getCount();

            // cancel item1 and switch to item2
            thread.enqueue(item2);
            unsigned int item1count2 = item1->getCount();

            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            unsigned int item1count3 = item1->getCount();
            unsigned int item2count = item2->getCount();

            testAssert(item1count2 == item1count3, "enqueue operation did not cancelled previous task");
            testAssert(item1count > 1, "item1 call count is too low");
            testAssert(item2count > 1, "item2 call count is too low");

            // make sure we can cancel.
            thread.cancel();
            unsigned int item2count2 = item2->getCount();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            item2count = item2->getCount();

            testAssert(item2count2 == item2count, "cancel operation did not worked");

            // now stress the thread switching...(this found threading bugs in CallLock!)
            for (size_t i = 0; i < 100; i++) {
                item1->reset(false);
                thread.enqueue(item1);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                item2->reset(false);
                thread.enqueue(item2);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            unsigned int item1count4 = item1->getCount();
            unsigned int item2count4 = item2->getCount();

            testAssert(item1count4 > item1count2 + 50, "item1 is getting starved");
            testAssert(item2count4 > item2count2 + 50, "item2 is getting starved");

            // test that enqueueAndWait works.
            item1->reset();
            item1->runTime = 2; // two seconds

            common_utils::Timer timer;
            timer.start();
            thread.enqueueAndWait(item1, 0.5);
            double elapsed = timer.seconds();
            testAssert(elapsed >= 0.5 && elapsed < 1 && item1->isCancelled() && !item1->isComplete(),
                       "enqueueAndWait waited too long, should have timed out");

            item2->reset();
            item2->runTime = 0.5; // half a second
            timer.start();
            thread.enqueueAndWait(item2, 2);
            elapsed = timer.seconds();
            testAssert(elapsed >= 0.5 && elapsed < 1 && !item2->isCancelled() && item2->isComplete(),
                       "enqueueAndWait waited too long, task should have completed in 0.5 seconds");
        }
    };
}
}
#endif