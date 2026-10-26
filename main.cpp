#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class TaskProcessor {
public:
    explicit TaskProcessor(int workerCount) : finished_(false) {
        for (int i = 1; i <= workerCount; ++i) {
            workers_.emplace_back(&TaskProcessor::worker, this, i);
        }
    }

    TaskProcessor(const TaskProcessor&) = delete;
    TaskProcessor& operator=(const TaskProcessor&) = delete;

    ~TaskProcessor() {
        shutdown();
    }

    void addTask(int taskId) {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (finished_) {
                throw std::runtime_error("Cannot add tasks after shutdown.");
            }
            tasks_.push(taskId);
        }
        condition_.notify_one();
    }

    void waitUntilFinished() {
        shutdown();
    }

private:
    std::queue<int> tasks_;
    std::mutex queueMutex_;
    std::mutex outputMutex_;
    std::condition_variable condition_;
    std::vector<std::thread> workers_;
    bool finished_;

    void worker(int workerId) {
        while (true) {
            int taskId = 0;

            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                condition_.wait(lock, [this] {
                    return !tasks_.empty() || finished_;
                });

                if (tasks_.empty() && finished_) {
                    return;
                }

                taskId = tasks_.front();
                tasks_.pop();
            }

            // Simulate work without holding the queue lock.
            std::this_thread::sleep_for(std::chrono::milliseconds(350));

            {
                std::lock_guard<std::mutex> lock(outputMutex_);
                std::cout << "[Worker " << workerId << "] processed task "
                          << taskId << " | thread id: "
                          << std::this_thread::get_id() << '\n';
            }
        }
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (finished_) {
                return;
            }
            finished_ = true;
        }

        condition_.notify_all();

        for (std::thread& workerThread : workers_) {
            if (workerThread.joinable()) {
                workerThread.join();
            }
        }
    }
};

int main() {
    constexpr int workerCount = 3;
    constexpr int taskCount = 10;

    std::cout << "=== Multithreaded Task Processing System ===\n";
    std::cout << "Starting " << workerCount << " workers...\n\n";

    TaskProcessor processor(workerCount);

    for (int taskId = 1; taskId <= taskCount; ++taskId) {
        processor.addTask(taskId);
    }

    processor.waitUntilFinished();

    std::cout << "\nAll " << taskCount << " tasks completed successfully.\n";
    return 0;
}
