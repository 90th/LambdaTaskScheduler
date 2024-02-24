#ifndef TASKSCHEDULER_HPP
#define TASKSCHEDULER_HPP

#include <iostream>
#include <functional>
#include <thread>
#include <queue>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

class TaskExecutionError : public std::runtime_error {
public:
	inline TaskExecutionError(const std::string& message)
		: std::runtime_error(message) {}
};

class Task {
public:
	virtual void execute() = 0;
	virtual ~Task() = default;
};

class OneTimeTask : public Task {
private:
	std::function<void()> taskFunction;

public:
	inline OneTimeTask(std::function<void()> func) : taskFunction(std::move(func)) {}

	inline void execute() override {
		if (taskFunction) {
			try {
				taskFunction();
			}
			catch (const std::exception& e) {
				throw TaskExecutionError(std::string("OneTimeTask execution failed: ") + e.what());
			}
		}
	}
};

class RecurringTask : public Task {
private:
	int taskId; // Identifier for the task
	std::function<void()> taskFunction;
	std::chrono::milliseconds interval;
	std::chrono::milliseconds duration;
	std::atomic<bool> stopRequested = false;
	std::atomic<bool>* externalStopCondition;
	mutable std::mutex mutex;
	std::condition_variable cv;

public:
	RecurringTask(int id, std::function<void()> func, std::chrono::milliseconds interval, std::chrono::milliseconds duration = std::chrono::milliseconds(-1))
		: taskId(id), taskFunction(std::move(func)), interval(interval), duration(duration), externalStopCondition(nullptr) {}

	RecurringTask(int id, std::function<void()> func, std::chrono::milliseconds interval, std::atomic<bool>* stopCondition, std::chrono::milliseconds duration = std::chrono::milliseconds(-1))
		: taskId(id), taskFunction(std::move(func)), interval(interval), duration(duration), externalStopCondition(stopCondition) {}

	// Delete the copy constructor and copy assignment operator
	RecurringTask(const RecurringTask&) = delete;
	RecurringTask& operator=(const RecurringTask&) = delete;

	// Implement move constructor
	RecurringTask(RecurringTask&& other) noexcept
		: taskId(other.taskId),
		taskFunction(std::move(other.taskFunction)),
		interval(other.interval),
		duration(other.duration),
		stopRequested(other.stopRequested.load()),
		externalStopCondition(other.externalStopCondition) {}

	// Implement move assignment operator
	RecurringTask& operator=(RecurringTask&& other) noexcept {
		if (this != &other) {
			taskId = other.taskId;
			taskFunction = std::move(other.taskFunction);
			interval = other.interval;
			duration = other.duration;
			stopRequested.store(other.stopRequested.load());
			externalStopCondition = other.externalStopCondition;
		}
		return *this;
	}

	inline void execute() override {
		auto startTime = std::chrono::steady_clock::now();
		while (!isStopped() && (duration.count() < 0 || std::chrono::steady_clock::now() - startTime < duration)) {
			if (taskFunction) {
				try {
					taskFunction();
				}
				catch (const std::exception& e) {
					throw TaskExecutionError(std::string("RecurringTask execution failed: ") + e.what());
				}
			}
			if (externalStopCondition != nullptr && *externalStopCondition) {
				stop();
				break;
			}

			std::this_thread::sleep_for(interval);
		}
	}

	inline void stop() {
		stopRequested = true;
	}

	inline bool isStopped() const {
		return stopRequested;
	}

	inline int getId() const { // Getter for task identifier
		return taskId;
	}
};

class TaskScheduler {
private:
	struct TaskEntry {
		std::unique_ptr<Task> task;

		inline TaskEntry() : task(nullptr) {} // Default constructor to initialize task to nullptr

		inline TaskEntry(std::unique_ptr<Task> t)
			: task(std::move(t)) {}
	};

	std::vector<std::thread> threads;
	std::unordered_map<int, TaskEntry> taskMap; // Map to store tasks with identifiers
	std::mutex taskMutex;
	std::condition_variable taskCV;
	std::atomic<bool> running;

public:
	inline TaskScheduler(size_t numThreads = std::thread::hardware_concurrency())
		: running(true) {
		for (size_t i = 0; i < numThreads; ++i) {
			threads.emplace_back([this] { this->taskLoop(); });
		}
	}

	inline ~TaskScheduler() {
		running = false;
		taskCV.notify_all();
		for (auto& thread : threads) {
			thread.join();
		}
	}

	// Add a task with a specified identifier
	inline void addTask(int taskId, std::unique_ptr<Task> task) {
		std::lock_guard<std::mutex> lock(taskMutex);
		taskMap[taskId] = TaskEntry(std::move(task));
		taskCV.notify_one();
	}

	// Stop a specific task by its identifier
	inline void stopTask(int taskId) {
		std::lock_guard<std::mutex> lock(taskMutex);
		taskMap.erase(taskId);
	}

	inline void stop() {
		running = false;
		taskCV.notify_all();
	}

private:
	inline void taskLoop() {
		while (running) {
			std::unique_lock<std::mutex> lock(taskMutex);
			taskCV.wait(lock, [this] {
				return !taskMap.empty() || !running;
				});
			if (!running) return;

			if (!taskMap.empty()) {
				auto entry = std::move(taskMap.begin()->second); // Get the first task
				taskMap.erase(taskMap.begin()); // Remove the task from the map
				lock.unlock();
				try {
					entry.task->execute();
				}
				catch (const std::exception& e) {
					// Log or handle the error appropriately
					// For now, rethrow the exception
					throw TaskExecutionError(std::string("Error executing task: ") + e.what());
				}
			}
		}
	}
};

#endif