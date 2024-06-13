<img src="https://i.imgur.com/39Fa2Gv.png" alt="Alt Text" width="1640" height="315">


# LambdaTaskScheduler 
(Archived, I don't see needing to keep updating this plus with the little time I have; Though maybe in the future I'll improve it.)

LambdaTaskScheduler is a lightweight C++ task scheduling library that enables asynchronous execution of one-time and recurring tasks. It offers a straightforward interface for managing tasks and executing them on specified intervals.

## Key Features

- **Task Management**: Schedule both one-time and recurring tasks effortlessly.
- **Customizable**: Tailor tasks with adjustable intervals, durations, and support for external stop conditions.
- **Thread-Safe Execution**: Ensures safe concurrent task execution using mutex and condition variables.
- **Dynamic Thread Pool**: Flexible task scheduler with a dynamically adjustable thread pool size.
- **Modern C++ Implementation**: Clean and efficient codebase leveraging modern C++ features.
- **Broad Compatibility**: Designed to work with **C++17**, and **C++20** .

## Installation

To integrate LambdaTaskScheduler into your project, simply include the necessary header files and link against the library.

```cpp
#include "TaskScheduler.hpp"
```

## Usage
# Scheduling a Recurring Task with a Stop Condition

```cpp
int main() {
    // Create a task scheduler with 2 threads
    TaskScheduler scheduler(2);

    // Define a stop condition
    std::atomic<bool> stopCondition = false;

    // Define a recurring task
    auto recurringTask = std::make_unique<RecurringTask>(
        []() {
            std::cout << "Recurring task executed." << std::endl;
        },
        std::chrono::milliseconds(500), // Interval of 500ms
        &stopCondition
    );

    // Add the recurring task to the scheduler
    scheduler.addTask(std::move(recurringTask));

    // Let the recurring task run for 3 seconds
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Signal the stop condition to stop the recurring task
    stopCondition = true;

    // Give some time for the task to stop
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Stop the scheduler
    scheduler.stop();

    return 0;
}
```
# Scheduling a Recurring Task with a Duration
```cpp
int main() {
    // Create a task scheduler with default number of threads
    TaskScheduler scheduler;

    // Define a recurring task with a duration of 2 seconds
    auto recurringTask = std::make_unique<RecurringTask>(
        []() {
            std::cout << "Recurring task executed." << std::endl;
        },
        std::chrono::milliseconds(500), // Interval of 500ms
        std::chrono::milliseconds(2000) // Duration of 2000ms (2 seconds)
    );

    // Add the recurring task to the scheduler
    scheduler.addTask(std::move(recurringTask));

    // Give some time for the task to execute and complete
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Stop the scheduler
    scheduler.stop();

    return 0;
}
```
# Stopping a Specific Task
```cpp
int main() {
    // Create a task scheduler with default number of threads
    TaskScheduler scheduler;

    // Define a recurring task
    auto recurringTask = std::make_unique<RecurringTask>(
        []() {
            std::cout << "Recurring task executed." << std::endl;
        },
        std::chrono::milliseconds(500) // Interval of 500ms
    );

    int taskId = recurringTask->k_task_id;

    // Add the recurring task to the scheduler
    scheduler.addTask(std::move(recurringTask));

    // Let the recurring task run for 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Stop the specific recurring task
    scheduler.stopTask(taskId);

    // Give some time to confirm the task has stopped
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Stop the scheduler
    scheduler.stop();

    return 0;
}
```


## Contributing

We welcome contributions from the community!

- Fork the repository
- Make your changes
- Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
