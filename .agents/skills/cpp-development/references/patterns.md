# Common C++ Design Patterns

## Singleton (Thread-Safe)

```cpp
class Logger {
public:
    static Logger& instance() {
        static Logger instance;  // Thread-safe in C++11+
        return instance;
    }

    void log(std::string_view message) { /* ... */ }

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
};

// Usage: Logger::instance().log("message");
```

## Factory

```cpp
class Shape {
public:
    virtual ~Shape() = default;
    virtual void draw() = 0;
};

class Circle : public Shape {
public:
    void draw() override { /* ... */ }
};

class Square : public Shape {
public:
    void draw() override { /* ... */ }
};

class ShapeFactory {
public:
    static std::unique_ptr<Shape> create(std::string_view type) {
        if (type == "circle") return std::make_unique<Circle>();
        if (type == "square") return std::make_unique<Square>();
        return nullptr;
    }
};
```

## Builder

```cpp
class HttpRequest {
public:
    class Builder {
        std::string url_;
        std::string method_ = "GET";
        std::map<std::string, std::string> headers_;

    public:
        Builder& url(std::string url) {
            url_ = std::move(url);
            return *this;
        }

        Builder& method(std::string method) {
            method_ = std::move(method);
            return *this;
        }

        Builder& header(std::string key, std::string value) {
            headers_[std::move(key)] = std::move(value);
            return *this;
        }

        HttpRequest build() {
            return HttpRequest(std::move(url_), std::move(method_), std::move(headers_));
        }
    };

private:
    HttpRequest(std::string url, std::string method,
                std::map<std::string, std::string> headers)
        : url_(std::move(url)), method_(std::move(method)), headers_(std::move(headers)) {}

    std::string url_;
    std::string method_;
    std::map<std::string, std::string> headers_;
};

// Usage:
// auto req = HttpRequest::Builder()
//     .url("https://api.example.com")
//     .method("POST")
//     .header("Content-Type", "application/json")
//     .build();
```

## Observer

```cpp
template<typename... Args>
class Signal {
    std::vector<std::function<void(Args...)>> slots_;

public:
    void connect(std::function<void(Args...)> slot) {
        slots_.push_back(std::move(slot));
    }

    void emit(Args... args) {
        for (auto& slot : slots_) {
            slot(args...);
        }
    }
};

// Usage:
// Signal<int, std::string> onEvent;
// onEvent.connect([](int id, const std::string& msg) { /* handle */ });
// onEvent.emit(42, "hello");
```

## PIMPL (Pointer to Implementation)

```cpp
// header.h
class Widget {
public:
    Widget();
    ~Widget();
    Widget(Widget&&) noexcept;
    Widget& operator=(Widget&&) noexcept;

    void doSomething();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// source.cpp
struct Widget::Impl {
    int internal_state = 0;
    void helper() { /* ... */ }
};

Widget::Widget() : impl_(std::make_unique<Impl>()) {}
Widget::~Widget() = default;
Widget::Widget(Widget&&) noexcept = default;
Widget& Widget::operator=(Widget&&) noexcept = default;

void Widget::doSomething() {
    impl_->helper();
}
```

## Strategy

```cpp
class SortStrategy {
public:
    virtual ~SortStrategy() = default;
    virtual void sort(std::vector<int>& data) = 0;
};

class QuickSort : public SortStrategy {
public:
    void sort(std::vector<int>& data) override {
        std::sort(data.begin(), data.end());
    }
};

class BubbleSort : public SortStrategy {
public:
    void sort(std::vector<int>& data) override {
        // Bubble sort implementation
    }
};

class Sorter {
    std::unique_ptr<SortStrategy> strategy_;

public:
    void set_strategy(std::unique_ptr<SortStrategy> s) {
        strategy_ = std::move(s);
    }

    void sort(std::vector<int>& data) {
        if (strategy_) strategy_->sort(data);
    }
};
```

## Command

```cpp
class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
};

class InsertTextCommand : public Command {
    std::string& document_;
    std::string text_;
    size_t position_;

public:
    InsertTextCommand(std::string& doc, std::string text, size_t pos)
        : document_(doc), text_(std::move(text)), position_(pos) {}

    void execute() override {
        document_.insert(position_, text_);
    }

    void undo() override {
        document_.erase(position_, text_.size());
    }
};

class CommandHistory {
    std::vector<std::unique_ptr<Command>> history_;

public:
    void execute(std::unique_ptr<Command> cmd) {
        cmd->execute();
        history_.push_back(std::move(cmd));
    }

    void undo() {
        if (!history_.empty()) {
            history_.back()->undo();
            history_.pop_back();
        }
    }
};
```

## Type Erasure

```cpp
class AnyCallable {
    struct Concept {
        virtual ~Concept() = default;
        virtual void invoke() = 0;
    };

    template<typename F>
    struct Model : Concept {
        F func_;
        Model(F f) : func_(std::move(f)) {}
        void invoke() override { func_(); }
    };

    std::unique_ptr<Concept> ptr_;

public:
    template<typename F>
    AnyCallable(F f) : ptr_(std::make_unique<Model<F>>(std::move(f))) {}

    void operator()() { ptr_->invoke(); }
};
```

## CRTP (Curiously Recurring Template Pattern)

```cpp
template<typename Derived>
class Comparable {
public:
    bool operator!=(const Derived& other) const {
        return !static_cast<const Derived*>(this)->operator==(other);
    }

    bool operator>(const Derived& other) const {
        return other < static_cast<const Derived&>(*this);
    }

    bool operator<=(const Derived& other) const {
        return !(static_cast<const Derived&>(*this) > other);
    }

    bool operator>=(const Derived& other) const {
        return !(static_cast<const Derived&>(*this) < other);
    }
};

class Integer : public Comparable<Integer> {
    int value_;
public:
    explicit Integer(int v) : value_(v) {}

    bool operator==(const Integer& other) const { return value_ == other.value_; }
    bool operator<(const Integer& other) const { return value_ < other.value_; }
};
// Integer now has ==, !=, <, >, <=, >= automatically
```
