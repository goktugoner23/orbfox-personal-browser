# Testing Patterns by Language

## C++ with GoogleTest

### Setup & Teardown

```cpp
class MyTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        // Called before each test
        resource_ = std::make_unique<Resource>();
    }

    void TearDown() override {
        // Called after each test
        resource_.reset();
    }

    // Shared across tests in this fixture
    std::unique_ptr<Resource> resource_;
};

TEST_F(MyTestFixture, TestName) {
    // resource_ is available here
    EXPECT_TRUE(resource_->IsValid());
}
```

### Parameterized Tests

```cpp
class AdditionTest : public ::testing::TestWithParam<std::tuple<int, int, int>> {};

TEST_P(AdditionTest, AddReturnsCorrectSum) {
    auto [a, b, expected] = GetParam();
    Calculator calc;
    EXPECT_EQ(calc.Add(a, b), expected);
}

INSTANTIATE_TEST_SUITE_P(
    AdditionCases,
    AdditionTest,
    ::testing::Values(
        std::make_tuple(1, 1, 2),
        std::make_tuple(0, 0, 0),
        std::make_tuple(-1, 1, 0),
        std::make_tuple(INT_MAX, 0, INT_MAX)
    )
);
```

### Death Tests

```cpp
TEST(DeathTest, AbortTerminatesProcess) {
    EXPECT_DEATH(std::abort(), "");
}

TEST(DeathTest, AssertionFailure) {
    EXPECT_DEBUG_DEATH(assert(false), "");
}
```

### Type-Parameterized Tests

```cpp
template <typename T>
class ContainerTest : public ::testing::Test {
protected:
    T container_;
};

using ContainerTypes = ::testing::Types<std::vector<int>, std::list<int>, std::deque<int>>;
TYPED_TEST_SUITE(ContainerTest, ContainerTypes);

TYPED_TEST(ContainerTest, EmptyOnConstruction) {
    EXPECT_TRUE(this->container_.empty());
}
```

## GoogleMock Patterns

### Basic Mock

```cpp
class MockService : public IService {
public:
    MOCK_METHOD(int, GetValue, (), (const, override));
    MOCK_METHOD(void, SetValue, (int value), (override));
    MOCK_METHOD(std::string, Process, (const std::string& input), (override));
};

TEST(ClientTest, UsesService) {
    MockService mock;
    EXPECT_CALL(mock, GetValue())
        .WillOnce(Return(42));

    Client client(&mock);
    EXPECT_EQ(client.DoWork(), 42);
}
```

### Matchers

```cpp
using ::testing::_;
using ::testing::Gt;
using ::testing::StartsWith;
using ::testing::HasSubstr;

EXPECT_CALL(mock, Process(StartsWith("prefix")))
    .WillOnce(Return("result"));

EXPECT_CALL(mock, SetValue(Gt(0)));  // Greater than 0
EXPECT_CALL(mock, Process(_));       // Any argument
```

### Actions

```cpp
using ::testing::Return;
using ::testing::Throw;
using ::testing::Invoke;
using ::testing::DoAll;
using ::testing::SaveArg;
using ::testing::SetArgReferee;

// Return value
EXPECT_CALL(mock, GetValue()).WillOnce(Return(42));

// Throw exception
EXPECT_CALL(mock, Process(_)).WillOnce(Throw(std::runtime_error("error")));

// Call real function
EXPECT_CALL(mock, Process(_)).WillOnce(Invoke([](const std::string& s) {
    return s + "_processed";
}));

// Multiple actions
EXPECT_CALL(mock, SetValue(_))
    .WillOnce(DoAll(
        SaveArg<0>(&saved_value),
        Return()
    ));
```

### Cardinality

```cpp
EXPECT_CALL(mock, GetValue())
    .Times(3)           // Exactly 3 times
    .WillRepeatedly(Return(42));

EXPECT_CALL(mock, GetValue())
    .Times(AtLeast(1)); // At least once

EXPECT_CALL(mock, GetValue())
    .Times(Between(2, 5)); // 2 to 5 times
```

## Python with pytest

### Basic Tests

```python
def test_addition():
    assert 1 + 1 == 2

def test_exception_raised():
    with pytest.raises(ValueError):
        int("not a number")

def test_exception_message():
    with pytest.raises(ValueError, match="invalid literal"):
        int("not a number")
```

### Fixtures

```python
@pytest.fixture
def sample_user():
    return User(id=1, name="John")

@pytest.fixture
def database(tmp_path):
    db = Database(tmp_path / "test.db")
    yield db
    db.close()

def test_user_creation(database, sample_user):
    database.save(sample_user)
    assert database.get(1) == sample_user
```

### Parameterized Tests

```python
@pytest.mark.parametrize("input,expected", [
    (1, 2),
    (2, 4),
    (0, 0),
    (-1, -2),
])
def test_double(input, expected):
    assert double(input) == expected

@pytest.mark.parametrize("a,b,expected", [
    pytest.param(1, 1, 2, id="positive"),
    pytest.param(-1, -1, -2, id="negative"),
    pytest.param(0, 0, 0, id="zero"),
])
def test_add(a, b, expected):
    assert add(a, b) == expected
```

### Mocking

```python
from unittest.mock import Mock, patch, MagicMock

def test_with_mock():
    mock_db = Mock()
    mock_db.get_user.return_value = User(id=1, name="John")

    service = UserService(mock_db)
    user = service.get_user(1)

    assert user.name == "John"
    mock_db.get_user.assert_called_once_with(1)

@patch('mymodule.external_api')
def test_with_patch(mock_api):
    mock_api.fetch.return_value = {"data": "value"}
    result = my_function()
    assert result == "value"
```

## JavaScript/TypeScript with Jest

### Basic Tests

```javascript
describe('Calculator', () => {
    it('adds two numbers', () => {
        expect(add(1, 2)).toBe(3);
    });

    it('throws on invalid input', () => {
        expect(() => add(null, 2)).toThrow('Invalid input');
    });
});
```

### Setup/Teardown

```javascript
describe('Database', () => {
    let db;

    beforeAll(async () => {
        db = await Database.connect();
    });

    afterAll(async () => {
        await db.close();
    });

    beforeEach(async () => {
        await db.clear();
    });

    it('stores data', async () => {
        await db.save({ id: 1, name: 'test' });
        expect(await db.get(1)).toEqual({ id: 1, name: 'test' });
    });
});
```

### Mocking

```javascript
// Mock module
jest.mock('./database');
import { Database } from './database';

Database.mockImplementation(() => ({
    get: jest.fn().mockResolvedValue({ id: 1 }),
    save: jest.fn().mockResolvedValue(true),
}));

// Mock function
const mockFn = jest.fn()
    .mockReturnValueOnce(1)
    .mockReturnValueOnce(2)
    .mockReturnValue(3);

// Spy
const spy = jest.spyOn(object, 'method');
expect(spy).toHaveBeenCalledWith(arg1, arg2);
```

### Async Tests

```javascript
it('fetches data', async () => {
    const data = await fetchData();
    expect(data).toBeDefined();
});

it('handles promises', () => {
    return fetchData().then(data => {
        expect(data).toBeDefined();
    });
});

it('handles callbacks', done => {
    fetchData(data => {
        expect(data).toBeDefined();
        done();
    });
});
```

## Go with testing Package

### Basic Tests

```go
func TestAdd(t *testing.T) {
    result := Add(1, 2)
    if result != 3 {
        t.Errorf("Add(1, 2) = %d; want 3", result)
    }
}

func TestAddTableDriven(t *testing.T) {
    tests := []struct {
        name     string
        a, b     int
        expected int
    }{
        {"positive", 1, 2, 3},
        {"negative", -1, -2, -3},
        {"zero", 0, 0, 0},
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            result := Add(tt.a, tt.b)
            if result != tt.expected {
                t.Errorf("Add(%d, %d) = %d; want %d",
                    tt.a, tt.b, result, tt.expected)
            }
        })
    }
}
```

### Subtests and Setup

```go
func TestDatabase(t *testing.T) {
    db := setupTestDB(t)
    defer db.Close()

    t.Run("Insert", func(t *testing.T) {
        err := db.Insert(testData)
        if err != nil {
            t.Fatal(err)
        }
    })

    t.Run("Query", func(t *testing.T) {
        result, err := db.Query(1)
        if err != nil {
            t.Fatal(err)
        }
        if result != expected {
            t.Errorf("got %v; want %v", result, expected)
        }
    })
}
```

## Rust with cargo test

### Basic Tests

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_add() {
        assert_eq!(add(1, 2), 3);
    }

    #[test]
    #[should_panic(expected = "divide by zero")]
    fn test_divide_by_zero() {
        divide(1, 0);
    }

    #[test]
    fn test_result() -> Result<(), String> {
        if add(1, 2) == 3 {
            Ok(())
        } else {
            Err("addition failed".into())
        }
    }
}
```

### Test Organization

```rust
// Unit tests in same file
#[cfg(test)]
mod tests {
    use super::*;
    // ...
}

// Integration tests in tests/ directory
// tests/integration_test.rs
use my_crate::public_function;

#[test]
fn test_public_api() {
    // ...
}
```
