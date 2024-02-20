# LambdaTaskScheduler

LambdaTaskScheduler is a C++ task scheduling library that enables asynchronous execution of one-time and recurring tasks. It offers a straightforward interface for managing tasks and executing them on specified intervals.

## Key Features

- **Task Management**: Schedule both one-time and recurring tasks effortlessly.
- **Customizable**: Tailor tasks with adjustable intervals, durations, and support for external stop conditions.
- **Thread-Safe Execution**: Ensures safe concurrent task execution using mutex and condition variables.
- **Dynamic Thread Pool**: Flexible task scheduler with a dynamically adjustable thread pool size.
- **Modern C++ Implementation**: Clean and efficient codebase leveraging modern C++ features.

## Installation

To integrate LambdaTaskScheduler into your project, simply include the necessary header files and link against the library.

```cpp
#include "TaskScheduler.h"
```

## Usage

### Creating Tasks

Define tasks using the provided `Task` interface. Implement one-time tasks and recurring tasks with ease.

```cpp
// Define a one-time task
auto oneTimeTask = std::make_unique<OneTimeTask>([] {
    // Task logic here
    std::cout << "Executing one-time task" << std::endl;
});

// Define a recurring task with a specified interval and duration
auto recurringTask = std::make_unique<RecurringTask>([] {
    // Task logic here
    std::cout << "Executing recurring task" << std::endl;
}, std::chrono::milliseconds(1000), std::chrono::milliseconds(5000)); // Interval of 1 second, duration of 5 seconds

// Define a recurring task with an external stop condition
std::atomic<bool> stopFlag(false);
auto externalStopConditionTask = std::make_unique<RecurringTask>([] {
    // Task logic here
    std::cout << "Executing task with external stop condition" << std::endl;
}, std::chrono::milliseconds(1000), &stopFlag); // Interval of 1 second, external stop condition
```

### Scheduling Tasks

Add tasks to the task scheduler to initiate their execution.

```cpp
TaskScheduler scheduler;
scheduler.addTask(std::move(oneTimeTask));
scheduler.addTask(std::move(recurringTask));
scheduler.addTask(std::move(externalStopConditionTask));
```

### Stopping Tasks

Stop recurring tasks when they are no longer needed or when an external condition is met.

```cpp
scheduler.stopRecurringTasks(); // Stop all recurring tasks

// Alternatively, stop a specific recurring task when an external condition is met
// For example, when a stop condition atomic flag is set
stopFlag.store(true);
```

### Stopping the Scheduler

Terminate all running tasks by stopping the task scheduler.

```cpp
scheduler.stop();
```

## Example

Here's a simple example demonstrating the usage of LambdaTaskScheduler:

```cpp
#include "TaskScheduler.h"

int main() {
    TaskScheduler scheduler;

    // Add tasks to the scheduler
    scheduler.addTask(std::make_unique<OneTimeTask>([] {
        std::cout << "Executing one-time task" << std::endl;
    }));

    scheduler.addTask(std::make_unique<RecurringTask>([] {
        std::cout << "Executing recurring task" << std::endl;
    }, std::chrono::milliseconds(1000), std::chrono::milliseconds(5000))); // Interval of 1 second, duration of 5 seconds

    std::atomic<bool> stopFlag(false);
    scheduler.addTask(std::make_unique<RecurringTask>([] {
        std::cout << "Executing task with external stop condition" << std::endl;
    }, std::chrono::milliseconds(1000), &stopFlag)); // Interval of 1 second, external stop condition

    // Wait for tasks to complete
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Stop the scheduler
    scheduler.stop();

    return 0;
}
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
