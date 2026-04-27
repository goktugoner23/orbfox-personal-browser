# Mocking and Test Doubles Guide

## Types of Test Doubles

### 1. Dummy
Objects passed around but never used. Typically fill parameter lists.

```cpp
// Dummy - just satisfies the interface
class DummyLogger : public ILogger {
public:
    void Log(const std::string&) override {}
    void Error(const std::string&) override {}
};

TEST(ServiceTest, DoesNotNeedLogger) {
    DummyLogger dummy;
    Service service(&dummy);  // Logger not used in this test
    EXPECT_TRUE(service.DoSomething());
}
```

### 2. Stub
Provides canned answers to calls made during the test.

```cpp
class StubUserRepository : public IUserRepository {
public:
    User GetUser(int id) override {
        return User{id, "Stub User", "stub@example.com"};
    }
};

TEST(UserServiceTest, GetUserReturnsUser) {
    StubUserRepository stub;
    UserService service(&stub);

    auto user = service.GetUser(1);
    EXPECT_EQ(user.name, "Stub User");
}
```

### 3. Spy
Stubs that also record information about how they were called.

```cpp
class SpyEmailService : public IEmailService {
public:
    std::vector<std::pair<std::string, std::string>> sent_emails;

    void Send(const std::string& to, const std::string& body) override {
        sent_emails.push_back({to, body});
    }
};

TEST(NotificationTest, SendsEmailOnEvent) {
    SpyEmailService spy;
    NotificationService service(&spy);

    service.NotifyUser("user@example.com", "Hello");

    ASSERT_EQ(spy.sent_emails.size(), 1);
    EXPECT_EQ(spy.sent_emails[0].first, "user@example.com");
    EXPECT_EQ(spy.sent_emails[0].second, "Hello");
}
```

### 4. Mock
Pre-programmed with expectations about calls they will receive.

```cpp
class MockPaymentGateway : public IPaymentGateway {
public:
    MOCK_METHOD(bool, Charge, (double amount, const std::string& card), (override));
    MOCK_METHOD(bool, Refund, (const std::string& transaction_id), (override));
};

TEST(PaymentServiceTest, ChargesCorrectAmount) {
    MockPaymentGateway mock;
    EXPECT_CALL(mock, Charge(99.99, "4111111111111111"))
        .WillOnce(Return(true));

    PaymentService service(&mock);
    bool result = service.ProcessPayment(99.99, "4111111111111111");

    EXPECT_TRUE(result);
}
```

### 5. Fake
Working implementations with shortcuts (e.g., in-memory database).

```cpp
class FakeDatabase : public IDatabase {
private:
    std::unordered_map<int, User> users_;

public:
    void Save(const User& user) override {
        users_[user.id] = user;
    }

    std::optional<User> Get(int id) override {
        auto it = users_.find(id);
        if (it != users_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    void Clear() {
        users_.clear();
    }
};

TEST(UserServiceTest, SaveAndRetrieve) {
    FakeDatabase fake;
    UserService service(&fake);

    service.CreateUser(User{1, "John"});
    auto user = service.GetUser(1);

    EXPECT_EQ(user->name, "John");
}
```

## When to Use Each Type

| Double Type | Use When |
|-------------|----------|
| Dummy | You need to satisfy a parameter but don't care about it |
| Stub | You need controlled indirect inputs |
| Spy | You need to verify indirect outputs were correct |
| Mock | You need to verify interactions happened correctly |
| Fake | You need a working implementation but simpler/faster |

## Best Practices

### 1. Don't Over-Mock

**Bad: Mocking too much**
```cpp
// This tests almost nothing!
TEST(ServiceTest, OverMocked) {
    MockDep1 mock1;
    MockDep2 mock2;
    MockDep3 mock3;
    MockDep4 mock4;

    EXPECT_CALL(mock1, Method()).WillOnce(Return(result1));
    EXPECT_CALL(mock2, Method()).WillOnce(Return(result2));
    EXPECT_CALL(mock3, Method()).WillOnce(Return(result3));
    EXPECT_CALL(mock4, Method()).WillOnce(Return(result4));

    Service service(&mock1, &mock2, &mock3, &mock4);
    auto result = service.DoWork();

    // What are we even testing here?
    EXPECT_EQ(result, expected);
}
```

**Good: Mock only external boundaries**
```cpp
TEST(ServiceTest, ProcessesDataCorrectly) {
    // Only mock the external API, use real internal logic
    MockExternalAPI mockApi;
    EXPECT_CALL(mockApi, Fetch("data"))
        .WillOnce(Return(rawData));

    Service service(&mockApi);
    auto result = service.ProcessData("data");

    // Now we're testing the actual processing logic
    EXPECT_EQ(result.processedValue, expectedValue);
}
```

### 2. Prefer State Testing Over Interaction Testing

**Interaction testing (fragile)**
```cpp
TEST(OrderServiceTest, CreatesOrder_Fragile) {
    MockInventory mockInventory;
    MockPayment mockPayment;
    MockNotification mockNotification;

    // These expectations make the test fragile
    EXPECT_CALL(mockInventory, Reserve(_, _));
    EXPECT_CALL(mockPayment, Charge(_, _));
    EXPECT_CALL(mockNotification, Send(_));

    OrderService service(&mockInventory, &mockPayment, &mockNotification);
    service.CreateOrder(order);
    // If implementation changes order of calls, test breaks
}
```

**State testing (robust)**
```cpp
TEST(OrderServiceTest, CreatesOrder_Robust) {
    FakeInventory fakeInventory;
    fakeInventory.AddStock("item1", 10);

    FakePayment fakePayment;
    SpyNotification spyNotification;

    OrderService service(&fakeInventory, &fakePayment, &spyNotification);
    auto result = service.CreateOrder(order);

    // Test the outcomes, not the interactions
    EXPECT_TRUE(result.success);
    EXPECT_EQ(fakeInventory.GetStock("item1"), 9);
    EXPECT_TRUE(spyNotification.WasNotified());
}
```

### 3. Make Dependencies Injectable

**Bad: Hard-coded dependency**
```cpp
class UserService {
    Database db_;  // Can't substitute for testing
public:
    User GetUser(int id) {
        return db_.Query("SELECT * FROM users WHERE id = ?", id);
    }
};
```

**Good: Injected dependency**
```cpp
class UserService {
    IDatabase& db_;
public:
    explicit UserService(IDatabase& db) : db_(db) {}

    User GetUser(int id) {
        return db_.Query("SELECT * FROM users WHERE id = ?", id);
    }
};
```

### 4. Use Descriptive Mock Expectations

```cpp
// Bad: Unclear what's expected
EXPECT_CALL(mock, Process(_)).WillOnce(Return(true));

// Good: Clear documentation of expected behavior
EXPECT_CALL(mock, Process(HasSubstr("valid")))
    .Times(1)
    .WillOnce(Return(true))
    .RetiresOnSaturation();
```

### 5. Reset Mocks Between Tests

```cpp
class MyTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        mock_ = std::make_unique<MockService>();
    }

    void TearDown() override {
        // Ensures expectations are verified
        mock_.reset();
    }

    std::unique_ptr<MockService> mock_;
};
```

## Common Mocking Patterns

### Returning Different Values on Consecutive Calls

```cpp
EXPECT_CALL(mock, GetValue())
    .WillOnce(Return(1))
    .WillOnce(Return(2))
    .WillOnce(Return(3));

// Or for indefinite sequence
EXPECT_CALL(mock, GetValue())
    .WillOnce(Return(1))
    .WillRepeatedly(Return(2));
```

### Saving Arguments

```cpp
std::string savedArg;
EXPECT_CALL(mock, Process(_))
    .WillOnce(DoAll(
        SaveArg<0>(&savedArg),
        Return(true)
    ));

service.DoWork("test input");
EXPECT_EQ(savedArg, "test input");
```

### Delegating to Real Implementation

```cpp
class MockService : public Service {
public:
    MOCK_METHOD(int, Process, (int x), (override));

    // Delegate to parent
    int RealProcess(int x) { return Service::Process(x); }
};

TEST(ServiceTest, DelegatesToReal) {
    MockService mock;
    ON_CALL(mock, Process(_))
        .WillByDefault(Invoke(&mock, &MockService::RealProcess));

    // Only override specific cases
    EXPECT_CALL(mock, Process(42)).WillOnce(Return(0));
}
```

### Throwing Exceptions

```cpp
EXPECT_CALL(mock, Process(_))
    .WillOnce(Throw(std::runtime_error("Network error")));

EXPECT_THROW(service.DoWork(), std::runtime_error);
```

### Conditional Returns

```cpp
EXPECT_CALL(mock, IsValid(_))
    .WillRepeatedly(Invoke([](int id) {
        return id > 0;
    }));
```

## Testing with Time

### C++ Approach

```cpp
class IClock {
public:
    virtual ~IClock() = default;
    virtual std::chrono::system_clock::time_point Now() const = 0;
};

class FakeClock : public IClock {
public:
    std::chrono::system_clock::time_point now_;

    std::chrono::system_clock::time_point Now() const override {
        return now_;
    }

    void Advance(std::chrono::seconds seconds) {
        now_ += seconds;
    }
};

TEST(CacheTest, ExpiresAfterTimeout) {
    FakeClock clock;
    clock.now_ = std::chrono::system_clock::now();

    Cache cache(&clock, std::chrono::seconds(60));
    cache.Set("key", "value");

    EXPECT_EQ(cache.Get("key"), "value");

    clock.Advance(std::chrono::seconds(61));

    EXPECT_FALSE(cache.Get("key").has_value());
}
```

## Testing Randomness

```cpp
class IRandomGenerator {
public:
    virtual ~IRandomGenerator() = default;
    virtual int Next(int min, int max) = 0;
};

class FakeRandom : public IRandomGenerator {
public:
    std::vector<int> values_;
    size_t index_ = 0;

    int Next(int, int) override {
        return values_[index_++];
    }
};

TEST(GameTest, DiceRollsAreUsed) {
    FakeRandom fakeRandom;
    fakeRandom.values_ = {6, 1, 4};

    Game game(&fakeRandom);

    EXPECT_EQ(game.RollDice(), 6);
    EXPECT_EQ(game.RollDice(), 1);
    EXPECT_EQ(game.RollDice(), 4);
}
```
