---
name: cpp-development
description: Guide for modern C++17/20 development with best practices. Use when writing C++ code, implementing classes, managing memory with smart pointers, working with STL containers/algorithms, handling errors, or structuring C++ projects. Covers RAII patterns, move semantics, templates, and CMake build configuration.
---

# Modern C++ Development

## Overview

Modern C++ (C++17/20) emphasizes safety, expressiveness, and performance through RAII, smart pointers, and zero-cost abstractions.

## Memory Management

### Smart Pointers (Never use raw new/delete)

```cpp
// Unique ownership
auto widget = std::make_unique<Widget>(args...);

// Shared ownership (use sparingly)
auto shared = std::make_shared<Resource>(args...);

// Weak reference to shared
std::weak_ptr<Resource> weak = shared;

// Observer pattern: raw pointer for non-owning
Widget* observer = widget.get();  // Does not own
```

### RAII Pattern

```cpp
class FileHandle {
    std::FILE* file_;
public:
    explicit FileHandle(const char* path) : file_(std::fopen(path, "r")) {
        if (!file_) throw std::runtime_error("Failed to open file");
    }
    ~FileHandle() { if (file_) std::fclose(file_); }

    // Prevent copies
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    // Allow moves
    FileHandle(FileHandle&& other) noexcept : file_(other.file_) {
        other.file_ = nullptr;
    }
    FileHandle& operator=(FileHandle&& other) noexcept {
        if (this != &other) {
            if (file_) std::fclose(file_);
            file_ = other.file_;
            other.file_ = nullptr;
        }
        return *this;
    }

    std::FILE* get() const { return file_; }
};
```

## Class Design

### Rule of Zero/Five

```cpp
// Rule of Zero: Let compiler generate everything
class SimpleClass {
    std::string name_;
    std::vector<int> data_;
    std::unique_ptr<Resource> resource_;
};

// Rule of Five: If you define one, define all
class ManagedResource {
public:
    ManagedResource();
    ~ManagedResource();
    ManagedResource(const ManagedResource&);
    ManagedResource& operator=(const ManagedResource&);
    ManagedResource(ManagedResource&&) noexcept;
    ManagedResource& operator=(ManagedResource&&) noexcept;
};
```

### Modern Constructors

```cpp
class Config {
    std::string name_;
    int value_;
public:
    // Take by value and move
    explicit Config(std::string name, int value = 0)
        : name_(std::move(name)), value_(value) {}

    // Getters: return by const ref for expensive types
    const std::string& name() const { return name_; }
    int value() const { return value_; }

    // Setters: take by value and move
    void set_name(std::string name) { name_ = std::move(name); }
};
```

## STL Containers

### Choosing Containers

| Container | Use When |
|-----------|----------|
| `vector` | Default choice, contiguous memory |
| `array` | Fixed size known at compile time |
| `string` | Text data |
| `unordered_map` | Key-value with O(1) average lookup |
| `map` | Key-value with ordered iteration |
| `unordered_set` | Unique elements, O(1) lookup |
| `deque` | Frequent front/back insertion |
| `list` | Frequent mid-sequence insertion |

### Common Operations

```cpp
std::vector<int> v = {1, 2, 3, 4, 5};

// Reserve to avoid reallocations
v.reserve(100);

// Emplace (construct in-place)
v.emplace_back(6);

// Range-based for
for (const auto& item : v) { /* read-only */ }
for (auto& item : v) { /* modify */ }

// Structured bindings (C++17)
std::map<std::string, int> m = {{"a", 1}, {"b", 2}};
for (const auto& [key, value] : m) {
    // Use key and value directly
}
```

## Algorithms

```cpp
#include <algorithm>
#include <numeric>

std::vector<int> v = {5, 2, 8, 1, 9};

// Sort
std::sort(v.begin(), v.end());
std::sort(v.begin(), v.end(), std::greater<>{});  // Descending

// Find
auto it = std::find(v.begin(), v.end(), 8);
auto it2 = std::find_if(v.begin(), v.end(), [](int x) { return x > 5; });

// Transform
std::transform(v.begin(), v.end(), v.begin(), [](int x) { return x * 2; });

// Accumulate
int sum = std::accumulate(v.begin(), v.end(), 0);

// Remove-erase idiom
v.erase(std::remove_if(v.begin(), v.end(), [](int x) { return x < 5; }), v.end());

// C++20 ranges (if available)
// std::ranges::sort(v);
// auto filtered = v | std::views::filter([](int x) { return x > 5; });
```

## Error Handling

### Exceptions vs Error Codes

```cpp
// Exceptions: for exceptional conditions
void process_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open: " + path);
    }
    // Process...
}

// std::optional: for expected "no value"
std::optional<User> find_user(int id) {
    auto it = users.find(id);
    if (it != users.end()) {
        return it->second;
    }
    return std::nullopt;
}

// std::expected (C++23) or result types: for expected errors
// enum class Error { NotFound, InvalidFormat };
// std::expected<User, Error> find_user(int id);
```

### Exception Safety

```cpp
void safe_operation() {
    auto resource = std::make_unique<Resource>();  // Acquired

    do_something();  // May throw

    // resource automatically released if exception thrown
}
```

## Templates

### Basic Templates

```cpp
template<typename T>
T max_value(T a, T b) {
    return (a > b) ? a : b;
}

template<typename Container>
void print_all(const Container& c) {
    for (const auto& item : c) {
        std::cout << item << ' ';
    }
}
```

### Concepts (C++20)

```cpp
template<typename T>
concept Numeric = std::is_arithmetic_v<T>;

template<Numeric T>
T add(T a, T b) {
    return a + b;
}
```

## String Handling

```cpp
#include <string>
#include <string_view>

// string_view for read-only, non-owning access
void process(std::string_view sv) {
    // Cheap to copy, no allocation
}

// String building
std::string result;
result.reserve(100);
result += "Hello";
result.append(" World");

// String formatting (C++20)
// auto formatted = std::format("Value: {}", 42);
```

## Filesystem (C++17)

```cpp
#include <filesystem>
namespace fs = std::filesystem;

fs::path p = "/home/user/file.txt";
if (fs::exists(p)) {
    auto size = fs::file_size(p);
    auto parent = p.parent_path();
    auto ext = p.extension();
}

// Iterate directory
for (const auto& entry : fs::directory_iterator("/path")) {
    if (entry.is_regular_file()) {
        std::cout << entry.path() << '\n';
    }
}
```

## Threading

```cpp
#include <thread>
#include <mutex>
#include <atomic>

std::mutex mtx;
std::atomic<int> counter{0};

void worker() {
    std::lock_guard<std::mutex> lock(mtx);  // RAII lock
    // Critical section
}

// Prefer std::jthread (C++20) - auto-joins
// std::jthread t(worker);

std::thread t(worker);
t.join();  // Or t.detach()
```

## Best Practices Summary

1. **Never use raw new/delete** - Use smart pointers
2. **Prefer value semantics** - Copy/move over pointers when practical
3. **Use const liberally** - const methods, const references
4. **Initialize at declaration** - Avoid uninitialized variables
5. **Prefer algorithms** - Over raw loops
6. **Use auto judiciously** - For complex types, iterators
7. **Mark functions noexcept** - When they don't throw
8. **Use [[nodiscard]]** - For return values that shouldn't be ignored

## CMake Configuration

See [references/cmake-modern.md](references/cmake-modern.md) for detailed CMake setup.

Basic structure:

```cmake
cmake_minimum_required(VERSION 3.15...4.0)
project(MyProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(MyApp src/main.cpp)
target_include_directories(MyApp PRIVATE include)
```

## References

- [references/cmake-modern.md](references/cmake-modern.md) - Modern CMake patterns
- [references/patterns.md](references/patterns.md) - Common design patterns in C++
