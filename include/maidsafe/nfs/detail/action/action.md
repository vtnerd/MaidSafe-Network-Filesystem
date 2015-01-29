# Action Library #
A simple callback library designed for use with ASIO stackless coroutines. `boost::phoenix` would've been the first choice, but it does not have move support.

## Problem ##
ASIO stackless coroutines require the same signature for the re-entrancy function. This is an issue if a stackless coroutine makes two or more async operation calls that return different values. The function would need a giant variant type supporting every return type, and then would have to react to it.

## Solution ##
This mini-library provides a couple of simple functors that assist in ASIO stackless usage. A few of the actions (which are marked) require a `nfs::detail::Coroutine` object.

## OperationHandler ##
Operation handler provides function operators that are designed to be used with typical ASIO async operations. Operation handler must be given a success and failure routine, only one of which is called when the async operation completes. The success routine must accept a parameter for the value returned by the routine, and the failure routine must accept a std::error_code.

### API ###
```c++
template<typename SuccessRoutine, typename FailureRoutine>
struct OperationHandler {
  using ExpectedValue = typename SuccessRoutine::ExpectedValue;

  void operator()();
  void operator()(const std::error_code error);
  void operator()(Expected<ExpectedValue> expected_value);

  template<typename Value>
  void operator()(Value&& value, const std::error_code error);

  template<typename Value>
  void operator()(Expected<Value> expect_value);
};
```

### Example Usage ###
```c++
TEST(OperationHander, Basic) {
  bool success = false;
  std::error_code failure{};

  auto callback = operation
    .OnSuccess(action::Store(std::ref(success)))
    .OnFailure(action::Store(std::ref(failure)));

  EXPECT_FALSE(success);
  EXPECT_FALSE(static_cast<bool>(failure));

  callback(true);

  EXPECT_TRUE(success);
  EXPECT_FALSE(static_cast<bool>(failure));
}
```
This creates a callback (variable `callback`) for an async operation that returns a bool on success, and a std::error_code on failure. The [store action](#store) simply forwards the value given to it, to the underlying type (which is a `std::reference_wrapper<T>` to the real variable).

## Actions ##
### Abort ###
Typically used as the fail function for an operation. Passes the error to the handler as an unexpected value.

* **Pre-Req**
 * Must be given a `nfs::detail::Coroutine` object.
 * The frame in use by the Coroutine must have a `handler` that can accept a boost::unexpected<T>
* **Action**
 * Calls `coro.frame().handler(boost::make_unexpected(VALUE)` where `VALUE` is the parameter given to the action callback.

#### API ####
```c++
template<...>
struct ActionAbort {
  using ExpectedValue = std::error_code;
  void operator()(const std::error_code);
};
```

#### Example Usage ####
```c++
struct Handler {
  void operator()(boost::expected<int, std::error_code> result)) {
    if (result) {
      value = *result;
    } else {
      error = result.error();
    }
  }
  std::reference_wrapper<int> value;
  std::reference_wrapper<std::error_code> error;
};

struct Routine {
  struct Frame {
    Handler handler;
  };
  void operator()(Coroutine<Routine, Frame>&) const {}
};

TEST(Action, Abort) {
  int value{};
  std::error_code error{};

  std::function<void(std::error_code)> fail_function =
    action::Abort(MakeCoroutine<Routine>(Handler{std::ref(value), std::ref(error)}));

  std::error_code op_error = std::make_error_code(std::errc::operation_canceled);

  EXPECT_EQ(0, value);
  EXPECT_NE(op_error, error);

  fail_function(op_error)

  EXPECT_EQ(0, value);
  EXPECT_EQ(op_error, error);
}
```

### CallOnce ###
Lazy callback for std::call_once.

* **Pre-Req**
 * Must be given a `nfs::detail::Coroutine` object.
* **Action**
 * Calls the `Execute` method on the coroutine.

#### API ####
```c++
template<typename Once, typename Callback>
struct ActionCallOnce {
  using ExpectedValue = typename Callback::ExpectedValue;

  template<typename... Args>
  void operator()(Args&&... args);
};
```

#### Example Usage ####
```c++
TEST(Action, CallOnce) {
  std::once_flag once{};
  int value{};

  std::function<void(int)> call_once_function =
    action::CallOnce(std::ref(once), action::Store(std::ref(value)));

  EXPECT_EQ(0, value);
  call_once_function(100);
  EXPECT_EQ(100, value);

  call_once_function(50);  // already invoked
  EXPECT_EQ(100, value);
}
```

### Continuation ###
A few callbacks support continuation, a .Then() function accepts a second callback that is invoked after the first.

* **Action**
 * Invokes first callback, then the second callback.

#### API ####
```c++
template<typename typename First, typename Second>
struct ActionContinuation {
  using ExpectedValue = typename First::ExpectedValue;

  template<typename Arg>
  void operator()(Arg&&... arg);
};
```

#### Example Usage ####
```c++
struct Routine {
  struct Frame {
    std::reference_wrapper<int> value;
  };
  void operator()(Coroutine<Routine, Frame>& coro) const {
    ASIO_CORO_REENTER(coro) {
      ++(coro.frame().value.get())
    }
  }
};

TEST(Action, Continuation) {
  int value1{};
  int value2{};

  std::function<void(int)> continued_function =
    action::Store(std::ref(value1)).Then(MakeCoroutine<Routine>(std::ref(value2)));

  EXPECT_EQ(0, value1);
  EXPECT_EQ(0, value2);

  continued_function(100);
  EXPECT_EQ(100, value1);
  EXPECT_EQ(1, value2);

  continued_function(150);
  EXPECT_EQ(150, value1);
  EXPECT_EQ(2, value2);
}
```

### Resume ###
Continues execution of a `nfs::detail::Coroutine` object.

> Supports [continuation](#continuation).

* **Pre-Req**
 * Must be given a `nfs::detail::Coroutine` object.
* **Action**
 * Calls `coro.Execute()`

#### API ####
```c++
template<typename Once, typename Callback>
struct ActionResume {
  using ExpectedValue = void;
  void operator()();
};
```

#### Example Usage ####
```c++
struct Routine {
  struct Frame {
    std::reference_wrapper<int> value;
  };
  void operator()(Coroutine<Routine, Frame>& coro) const {
    ASIO_CORO_REENTER(coro) {
      ++(coro.frame().value.get())
    }
  }
};

TEST(Action, Resume) {
  int value{};

  std::function<void()> resume_function =
    action::Resume(MakeCoroutine<Routine>(std::ref(value)));

  EXPECT_EQ(0, value);
  resume_function();
  EXPECT_EQ(1, value);

  resume_function();  // coro is complete, so it immediately aborts
  EXPECT_EQ(1, value);
}
```

### Store ###
Callback that forwards arguments to a the assignment operator of a provided value.

> Supports [continuation](#continuation).

* **Pre-Req**
 * A reference to the value is stored, so the reference must remain valid for lifetime of callback.
* **Action**
 * Forwards functor arguments to the provided value.

#### API ####
```c++
template<typename Value>
struct ActionStore {
  using ExpectedValue = Value;

  template<typename Arg>
  void operator()(Arg&&... arg);
};
```

#### Example Usage ####
```c++
TEST(Action, Store) {
  int value{};

  std::function<void(int)> call_once_function = action::Store(std::ref(value));

  EXPECT_EQ(0, value);
  call_once_function(100);
  EXPECT_EQ(100, value);

  call_once_function(50);
  EXPECT_EQ(50, value);
}
```

## Real Usage ##
This is the routine for retrieving the version history of a container.
```c++
template<typename Handler>
struct GetVersionsRoutine {
  struct Frame {
    std::shared_ptr<Container> container;
    std::vector<ContainerVersion> result;
    Handler handler;
  };

  void operator()(Coroutine<GetVersionsRoutine<Handler>, Frame>& coro) const {
    assert(coro.frame().container != nullptr);

    ASIO_CORO_REENTER(coro) {
      ASIO_CORO_YIELD
        Network::GetSDVVersions(
            coro.frame().container->network_.lock(),
            coro.frame().container->container_key_.GetId(),
            operation
              .OnSuccess(action::Store(std::ref(coro.frame().result)).Then(action::Resume(coro)))
              .OnFailure(action::Abort(coro)));

      coro.frame().container->UpdateCachedVersions(coro.frame().result);
      coro.frame().handler(std::move(coro.frame().result));
    }
  }
};
```
