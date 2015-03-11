# Maidsafe App REST API [DRAFT] #
> NOTE: This API is subject to change.

The Maidsafe REST API strives to be easy to use, consistent, and flexible for advanced users requiring performance.

## Storage Abstractions ##
### Blobs ###
Data on the SAFE network is stored in Blobs. A Blob can contain text or binary data, and the SAFE network has no upward limit on size. However, local system contraints may apply to maximum size. Each Blob is immutable, once it is stored it cannot be modified.

### Container ###
A Container stores Blobs at keys that have no restrictions (any sequence of bytes are valid). Each key is versioned, so past Blobs can be retrieved (which gives the appearance of mutability since a new `Blob`s can be stored at an existing key).

## Behavior Overview ##
Every REST API function call that requires a network operation returns a [`Future<T>`](#futuret) object. This prevents the interface from blocking, and provides an interface for signalling completion. Every `Future<T>` object in the REST API returns an [expected](#expected) object, which either holds the result of the operation or a network related error. An expected object allows for exception-style programming, return-code style programming, or monadic style programming. When an expected object contains a successful operation, it will have the result of the operation and version information. When an expected object contains a failed operation, it will contain an error code and a retry mechanism that returns a `Future<T>` with the same type as the original `Future<T>`.

## Examples ##
### Hello World (Exception Style) ###
```c++
bool HelloWorld(maidsafe::nfs::Storage& storage) {
  try {
    maidsafe::nfs::Container container(
        storage.OpenContainer("example_container").get().value().result());

    const auto put_operation = container.Put(
        "example_blob", "hello world", maidsafe::nfs::ModifyBlobVersion::Create()).get();
    const auto get_operation = container.Get(
        "example_blob", put_operation.value().version()).get();

    std::cout << get_operation.value().result() << std::endl;
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
The `.get()` calls after `GetContainer`, `Put` and `Get` indicate that the process should wait until the SAFE network successfully completes the requested operation (the `.get()` is called on the `Future<T>` object). The `Future<T>` object allows a process to make additional requests before prior requests have completed (see [Hello World Concatenation](#hello-world-concatenation)). If the above example issued the `Get` call without waiting for the `Put` `Future<T>` to signal completion, the `Get` could've failed. So the `Future<T>` will signal when the result of that operation can be seen by calls locally or remotely.

The `Future<T>` returns a `boost::expected` object. In this example, exception style error-handling was used, so `.value()` was invoked on the `boost::expected`. The `.value()` function checks the error status, and throws `std::system_error` if the `boost::expected` object has an error instead of a valid operation.

The `Put` call uses `ModifyBlobVersion::Create()` to indicate that it is creating and storing a new file. If an existing file exists at `example_blob`, then an exception will be thrown in the `Get` call because `put_operation` contains an error (so after running this program once, all subsequent runs should fail). The `Get` call uses the [`BlobVersion`](#blobversion) returned by the `Put`, guaranteeing that the contents from the original `Put` ("hello world"), are retrieved. Alternatively, `RetrieveBlobVersion::Latest()` could've been used instead, but if another process or thread updated the file, the new contents would be returned, which may not be "hello world" as desired.

### Hello World Retry (Return-Code Style) ###
```c++
namespace {
  template<typename Result>
  boost::optional<maidsafe::nfs::BlobOperation<Result>> GetOperationResult(
      maidsafe::nfs::ExpectedBlobOperation<Result> operation) {
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
  const boost::optional<maidsafe::nfs::BlobOperation<>> put_operation(
      GetOperationResult(
          storage.Put(
              "example_blob", "hello world", maidsafe::nfs::ModifyBlobVersion::Create()).get()));
  if (put_operation) {
    const boost::optional<maidsafe::nfs::BlobOperation<std::string>> get_operation(
        GetOperationResult(
            storage.Get(
                "example_blob", put_operation->version()).get()));
    if (get_operation) {
      std::cout << get_operation->result() << std::endl;
      return true;
    }
  }
  return false;
}
```
This example starts from the `Container` object for brevity. It is identical to the [hello world](#hello-world) example, except `Put` and `Get` operations that failed due to the network being down (no connection) are retried. In production code you may want to limit the attempts, or have a signal that indicates the return of network connectivity.

> If the retry mechanism returns std::errc::not_supported then no retry is possible. It is important that clients check the error code after a retry, or clients could continually attempt an operation that will never succeed.

### Hello World (Monad Style) ###
```c++
bool HelloWorld(const maidsafe::nfs::Storage& storage) {
  namespace nfs = maidsafe::nfs;

  return nfs::monadic(storage.OpenContainer("example_container").get()).bind(

      [](nfs::ContainerOperation<nfs::Container> open_operation) {
        return nfs::monadic(
            open_operation.result().Put(
                "example_blob", "hello world", nfs::ModifyBlobVersion::Create()).get()).bind(

                [&open_operation](nfs::BlobOperation<> put_operation) {
                  return nfs::monadic(
                      open_operation.result().Get("example_blob", put_operation.version()).get());

                }).bind([](nfs::BlobOperation<std::string> get_operation) {
                  std::cout << get_operation.result() << std::endl;
                });

      }).catch_error([](std::error_code error) {
        std::cerr << "Error: " << error.message() << std::endl;
        return boost::make_unexpected(error);

      }).valid();
}
```
This is an example of monadic programming, which is better described in the [Expected](#expected) documentation. The callbacks provided to the `bind` function calls are only invoked if the operation was successful, and the `catch_error` callback is only invoked if *any* of the previous operations failed. This eliminates the need for client code to check for errors after each operation. Also, in this example all values are *moved*, not copied, so it is efficient as well.

> Using monadic programming with boost expected will require the usage of [`maidsafe::nfs::monadic`](#monadic).

### Hello World Concatenation ###
```c++
bool HelloWorld(maidsafe::nfs::Container& container) {
  auto put_part1 = container.Put(
      "split_example/part1", "hello ", maidsafe::nfs::ModifyBlobVersion::Create());
  auto put_part2 = container.Put(
      "split_example/part2", "world", maidsafe::nfs::ModifyBlobVersion::Create());

  const auto put_part1_result = put_part1.get();
  const auto put_part2_result = put_part2.get();

  if (put_part1_result && put_part2_result) {
    auto get_part1 = container.Get("split_example/part1", put_part1_result->version());
    auto get_part2 = container.Get("split_example/part2", put_part2_result->version());

    const auto get_part1_result = get_part1.get();
    const auto get_part2_result = get_part2.get();

    if (get_part1_result && get_part2_result) {
      std::cout <<
        get_part1_result->result() <<
        get_part2_result->result() <<
        std::endl;
      return true;
    }
  }

  return false;
}
```
In this example, both `Put` calls are done in parallel, and both `Get` calls are done in parallel. Each `Get` call cannot be requested until the corresponding `Put` operation completes. Also, these files are **not** stored in a child `Container` called "split_example", but are stored in the `container` object directly.

This examples uses the `->` operator on the `boost::expected` object instead of `.value()` like in the [exception example](#hello-world-exception-style). The `->` operator does not check if the `boost::expected` has an error (similar to `->` being unchecked for `boost::optional`); the conversion to bool in the if statement is the check for validity.

## REST Style API ##
All public functions listed in this API provide the strong exception guarantee. All public const methods are thread-safe.

### maidsafe::nfs::BlobVersion ###
> maidsafe/nfs/blob_version.h

- [ ] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

Blobs stored at the same key are differentiated/identified by a `BlobVersion` object. The `BlobVersion` allows Posix API users to retrieve older revisions of Blobs, or place constraints on operations that change the blob associated with a key.

```c++
class BlobVersion {
  bool Equal(const BlobVersion&) const noexcept;
};

bool operator==(const BlobVersion&, const BlobVersion&) noexcept;
bool operator!=(const BlobVersion&, const BlobVersion&) noexcept;
```
- **Equal(const BlobVersion&)**
  - Return true if *this BlobVersion is equivalent to the BlobVersion given in the parameter.
- The non-member operator overloads call the Equal function.

### ContainerVersion ###
> maidsafe/nfs/container_version.h

- [ ] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

Each time a Blob is stored, or a container pointer is modified, a new version of the parent Container is created. The ContainerVersion object allows users of the Posix API to reference specific versions of the Container.

```c++
class ContainerVersion {};
``` 

### maidsafe::nfs::ModifyBlobVersion ###
> maidsafe/nfs/modfy_blob_version.h

- [ ] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

Operations on [`PosixContainer`](#maidsafenfsposixcontainer) that change the Blob stored at a key require a ModifyBlobVersion object.

```c++
class ModifyBlobVersion {
  static ModifyBlobVersion Create();
  static ModifyBlobVersion Latest();
  ModifyBlobVersion(BlobVersion);
  
  bool Equal(const ModifyBlobVersion&) const noexcept;
  bool Equal(const BlobVersion&) const noexcept;
};

bool operator==(const ModifyBlobVersion&, const ModifyBlobVersion&) noexcept;
bool operator!=(const ModifyBlobVersion&, const ModifyBlobVersion&) noexcept;

bool operator==(const ModifyBlobVersion&, const BlobVersion&) noexcept;
bool operator!=(const ModifyBlobVersion&, const BlobVersion&) noexcept;

bool operator!=(const BlobVersion&, const ModifyBlobVersion&) noexcept;
bool operator==(const BlobVersion&, const ModifyBlobVersion&) noexcept;
```
- **Create()**
  - Returns an object that indicates the Posix API should only succeed if the specified key is unused.
- **Latest()**
  - Returns an object that indicates the Posix API should overwrite any existing Blob at the specified key.
- **ModifyBlobVersion(BlobVersion)**
  - Creates an object that indicates the Posix API should only overwrite the Blob at the specified key if it matches the BlobVersion.
- **Equal(const ModifyBlobVersion&)**
  - Return true if *this ModifyBlobVersion is equivalent to the one given in the parameter.
- **Equal(const BlobVersion&)**
  - Return true if *this ModifyBlobVersion was constructed with an equivalent BlobVersion given in the parameter.
- The non-member operator overloads call the corresponding Equal functions.

### maidsafe::nfs::RetrieveBlobVersion ###
> maidsafe/nfs/retrieve_blob_version.h

- [ ] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

Operations on [`PosixContainer`](#maidsafenfsposixcontainer) that retrieve a Blob stored at a key require a RetrieveBlobVersion object.

```c++
class RetrieveBlobVersion {
  static RetrieveBlobVersion Latest();
  RetrieveBlobVersion(BlobVersion);
  
  bool Equal(const RetrieveBlobVersion&) const noexcept;
  bool Equal(const BlobVersion&) const noexcept;
};

bool operator==(const RetrieveBlobVersion&, const RetrieveBlobVersion&) noexcept;
bool operator!=(const RetrieveBlobVersion&, const RetrieveBlobVersion&) noexcept;

bool operator==(const RetrieveBlobVersion&, const BlobVersion&) noexcept;
bool operator!=(const RetrieveBlobVersion&, const BlobVersion&) noexcept;

bool operator!=(const BlobVersion&, const RetrieveBlobVersion&) noexcept;
bool operator==(const BlobVersion&, const RetrieveBlobVersion&) noexcept;
```
- **Latest()**
  - Returns an object that indicates the Posix API should retrieve the latest Blob stored at the specified key.
- **RetrieveBlobVersion(BlobVersion)**
  - Creates an object that indicates the Posix API needs to retrieve a specific Blob version stored at the specified key.
- **Equal(const RetrieveBlobVersion&)**
  - Return true if *this RetrieveBlobVersion is equivalent to the one given in the parameter.
- **Equal(const BlobVersion&)**
  - Return true if *this RetrieveBlobVersion was constructed with an equivalent BlobVersion given in the parameter.
- The non-member operator overloads call the corresponding Equal functions.

### maidsafe::nfs::Blob ###
> maidsafe/nfs/blob.h

- [x] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

Represents a single stored Blob on the network. Can be given to any valid [`PosixContainer`](#maidsafenfsposixcontainer) so that the contents can be read - this object stores pointers to the data on the network for quicker access. Object is immutable.

> The network currently has no time server of its own, so the timestamps are from the clients. If a client has a misconfigured clock, the timestamps stored will also be incorrect.

```c++
class Blob {
    const BlobVersion& version() const noexcept;
    const std::string& key() const noexcept;
    Clock::time_point creation_time() const noexcept;
    Clock::time_point modification_time() const noexcept;
    std::uint64_t size() const noexcept;
    const std::string& user_meta_data() const noexcept;
    Expected<std::string> data() const;
};
```
- **version()**
  - Returns the version that uniquely references this `Blob`.
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
- **data()**
  - Returns the contents of the `Blob`, if `size()` is less than 3KB. If this is not true, `CommonErrors::cannot_exceed_limit` is returned, and the `RestContainer::Get` function will have to be used instead.

### ContainerOperation<T> ###
> maidsafe/nfs/container_operation.h

- [ ] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

Most REST API users should only need to invoke the result() function to retrieve a `Container` returned in a `Storage::OpenContainer` call. A version is returned for consistency with BlobOperations, and for users that wish to use the [Posix API](posix_api.md) in some situations.

```c++
template<typename T = void>
class ContainerOperation {
  const ContainerVersion& version() const;
  const T& result() const; // iff T != void
};
```

### BlobOperation<T> ###
> maidsafe/nfs/container_operation.h

- [ ] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

Every operation on Blobs return a `BlobOperation` object.

```c++
template<typename T = void>
class BlobOperation {
  const BlobVersion& version() const;
  const T& result() const; // iff T != void
};
```
- **version**
  - Returns the `BlobVersion` involved in the operation.
- **result()**
  - If the operation is NOT void, this will return the result of the operation. Compile error on void.

### OperationError<ExpectedOperation> ###
> maidsafe/nfs/operation_error.h

- [ ] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

This object is returned if a network operation fails.
In the event of a failure, retrieving the cause of the error and a Retry attempt can be done with the `OperationError` interface. The error is a std::error_code object, and the retry attempt will return a new `Future` object with the exact type of the previous failed attempt.

```c++
template<typename ExpectedOperation>
class OperationError {
  using RetryResult = Future<boost::expected<ExpectedOperation, OperationError<ExpectedOperation>>;
  RetryResult Retry() const;
  std::error_code code() const;
};
```
- **Retry**
  - Return a Future to another attempt at the failed operation. Be careful of infinite loops - some operations could fail indefinitely (ModifyBlobVersion::Create() for example). If the retry returns an error code `std::errc::not_supported`, then a retry is not possible.
- **code()**
  - Return error code for the failed operation.

### Future<T> ###
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

### Expected ###
When a network operation has completed, the future will return a [`boost::expected`](https://github.com/ptal/std-expected-proposal) object. On network errors, the `boost::expected` object will contain a OperationError object, and on success the object will contain a BlobOperation or a ContainerOperation object depending on the operation requested. For convenience, the templated types `ExpectedContainerOperation<T>` and `ExpectedBlobOperation<T>` are provided, where `T` is the result of the operation (i.e. a std::string on a `Get` request). Both types assume `OperationError` as the error object for the operation.

#### ExpectedContainerOperation<T> ####
> maidsafe/nfs/expected_container_operation.h

- [ ] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

```c++
template<typename T = void>
using ExpectedContainerOperation =
    boost::expected<ContainerOperation<T>, OperationError<ContainerOperation<T>>>;
```

#### ExpectedBlobOperation<T> ####
> maidsafe/nfs/expected_blob_operation.h

- [ ] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

```c++
template<typename T = void>
using ExpectedBlobOperation =
    boost::expected<BlobOperation<T>, OperationError<BlobOperation<T>>>;
```

#### Monadic ####
The REST API returns `ExpectedContainerOperation<T>` or `ExpectedBlobOperation<T>` objects which use an error type that depends on `T`. This makes monadic programming difficult because the unwrap functions in `boost::expected` will not work as desired. The REST API includes some standalone functions that return a `boost::expected` object with a consistent error type, `std::error_code`. After removing the `OperationError<T>`, retrying the failed operation is not possible. [An example](#hello-world-monad-style) of the standalone functions in use.

> maidsafe/nfs/expected_container_operation.h

```c++
template<typename T>
boost::expected<BlobOperation<T>, std::error_code> monadic(
    const ExpectedBlobOperation<T>& expected);

template<typename T>
boost::expected<BlobOperation<T>, std::error_code> monadic(
    ExpectedBlobOperation<T>&& expected);
```

> maidsafe/nfs/expected_blob_operation.h

```c++
template<typename T>
boost::expected<ContainerOperation<T>, std::error_code> monadic(
    const ExpectedContainerOperation<T>& expected);

template<typename T>
boost::expected<ContainerOperation<T>, std::error_code> monadic(
    ExpectedContainerOperation<T>&& expected);
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
  
  Future<ExpectedOperation<Blob>>              GetBlob(std::string key);
  Future<ExpectedOperation<std::vector<Blob>>> GetBlobHistory(std::string key);
  
  Future<ExpectedOperation<Blob>> CreateBlob(const std::string& key, std::string data, std::string meta_data);
  Future<ExpectedOperation<Blob>> UpdateBlobContent(const Blob& blob, std::string);
  Future<ExpectedOperation<Blob>> UpdateBlobMetadata(const Blob& blob, std::string);
  Future<ExpectedOperation<Blob>> GetBlobContent(const Blob& blob);
  Future<ExpectedOperation<Blob>> GetBlobContent(const Blob& blob, std::uint64_t offset, std::uint64_t length);
  Future<ExpectedOperation<Blob>> DeleteBlob(const Blob& blob);
  
  Future<Blob> Copy(const Blob& from, std::string to);
};
```
- **GetBlobs()**
  - Retrieves the `Blob`s currently in the Container.
- **PutMetaData(std::string key, std::string, ModifyBlobVersion)**
  - Stores the contents at the key as user meta data. 
  - Maximum size is 64kB. 
  - The new `Blob` is returned on completion.
- **GetMetaData(std::string key, RetrieveBlobVersion)**
  - Retrieves the user meta data for a Blob.
- **Put(std::string key, std::string, ModifyBlobVersion)**
  - Stores the Blob contents at the key. 
  - The new `Blob` is returned on completion.
- **Get(Blob)**
  - Retrieves the `Blob` contents. 
  - Faster than the `std::string, Version` overload because the network pointers are contained within the `Blob` object.
- **Get(std::string key, RetrieveBlobVersion)**
  - Retrieves the Blob contents at the key. 
  - Slower than the `Blob` overload because the `Blob` object has to be retrieved first.
- **Delete(std::string key, RetrieveBlobVersion)**
  - Removes the key from the Container.
- **ModifyRange(std::string key, std::string, std::uint64_t offset, ModifyBlobVersion)**
  - Stores a new Blob by re-using a portion of an existing Blob. The size of the Blob is automatically extended if offset exceeds the size of the original Blob. The new `Blob` is returned on completion.
- **GetRange(Blob, std::size_t length, std::uint64_t offset)**
  - Retrieves a portion of the Blob contents at the key.
  - Returned std::string is automatically truncated if length + offset exceeds Blob size. 
  - Faster than the `std::string, Version` overload because the network pointers are contained within the Blob object.
- **GetRange(std::string key, std::size_t length, std::uint64_t offset, RetrieveBlobVersion)**
  - Retrieves a portion of the Blob contents at the key. 
  - Returned std::string is automatically truncated if length + offset exceeds Blob size.
  - Slower than the `Blob` overload because the `Blob` object has to be retrieved first.
- **Copy(Blob from, std::string to, ModifyBlobVersion)**
  - Copies user metadata + data from a Blob at one key to a Blob at another key.
  - Faster than the `std::string, Version` overload because the network pointers are contained within the Blob object.
- **Copy(std::string from, RetrieveBlobVersion, std::string to, ModifyBlobVersion)**
  - Copies user metadata + data from a Blob at one key to a Blob at another key.
  - Slower than the `Blob` overload because the `Blob` object has to be retrieved first.
