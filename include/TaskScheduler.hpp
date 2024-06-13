#ifndef TASKSCHEDULER_HPP
#define TASKSCHEDULER_HPP

#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <unordered_map>

class Task {
public:

    Task()
            : k_task_id(s_next_id++) {}

    int k_task_id;
    virtual void execute() = 0;
    virtual void stop() {
        // Default implementation
    };
    virtual ~Task() = default;
    inline static std::atomic<int> s_next_id { 0 };
};

class OneTimeTask : public Task {
public:

    explicit OneTimeTask(std::function<void()> a_func)
            : m_task_function(std::move(a_func)) {}

    void execute() override {
        if (!m_task_function) {
            return;
        }
        try {
            m_task_function();
        } catch (...) {
            // Ignore exceptions
        }
    }

private:

    std::function<void()> m_task_function;
};

class RecurringTask : public Task {
public:

    RecurringTask(std::function<void()> a_func,
            std::chrono::milliseconds a_interval,
            std::chrono::milliseconds a_duration = std::chrono::milliseconds(-1))
            : m_task_function(std::move(a_func)),
              m_interval(a_interval),
              m_duration(a_duration),
              m_external_stop_cond(nullptr) {}

    RecurringTask(std::function<void()> func,
            std::chrono::milliseconds interval,
            std::atomic<bool> *a_stop_condition,
            std::chrono::milliseconds duration = std::chrono::milliseconds(-1))
            : m_task_function(std::move(func)),
              m_interval(interval),
              m_duration(duration),
              m_external_stop_cond(a_stop_condition) {}

    // Delete the copy constructor and copy assignment operator
    RecurringTask(const RecurringTask &)            = delete;
    RecurringTask &operator=(const RecurringTask &) = delete;

    // Implement move constructor
    RecurringTask(RecurringTask &&a_other) noexcept
            : m_task_function(std::move(a_other.m_task_function)),
              m_interval(a_other.m_interval),
              m_duration(a_other.m_duration),
              m_stop_requested(a_other.m_stop_requested.load()),
              m_external_stop_cond(a_other.m_external_stop_cond) {}

    // Implement move assignment operator
    RecurringTask &operator=(RecurringTask &&other) noexcept {
        if (this != &other) {
            m_task_function = std::move(other.m_task_function);
            m_interval      = other.m_interval;
            m_duration      = other.m_duration;
            m_stop_requested.store(other.m_stop_requested.load());
            m_external_stop_cond = other.m_external_stop_cond;
        }
        return *this;
    }

    void execute() override {
        if (!m_task_function) {
            return;
        }
        const auto start_time = std::chrono::steady_clock::now();
        while (!isStopped() && (m_duration.count() < 0 || std::chrono::steady_clock::now() - start_time < m_duration)) {
            try {
                m_task_function();
            } catch (...) {
                stop();
                return; // Ignore exceptions
            }
            if (m_external_stop_cond != nullptr && *m_external_stop_cond) {
                stop();
                return;
            }
            std::this_thread::sleep_for(m_interval);
        }
    }

    void stop() override {
        m_stop_requested = true;
    }

    [[nodiscard]] bool isStopped() const {
        return m_stop_requested;
    }

private:

    std::function<void()> m_task_function;
    std::chrono::milliseconds m_interval;
    std::chrono::milliseconds m_duration;
    std::atomic<bool> m_stop_requested = false;
    std::atomic<bool> *m_external_stop_cond;
};

class TaskScheduler {
public:

    explicit TaskScheduler(size_t a_n_threads = std::thread::hardware_concurrency())
            : m_tasks_in_progress(a_n_threads),
              m_running(true) {
        for (size_t i = 0; i < a_n_threads; ++i) {
            m_threads.emplace_back([this, i] { this->taskLoop(i); });
        }
    }

    ~TaskScheduler() {
        stop();
        for (auto &thread : m_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    // Add a task with a specified identifier
    void addTask(std::unique_ptr<Task> a_task) {
        std::lock_guard lock(m_task_mtx);
        m_tasks.try_emplace(a_task->k_task_id, std::move(a_task));
        m_task_cv.notify_one();
    }

    // Stop a specific task by its identifier
    bool stopTask(int a_task_id) {
        std::lock_guard lock(m_task_mtx);
        auto found = m_tasks.find(a_task_id);
        if (found == m_tasks.end()) {
            std::unique_lock lock_progress(m_task_progress_mtx);
            for (auto &t : m_tasks_in_progress) {
                if (t.task && t.task->k_task_id == a_task_id) {
                    t.task->stop();
                    return true;
                }
            }
            return false;
        }
        found->second.task->stop();
        m_tasks.erase(found);
        return true;
    }

    void stop() {
        m_running = false;
        m_task_cv.notify_all();
        std::unique_lock lock(m_task_progress_mtx);
        for (auto &taskEntry : m_tasks_in_progress) {
            if (taskEntry.task) {
                taskEntry.task->stop();
            }
        }
    }

private:

    void taskLoop(int a_thread_index) {
        while (m_running) {
            std::unique_lock lock(m_task_mtx);
            m_task_cv.wait(lock, [this] { return !m_tasks.empty() || !m_running; });
            if (!m_running) {
                return;
            }
            if (m_tasks.empty()) {
                continue;
            }
            auto task_iter = m_tasks.begin();
            {
                std::unique_lock lock_progress(m_task_progress_mtx);
                m_tasks_in_progress[a_thread_index] = std::move(task_iter->second);
            }
            m_tasks.erase(task_iter);
            lock.unlock();
            try {
                m_tasks_in_progress[a_thread_index].task->execute();
            } catch (...) {
                // Ignore exceptions
            }
            {
                std::unique_lock lock_progress(m_task_progress_mtx);
                m_tasks_in_progress[a_thread_index].task.reset();
            }
            lock.lock();
        }
    }

    struct TaskEntry {
        std::unique_ptr<Task> task;

        TaskEntry()
                : task(nullptr) {}

        explicit TaskEntry(std::unique_ptr<Task> a_t)
                : task(std::move(a_t)) {}
    };

    std::vector<std::thread> m_threads;
    std::unordered_map</*task_id*/ int, TaskEntry> m_tasks;
    std::vector<TaskEntry> m_tasks_in_progress;
    std::mutex m_task_mtx;
    std::mutex m_task_progress_mtx;
    std::condition_variable m_task_cv;
    std::atomic<bool> m_running;
};
#endif
