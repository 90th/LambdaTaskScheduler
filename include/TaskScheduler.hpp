#ifndef TASKSCHEDULER_HPP
#define TASKSCHEDULER_HPP

#include <iostream>
#include <functional>
#include <thread>
#include <queue>
#include <atomic>
#include <mutex>
#include <stdexcept>

class TaskExecutionError : public std::runtime_error {
public:
	TaskExecutionError(const std::string& message)
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
	OneTimeTask(std::function<void()> func) : taskFunction(std::move(func)) {}

	void execute() override {
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
	std::function<void()> taskFunction;
	std::chrono::milliseconds interval;
	std::chrono::milliseconds duration;
	std::atomic<bool> stopRequested = false;
	std::atomic<bool>* externalStopCondition;
	mutable std::mutex mutex;
	std::condition_variable cv;

public:
	RecurringTask(std::function<void()> func, std::chrono::milliseconds interval, std::chrono::milliseconds duration = std::chrono::milliseconds(-1))
		: taskFunction(std::move(func)), interval(interval), duration(duration), externalStopCondition(nullptr) {}

	RecurringTask(std::function<void()> func, std::chrono::milliseconds interval, std::atomic<bool>* stopCondition, std::chrono::milliseconds duration = std::chrono::milliseconds(-1))
		: taskFunction(std::move(func)), interval(interval), duration(duration), externalStopCondition(stopCondition) {}

	void execute() override {
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
	void stop() {
		stopRequested = true;
	}
	bool isStopped() const {
		return stopRequested;
	}
};

class TaskScheduler {
private:
	struct TaskEntry {
		std::unique_ptr<Task> task;

		TaskEntry(std::unique_ptr<Task> t)
			: task(std::move(t)) {}
	};

	std::vector<std::thread> threads;
	std::queue<TaskEntry> taskQueue;
	std::mutex taskMutex;
	std::condition_variable taskCV;
	std::atomic<bool> running;

public:
	TaskScheduler(size_t numThreads = std::thread::hardware_concurrency())
		: running(true) {
		for (size_t i = 0; i < numThreads; ++i) {
			threads.emplace_back([this] { this->taskLoop(); });
		}
	}

	~TaskScheduler() {
		running = false;
		taskCV.notify_all();
		for (auto& thread : threads) {
			thread.join();
		}
	}

	void addTask(std::unique_ptr<Task> task) {
		std::lock_guard<std::mutex> lock(taskMutex);
		taskQueue.emplace(std::move(task));
		taskCV.notify_one();
	}

	void stopRecurringTasks() {
		std::lock_guard<std::mutex> lock(taskMutex);
		while (!taskQueue.empty()) {
			taskQueue.pop();
		}
	}

	void stop() {
		running = false;
		taskCV.notify_all();
	}

private:
	void taskLoop() {
		while (running) {
			std::unique_lock<std::mutex> lock(taskMutex);
			taskCV.wait(lock, [this] {
				return !taskQueue.empty() || !running;
				});
			if (!running) return;

			if (!taskQueue.empty()) {
				auto entry = std::move(taskQueue.front());
				taskQueue.pop();
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
			else {
				lock.unlock();
				continue;
			}
		}
	}
};

#endif