#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

// Class that represents a simple thread pool
class ThreadPool
{
public:
    ThreadPool(size_t num_threads = std::thread::hardware_concurrency())
    {

        for (size_t i = 0; i < num_threads; ++i)
        {
            auto main_job = [this]
            {
                while (true)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        auto wait_job = [this]
                        {
                            return !tasks_.empty() || stop_;
                        };
                        cv_.wait(lock, wait_job);

                        if (stop_ && tasks_.empty())
                        {
                            return;
                        }

                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }

                    task();
                }
            };
            threads_.emplace_back(main_job);
        }
    }

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }

        cv_.notify_all();

        for (auto &thread : threads_)
        {
            thread.join();
        }
    }

    void enqueue(std::function<void()> task)
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            tasks_.emplace(move(task));
        }
        cv_.notify_one();
    }

private:
    std::vector<std::thread> threads_;

    std::queue<std::function<void()>> tasks_;

    std::mutex queue_mutex_;

    std::condition_variable cv_;

    bool stop_ = false;
};

ThreadPool thread_pool;

#endif // THREAD_POOL_H_