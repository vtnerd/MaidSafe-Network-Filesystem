# Maidsafe App REST API [DRAFT] #
> NOTE: This API is subject to change.

The Maidsafe REST API strives to be easy to use, consistent, and flexible for advanced users requiring performance.

## Storage Abstractions ##
### Blobs ###
Data on the SAFE network is stored in Blobs. A Blob can contain text or binary data, and the SAFE network has no upward limit on size. However, local system contraints may apply to maximum size. Each Blob is immutable, once it is stored it cannot be modified.

### Container ###
A Container stores Blobs at keys that have no restrictions (any sequence of bytes are valid). Each key is versioned, so past Blobs can be retrieved (which gives the appearance of mutability since a new `Blob`s can be stored at an existing key).

## Behavior Overview ##
Every REST API function call that requires a network operation returns a [`Future<T>`](#maidsafenfsfuture) object. This prevents the interface from blocking, and provides an interface for signalling completion. Every `Future<T>` object in the REST API returns an [expected](#expected) object, which either holds the result of the operation or a network related error. An expected object allows for exception-style programming, return-code style programming, or monadic style programming. When an expected object contains a successful operation, it will have the result of the operation. When an expected object contains a failed operation, it will contain an [`ObjectError<T>`](#maidsafenfsoperationerror) object.

## Examples ##
### Hello World (Exception Style) ###
```c++
bool HelloWorld(maidsafe::nfs::RestContainer& container) {
  try {
    const auto blob = container.CreateBlob(
        "example_blob", "hello world", std::string()).get().value();
    const auto blob_contents = container.GetBlobContents(blob).get().value();

    std::cout << blob_contents << std::endl;
  }
  catch (const std::runtime_error& error) {
    std::cerr << "Error : " << error.what() << std::endl;
    return false;
  }
  catch (...) {
    std::cerr << "Uknown Error" << std::endl;
    return false;
  }

  return true;
}
```
The `.get()` calls after `CreateBlob`, and `GetBlobContents` indicate that the process should wait until the SAFE network successfully completes the requested operation (the `.get()` is called on the `Future<T>` object). The `Future<T>` object allows a process to make additional requests before prior requests have completed (see [Hello World Concatenation](#hello-world-concatenation)).

The `Future<T>` returns a `boost::expected` object. In this example, exception style error-handling was used, so `.value()` was invoked on the `boost::expected`. The `.value()` function checks the error status, and throws `std::system_error` if the `boost::expected` object has an error instead of a valid operation.

### Hello World Retry (Return-Code Style) ###
```c++
namespace {
  template<typename Result>
  boost::optional<Result> GetOperationResult(maidsafe::nfs::ExpectedOperation<Result> operation) {
    while (!operation) {
      if (operation.error().code() != std::errc::network_down) {
        std::cerr <<
            "Error: " << operation.error().code().message() << std::endl;
        return boost::none;
      }
      operation = operation.error().Retry().get();
    }
    return *operation;
  }
}

bool HelloWorld(maidsafe::nfs::Container& storage) {
  const boost::optional<Blob> create_operation(
      GetOperationResult(
          storage.CreateBlob("example_blob", "hello world", std::string()).get()));
  if (create_operation) {
    const boost::optional<std::string> get_operation(
        GetOperationResult(storage.GetBlobContents(*create_operation).get()));
    if (get_operation) {
      std::cout << *get_operation<< std::endl;
      return true;
    }
  }
  return false;
}
```
This example is identical to the [hello world](#hello-world) example, except `CreateBlob` and `GetBlobContents` operations that failed due to the network being down (no connection) are retried. In production code you may want to limit the attempts, or have a signal that indicates the return of network connectivity.

> If the retry mechanism returns `NfsErrors::no_retry` then no retry is possible. It is important that clients check the error code after a retry, or clients could continually attempt an operation that will never succeed.

### Hello World (Monad Style) ###
```c++
bool HelloWorld(const maidsafe::nfs::Container& container) {
  namespace nfs = maidsafe::nfs;

  return nfs::monadic(container.CreateBlob("example_blob", "hello world", std::string()).get()
      ).bind([&](Blob blob) {
        return nfs::monadic(container.GetBlobContents(blob).get());
      }).bind([](std::string contents) {
        std::cout << contents << std::endl;
      }).catch_error([](std::error_code error) { 
        std::cerr << error.message() << std::endl;
        return boost::make_unexpected(error)
      }).valid();
}
```
This is an example of monadic programming, which is better described in the [Expected](#expected) documentation. The callbacks provided to the `bind` function calls are only invoked if the operation was successful, and the `catch_error` callback is only invoked if *any* of the previous operations failed. This eliminates the need for client code to check for errors after each operation. Also, in this example all values are *moved*, not copied, so it is efficient as well.

> Using monadic programming with boost expected will require the usage of [`maidsafe::nfs::monadic`](#monadic).

### Hello World Concatenation ###
```c++
bool HelloWorld(maidsafe::nfs::Container& container) {
  auto create_part1 = container.CreateBlob("split_example/part1", "hello ", std::string());
  auto create_part2 = container.CreateBlob("split_example/part2", "world", std::string());

  const auto blob1 = create_part1.get();
  const auto blob2 = create_part2.get();

  if (blob1 && blob2) {
    auto get_part1 = container.GetBlobContents(*blob1);
    auto get_part2 = container.GetBlobContents(*blob2);

    const auto contents1 = get_part1.get();
    const auto contents2 = get_part2.get();

    if (contents1 && contents2) {
      std::cout << *contents1 << *contents2 << std::endl;
      return true;
    }
  }

  return false;
}
```
In this example, both `CreateBlob` calls are done in parallel, and both `GetBlobContent` calls are done in parallel. These blobs are **not** stored in a child `Container` called "split_example", but are stored in the `container` object directly.

This examples uses the `*` operator on the `boost::expected` object instead of `.value()` like in the [exception example](#hello-world-exception-style). The `*` operator does not check if the `boost::expected` has an error (similar to `*` being unchecked for `boost::optional`); the conversion to bool in the if statement is the check for validity.

## REST Style API ##
All public functions listed in this API provide the strong exception guarantee. All public const methods are thread-safe.

### maidsafe::nfs::Blob ###
> maidsafe/nfs/blob.h

- [x] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

Represents a single stored Blob on the network.

> The network currently has no time server of its own, so the timestamps are from the clients. If a client has a misconfigured clock, the timestamps stored will also be incorrect.

```c++
class Blob {
    const std::string& key() const noexcept;
    Clock::time_point creation_time() const;
    Clock::time_point modification_time() const;
    std::uint64_t size() const noexcept;
    const std::string& user_meta_data() const noexcept;
    
    bool Equal(const Blob& other) const noexcept
};

bool operator==(const Blob&, const Blob&) const noexcept;
bool operator!=(const Blob&, const Blob&) const noexcept;
```
- **key()**
  - Returns the key associated with this `Blob`.
- **creation_time()**
  - Returns the timestamp of when `key()` last went from storing nothing to storing a `Blob`.
- **modification_time()**
  - Returns the timestamp of when this `Blob` instance was stored.
- **size()**
  - Returns the size of this `Blob` in bytes.
- **user_meta_data()**
  - Returns the user metadata being stored for this `Blob`.
- **Equal(const Blob& other)**
  - Returns true if `other` is equivalent to `this` Blob.
- The non-member operator overloads call the corresponding Equal function.

### maidsafe::nfs::OperationError&lt;T> ###
> maidsafe/nfs/operation_error.h

- [ ] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

In the event of a failure, retrieving the cause of the error and a Retry attempt can be done with the `OperationError` interface. The error is a std::error_code object, and the retry attempt will return a new `Future` object with the exact type of the previous failed attempt.

```c++
template<typename T>
class OperationError {
  using RetryResult = Future<boost::expected<T, OperationError<T>>;
  RetryResult Retry() const;
  std::error_code code() const;
};
```
- **Retry**
  - Return a Future to another attempt at the failed operation. Be careful of infinite loops - some operations could fail indefinitely. If the retry returns an error code `NfsErrors::no_retry`, then a retry is not possible.
- **code()**
  - Return error code for the failed operation.

### maidsafe::nfs::Future&lt;T> ###
> maidafe/nfs/future.h

- [x] Thread-safe Public Functions
- [ ] Copyable
- [x] Movable

Currently `maidsafe::nfs::Future` is a `boost::future` object, but this may be changed to a non-allocating design. It is recommended that you use the typedef (`maidsafe::nfs::Future`) in case the implementation changes.

In the REST API, the `Future` will only throw exceptions on non-network related errors (std::bad_alloc, std::bad_promise, etc.). Values and network related errors are returned in a `boost::expected` object.

```c++
template<typename T>
using Future = boost::future<T>;
```

### maidsafe::nfs::Expected&lt;T> ###
> maidsafe/nfs/expected.h

When a network operation has completed, the future will return a [`boost::expected`](https://github.com/ptal/std-expected-proposal) object. On network errors, the `boost::expected` object will contain an `OperationError` object, and on success the object will contain an object of `T` as indicated by the interface.

```c++
template<typename T>
using Expected = boost::expected<T, std::error_code>;
```

#### maidsafe::nfs::ExpectedOperation&lt;T> ####
> maidsafe/nfs/expected_operation.h

- [ ] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

```c++
template<typename T>
using ExpectedOperation = boost::expected<T, OperationError<T>>;
```

#### maidsafe::nfs::monadic ####
> maidsafe/nfs/expected_operation.h

The REST API returns `ExpectedOperation<T>` objects which use an error type that depends on `T`. This makes monadic programming difficult because the unwrap functions in boost::expected will not work as desired. The REST API includes some standalone functions that return a `boost::expected `object with a consistent error type, `std::error_code`. After removing the `OperationError<T>`, retrying the failed operation is not possible.

```c++
template<typename T>
Expected<T> monadic(const ExpectedOperation<T>& expected);

template<typename T>
Expected<T> monadic(ExpectedOperation<T>&& expected);
```

### maidsafe::nfs::RestContainer ###
> maidsafe/nfs/rest_container.h

- [x] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

> This object has a single `shared_ptr`, and is shallow-copied. This makes it extremely quick to copy.

Represents the [`Container`](#container) abstraction listed above.
```c++
class RestContainer {
  Future<ExpectedOperation<std::vector<Blob>>> ListBlobs(std::string prefix = std::string());

  Future<ExpectedOperation<Blob>>              GetBlob(const std::string& key);
  Future<ExpectedOperation<std::vector<Blob>>> GetBlobHistory(const std::string& key);
  
  Future<ExpectedOperation<std::string>> GetBlobContent(const Blob& blob);
  Future<ExpectedOperation<std::string>> GetBlobContent(
      const Blob& blob, std::uint64_t offset, std::uint64_t length);

  Future<ExpectedOperation<Blob>> CreateBlob(
      const std::string& key, std::string data, std::string meta_data);

  Future<ExpectedOperation<Blob>> UpdateBlobContent(const Blob& blob, std::string);
  Future<ExpectedOperation<Blob>> UpdateBlobContent(
      const Blob& blob, std::string, std::uint64_t offset);
  Future<ExpectedOperation<Blob>> UpdateBlobMetadata(const Blob& blob, std::string);
  Future<ExpectedOperation<void>> DeleteBlob(const Blob& blob);

  Future<Blob> Copy(const Blob& from, const std::string& to);
};
```
- **ListBlobs(std::string prefix = std:string())**
  - Retrieves the most recent Blobs stored in the Container.
  - `prefix` will filter the returned values - only Blobs with key matching the prefix will be returned. The empty string indicates that all Blobs should be returned.
- **GetBlob(const std::string& key)**
  - Retrieve a handle to the most recent Blob referenced by `key`.
- **GetBlobHistory(const std::string& key)**
  - Retrieve every Blob referenced by key, stopping at the creation of the first Blob or the end of the finite history stored by the network. The front() of the std::vector will contain the newest Blob, while the back() of the vector will contain the oldest known Blob.
- **GetBlobContent(const Blob& blob)**
  - Retrieve the content of `blob`.
  - `blob` can be from _any_ Container.
  - Can be done at anytime, even if `blob` has been deleted from the Container.
- **GetBlobContent(const Blob& blob, std::uint64_t offset, std::uint64_t length)**
  - Retrieve from `offset` to `offset + length` of the `blob` contents.
  - If `blob.size() < offset + length`, then the returned string is truncated to the end of the `blob` contents.
  - `blob` can be from _any_ Container.
  - Can be done at anytime, even if `blob` has been popped from the history limit or been deleted.
- **CreateBlob(const std::string& key, std::string data, std::string meta_data)**
  - Create a Blob at the specified `key`.
  - `data` or `meta_data` can be empty.
  - Maximum size of `meta_data` is 64KiB.
  - `data` has no size restrictions (only local memory contraints apply).
  - Fails if `key` curently references a Blob or nested Container (from Posix API).
  - Returns the Blob created.
- **UpdateBlobContent(const Blob& blob, std::string)**
  - Update the contents of `blob`.
  - Fails if `blob.key()` does not reference `blob`.
  - Returns the new Blob stored.
- **UpdateBlobContent(const Blob& blob, std::string, std::uint64_t offset)**
  - Update the contents of `blob` starting at `offset`.
  - If `blob.size() < offset`, then zeroes are written from `blob.size()` to `offset`.
  - Fails if `blob.key()` does not currently reference `blob`.
  - Returns the new blob stored.
- **UpdateBlobMetadata(const Blob& blob, std::string)**
  - Update the user meta data of `blob`.
  - Maximum size of `meta_data` is 64KiB.
  - Fails if `blob.key()` does not currently reference `blob`.
  - Returns the new Blob stored.
- **DeleteBlob(const Blob& blob)**
  - Remove `blob` from the lastest container listings.
  - Fails if `blob.key()` does not currently reference `blob`.
- **Copy(const Blob& from, const std::string& to)**
  - Copies the contents and user meta data of `from` to a new key referenced by `to`.
  - `from` can be a Blob stored in _any_ container.
  - `to` must be an unassociated key - it cannot currently reference a Blob or child Container (from the Posix API).
  - Returns the new Blob stored. The creation time and modification will not be the same as `from`.
