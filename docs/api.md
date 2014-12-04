# Maidsafe App API #
The Maidsafe API strives to be easy to use, consistent, and flexible for advanced users requiring performance. Some of the design choices may seem to oppose the ease of use goal, so the rational behind their use is discussed.

## Storage Abstractions ##
### Blobs ###
Data on the SAFE network is stored in `Blob`s. A `Blob` can contain text or binary data, and the SAFE network has no upward limit on size. However, local system contraints may apply to maximum size. Each Blob is immutable, once it is stored it cannot be modified. `Blob`s are stored in a versioned structure, `Container`, so the appearance of mutability can be achieved (new versions).

### Container ###
A `Container` stores `Blobs` or a pointer to another `Container`, at keys that uniquely identify the object. The key has no restrictions (any sequence of bytes are valid). The value stored at each key is versioned, so past `Blob`s or `Container` pointers can be retrieved. Most users should **not** use nested `Container`ers, see [nested containers](#nested-containers-and-blob-forks). The API prevents cycles or self-referential `Container`s.

#### Root-Level Container ####
The top-most `Container`ers are stored in a special object that does not store `Blob`s. This special object is versioned, so the history of `Container` pointers at the top-most level can be reviewed.

#### Nested Containers and Blob Forks ####
The chunk information for each `Blob` is stored directly in the `Container`, but only a reference ID (a pointer) is stored for child `Container`s. Since a child `Container` is a pointer to another `Container` on the network, a key can have multiple reference IDs stored in its history for a child `Container`. If the client treats children `Container`s as directories on a local filesystem, the result can be a fork in the history. The problem is if a child `Container` is deleted and re-created while another process is writing to the same `Container`:
```
                        Container(users)-->V1["user1":foo]-->V2["user1:foo2]
                        /
Container(root)-->V1["users"]-->V2[]-->V3["users"]
                                              \
                                             Container(users)-->V1["user1":bar]
```
If treated as a filepath, "/users/user1" would have two different histories depending on what version of the root was retrieved. Clients are encouraged to only create a container at the top-level (at the `Account` object level), and rarely delete them. Advanced clients will have to handle these data fork issues; no mechanism for detecting forks and reacting to them currently exists.

#### Container Keys != Filesystem Paths ####
Containers are nested, but they cannot be used like paths. You cannot open "/container1/container2/document"; a "/" character has no special meaning in a `Container` key. This is intentional, [nested containers are complicated](#nested-containers-and-blob-forks), and should generally be avoided.

## Versioning ##
Every key in a `Container` is stored as a revision, so that conflicts between SAFE Apps (or multiple instances of the same SAFE App) can be detected. Every SAFE storage operation that modifies the contents stored at a key requires a `Version` object, and the operation will fail if the `Version` object represents an outdated `Version` from the one currently stored in the `Container`. SAFE App developers will be responsible for handling version conflicts, no generic solution exists.

Every operation will return a `Version` object which will be the most-up-to-date version known to the SAFE API.

## Basic API ##
## Futures ##
Every basic API function call that requires a network operation returns a `maidsafe::nfs::Future` object. This prevents the interface from blocking, and provides an interface for signalling completion. Currently `maidsafe::nfs::Future` is a `boost::future` object, but this may be changed to a non-allocating design. It is recommended that you use the typedef (`maidsafe::nfs::Future`) in case the implementation changes.

In the basic API, the `Future` will only throw exceptions on non-network related errors (std::bad_alloc, std::bad_promise, etc.). Values and network related errors are returned in a `boost::expected` object.

## Expected ##
When a network operation has completed, the future will return a [`boost::expected`](https://github.com/ptal/std-expected-proposal) object. On network errors, the `boost::expected` object will contain a OperationError object, and on success the object will contain a BlobOperation or a ContainerOperation object depending on the operation requested. For convenience, the templated types `ExpectedContainerOperation<T>` and `ExpectedBlobOperation<T>` are provided, where `T` is the result of the operation (i.e. a std::string on a `Get` request). Both types assume `OperationError<T>` as the error object for the operation.

## OperationError ##
In the event of a failure, retrieving the cause of the error and a Retry attempt can be done with the `OperationError<T>` interface, where `T` was the result of the failed operation. The Retry attempt will return a new Future object with the exact type of the previous failed attempt.

## Examples ##
### Hello World ###
```c++
bool PrintHelloWorld(const maidsafe::nfs::Container& container) {
  std::error_code error;
  
  const auto put_operation = container.Put("example_blob", "hello world", ModifyVersion::New()).get();
  if (put_operation) {
    const auto get_operation = container.Get("example_blob", put_operation->version()).get();
    if (get_operation) {
      std::cout << get_operation->result() << std::endl;
    }
    else {
      error = get_operation.error().code();
    }
  }
  else {
    error = get_operation.error().code();
  }
  
  if (error) {
    std::cerr << "Hello world failed" << error.message() << std::endl;
    return false;
  }
  
  return true;
}
```
The `Put` call uses `ModifyVersion::New()` to indicate that it is creating and storing a new file. If an existing file exists at `example_blob`, then `if (put_operation)` will return false because `put_operation` contains an error (so after running this program once, all subsequent runs should fail). The `Get` call uses the `Version` returned by the `Put`, guaranteeing that the contents from the original `Put` ("hello world"), are retrieved. Alternatively, `RetrieveVersion::Latest()` could've been used instead, but if another process or thread updated the file, the new contents would be returned, which may not be "hello world" as desired.

The `.get()` calls after the `Put` and `Get` indicate that the process should wait until the SAFE network successfully completes the requested operation. The `Future<T>` object allows a process to make additional requests before prior requests have completed. If the above example issued the `Get` call without waiting for the `Put` `Future<T>` to signal completion, the `Get` could've failed. So the `Future<T>` will signal when the result of that operation can be seen by calls locally or remotely.

### Hello World Retry ###
```c++
namespace {
  template<typename Result>
  boost::optional<BlobOperation<Result>> GetOperationResult(
      ExpectedBlobOperation<Result> operation) {
    while (!operation) {
      if (operation.error().code() != std::errc::network_down) {
        std::cerr << 
            "Hello world failed: " << operation.error().code().message() << std::endl;
        return boost::none;
      }
      operation = operation.error().Retry().get();
    }
    return *operation;
  }
}

bool PrintHelloWorld(const maidsafe::nfs::Container& container) {
  const boost::optional<BlobOperation<>> put_operation(
      GetOperationResult(
          container.Put(
              "example_blob", "hello world", ModifyVersion::New()).get()));
  if (put_operation) {
     const boost::optional<BlobOperation<std::string>> get_operation(
        GetOperationResult(
            container.Get(
                "example_blob", "hello world", put_operation->version()).get());
    if (get_operation) {
      std::cout << get_operation->result() << std::endl;
      return true;
    }
  }
  return false;
}
```
This is identical to the [hello world](#hello-world) example, except `Put` and `Get` operations that failed due to the network being down (no connection) are retried. In production code you may want to limit the attempts.

### Hello World Monad ###
```c++
bool PrintHelloWorld(const maidsafe::nfs::Container& container) {
  return container.Put("example_blob", "hello world", ModifyVersion::New()).get().then(
      [&container](BlobOperation<> put_operation) {
        return container.Get("example_blob", put_operation->version()).get();
      }).then([](BlobOperation<std::string> get_operation) {
        std::cout << get_operation->result() << std::endl;
      }).catch_error([](auto operation_error) {
        std::cerr << "Hello world failed" << operation_error.code().message() << std::endl;
      });
}
```
> This would almost work, except the error values differ. Will have to come up with a solution that allow this style of programming.

### Hello World Concatenation ###
```c++
bool PrintHelloWorld(const maidsafe::nfs::Container& container) {
  const auto put_part1 = container.Put(
      "split_example/part1", "hello ", maidsafe::nfs::ModifyVersion::New());
  const auto put_part2 = container.Put(
      "split_example/part2", "world", maidsafe::nfs::ModifyVersion::New());
      
  const auto put_part1_result = put_part1.get();
  const auto put_part2_result = put_part2.get();
  
  if (put_part1_result && put_part2_result) {
    const auto get_part1 = container.Get(
        "split_example/part1", put_part1_result->version());
    const auto get_part2 = container.Get(
        "split_example/part2", put_part2_result->version());
        
    const auto get_part1_result = get_part1.get();
    const auto get_part2_result = get_part2.get();
    
    if (get_part1_result && get_part2_result) {
      std::cout << get_part1_result->result() << get_part2_result->result() << std::endl;
      return true;
    }
  }

  return false;
}
```
In this example, both `Put` calls are done in parallel, and both `Get` calls are done in parallel. Unfortunately this waits for both `Put` calls to complete before issuing a single `Get` call. Also, these files are **not** stored in a child `Container` called "split_example", but are stored in the `container` object directly.

## Basic API Interface ##
```c++
struct ContainerVersion { /* all private */ };
struct BlobVersion { /* all private */ };

template<typename T = void>
class ContainerOperation {
  const ContainerVersion& version() const;
  const T& result() const; // iff T != void
};

template<typename T = void>
class BlobOperation {
  const BlobVersion& version() const;
  const T& result() const; // iff T != void
};

template<typename OperationResult>
class OperationError {
  using RetryResult = boost::expected<OperationResult, OperationError<OperationResult>>;
  RetryResult Retry() const;
  std::error_code code() const;
};

template<typename T = void>
using ExpectedContainerOperation = 
    boost::expected<ContainerOperation<T>, OperationError<ContainerOperation<T>>>;

template<typename T = void>
using ExpectedBlobOperation =
    boost::expected<BlobOperation<T>, OperationError<BlobOperation<T>>>;

template<typename T>
using Future = boost::future<T>;

class ModifyBlobVersion {
  ModifyBlobVersion(BlobVersion);
  static ModifyBlobVersion New();
  static ModifyBlobVersion Latest();
};

class RetrieveBlobVersion {
  RetrieveBlobVersion(BlobVersion);
  static RetrieveBlobVersion Latest();
};

class ContainerPagination {
  Future<ExpectedContainerOperation<std::vector<std::string>>> GetNext(std::size_t);
  Future<ExpectedContainerOperation<std::vector<std::string>>> GetRemaining();
};

class BlobPagination {
  Future<ExpectedBlobOperation<std::vector<std::pair<std::string, BlobVersion>>>> GetNext(std::size_t);
  Future<ExpectedBlobOperation<std::vector<std::pair<std::string, BlobVersion>>>> GetRemaining();
};

class Account {
  template<typename Fob>
  explicit Account(const Fob& fob);
  
  ContainerPagination ListContainers();
  ContainerPagination ListContainers(std::regex filter);

  Future<ExpectedContainerOperation<Container>> OpenContainer(std::string);
  Future<ExpectedContainerOperation<>>          DeleteContainer(std::string);
};

class Container {
  BlobPagination ListBlobs();
  BlobPagination ListBlobs(std::regex filter);

  Future<ExpectedBlobOperation<>>            Put(std::string key, std::string, ModifyBlobVersion);
  Future<ExpectedBlobOperation<std::string>> Get(std::string key, RetrieveBlobVersion);
  Future<ExpectedBlobOperation<>>            Delete(std::string key, RetrieveBlobVersion);
  
  Future<ExpectedBlobOperation<std::string>> GetRange(
      std::string key, std::uint64_t offset, std::size_t length, RetrieveBlobVersion);

  Future<ExpectedBlobOperation<>> Copy(
      std::string from, RetrieveBlobVersion, std::string to, ModifyBlobVersion);
};
```

## Advanced Information ##
### maidsafe::nfs::ContainerVersion ###
> maidsafe/nfs/container_version.h

Currently an alias for StructuredDataVersions::VersionName in common. The user should never have to manipulate this object (except for copying or moving), so no API for this class is listed.

### maidsafe::nfs::BlobVersion ###
> maidsafe/nfs/blob_version.h

References a specific version of a `Blob`; hash of metadata and data map chunks (so `BlobVersion` is a hash of the contents).

### maidsafe::nfs::RetrieveVersion ###
> maidsafe/nfs/retrieve_version.h

A template that has a special state to indicate latest version. Allows `Get`, `Copy`, or `Open` calls to retrieve a specific version, or just the newest one.

```c++
template<typename Version>
class RetrieveVersion {
  static RetrieveVersion Latest();
  RetrieveVersion(Version);
  
  bool is_latest() const; // True if constructed with Latest();
  const Version& version() const; // throw if is_latest();
};
```

> maidsafe/nfs/retrieve_container_version.h

```c++
using RetrieveContainerVersion = RetrieveVersion<ContainerVersion>;
```

> maidsafe/nfs/retrieve_blob_version.h

```c++
using RetrieveBlobVersion = RetrieveVersion<BlobVersion>;
```

### maidsafe::nfs::ModifyVersion ###
> maidsafe/nfs/modify_version.h

A template that has special states to indicate initial, and latest version. Allows `Put`, or `Copy` calls to overwrite a specific version, create a new one, or blindly overwrite data.

```c++
template<typename Version>
class ModifyVersion {
  enum class Instruction {
    kSpecific = 0,
    kNew,
    kOverwrite
  };

  static ModifyVersion New();
  ModifyVersion(Version);
  
  Instruction instruction() const;
  const Version& version() const; // throw if instruction() != Instruction::kSpecific;
};
```

> maidsafe/nfs/modify_container_version.h

```c++
using ModifyContainerVersion = ModifyVersion<ContainerVersion>;
```

> maidsafe/nfs/modify_blob_version.h

```c++
using ModifyBlobVersion = ModifyVersion<BlobVersion>;
```

### maidsafe::nfs::Operation<T> ###
> maidsafe/nfs/operation.h

Network requests in the basic API will yield an `Operation<T>` object on success. Every `Operation` object will have a version, and a result value (which can be void).

```c++
template<typename Version, typename Result>
class Operation {
  const Version& version() & const;
  Version&& version() &&;
  
  const Result& result() & const; // if Result != void
  Result&& result() &&; // if Result != void
};
```

> maidsafe/nfs/container_operation.h

```c++
template<typename Result = void>
using ContainerOperation = Operation<ContainerVersion, Result>;
```

> maidsafe/nfs/blob_operation.h

```c++
template<typename Result = void>
using BlobOperation = Operation<BlobVersion, Result>;
```

### maidsafe::nfs::OperationError<T> ###
> maidsafe/nfs/operation.h

Network requests in the basic API will yield an `OperationError<T>` object on network failure. Every `OperationError` object will have an error code, and Retry capability that returns a `Future` to a re-attempt at the failed operation.

```c++
template<typename OperationResult>
class OperationError {
  using RetryResult = 
      Future<boost::expected<OperationResult, OperationError<OperationResult>>>
  
  RetryResult Retry() const;
  const std::error_code& code() const;
};
```

### maidsafe::nfs::ExpectedContainerOperation<T> ###
> maidsafe/nfs/expected_container_operation.h

A type conforming to the proposed [expected](https://github.com/ptal/std-expected-proposal) interface. All functionality listed in [N4109](http://isocpp.org/blog/2014/07/n4109) can be assumed to be available in future releases.

```c++
template<typename T = void>
using ExpectedContainerOperation = 
    boost::expected<ContainerOperation<T>, OperationError<ContainerOperation<T>>>;
```

### maidsafe::nfs::ExpectedBlobOperation<T> ###
> maidsafe/nfs/expected_blob_operation.h

A type conforming to the proposed [expected](https://github.com/ptal/std-expected-proposal) interface. All functionality listed in [N4109](http://isocpp.org/blog/2014/07/n4109) can be assumed to be available in future releases.

```c++
template<typename T = void>
using ExpectedBlobOperation = 
    boost::expected<BlobOperation<T>, OperationError<BlobOperation<T>>>;
```

### maidsafe::nfs::Future<T> ###
> maidsafe/nfs/future.h

Returned by all basic API functions that required network access. `Future<T>` is a type conforming to [std::future<T>](http://en.cppreference.com/w/cpp/thread/future). [boost::future<T>](http://www.boost.org/doc/libs/1_57_0/doc/html/thread/synchronization.html#thread.synchronization.futures) is currently the type being used, but a type supporting non-allocating future promises may be used eventually.

```c++
template<typename T>
using Future = boost::future<T>;
```

### maidsafe::nfs::Pagination ###
> maidsafe/nfs/pagination.h

```c++
template<typename T>
class Pagination {
  T GetNext(std::size_t);
  T GetRemaining();
};

using ContainerPagination = 
    Pagination<FutureExpectedContainerOperation<std::vector<std::string>>>;
using BlobPagination =
    Pagination<FutureExpectedBlobOperation<std::vector<std::pair<std::string, BlobVersion>>>>;
```

### maidsafe::nfs::Account ###
> maidsafe/nfs/account.h

An `Acccount` object is tied to an identity on the SAFE network. It only has `Container`s, and also supports versioning. This allows for multiple `Container`s to be mapped to the same name (in different versions).

```c++
class Account {
  //
  // Basic API
  //
  template<typename Fob>
  explicit Account(const Fob& fob);
  
  ContainerPagination ListContainers();
  ContainerPagination ListContainers(std::regex filter);

  Future<ExpectedContainerOperation<Container>> OpenContainer(std::string);
  Future<ExpectedContainerOperation<>>          DeleteContainer(std::string);
      
  //
  // Advanced API
  //
  unspecified ListVersions(AsyncResult<std::vector<ContainerVersion>>);
  
  unspecified ListContainers(
      RetrieveContainerVersion, AsyncResult<std::vector<std::string>>);
  unspecified ListContainers(
      RetrieveContainerVersion, std::regex filter, AsyncResult<std::vector<std::string>>);

  unspecified OpenContainer(
      RetreiveContainerVersion, std::string, AsyncResult<std::shared_ptr<Container>>);
  unspecified DeleteContainer(
      RetrieveContainerVersion, std::string, AsyncResult<>);
};
```

### maidsafe::nfs::Container ###
> maidsafe/nfs/container.h

The `Container` class stores `Blob` objects or pointers to a `Container` at SAFE network. Construction of a `Container` object requires a FOB object for identifying an identity on the network, or a parent `Container`. There is an additional constructor for test purposes only - it takes a local filesystem path for storing data. A `Container` object constructed in that mode will never store data on the SAFE network.

The `Put` and `Get` methods will move the content `std::string` to the `Future<T>` upon success. This allows advanced users to re-use buffers (and explains why the `Put` method returns a std::string as a result). The `Get` overload that does not accept a std::string as a parameter will create a new std::string as needed.

Parameters labeled as `AsyncResult<T>` affect the return type of the function, and valid values are:
- A callback that accepts `boost::expected<T, std::error_code>`; return type is void
- A boost::asio::yield_context object; return type is `boost::expected<T, std::error_code>`.
- A boost::asio::use_future; return type is `boost::future<boost::expected<T, std::error_code>>`.

```c++
class Container {
  //
  // Basic API
  //
  BlobPagination ListBlobs();
  BlobPagination ListBlobs(std::regex filter);

  FutureExpectedBlobOperation<>            Put(std::string key, std::string, ModifyBlobVersion);
  FutureExpectedBlobOperation<std::string> Get(std::string key, RetrieveBlobVersion);
  FutureExpectedBlobOperation<>            Delete(std::string key, RetrieveBlobVersion);
  
  FutureExpectedBlobOperation<std::string> GetRange(
      std::string key, std::uint64_t offset, std::size_t length, RetrieveBlobVersion);

  FutureExpectedBlobOperation<> Copy(
      std::string from, RetrieveBlobVersion, std::string to, ModifyBlobVersion);
      
  //
  // Advanced API
  //
  unspecified ListVersions(AsyncResult<std::vector<ContainerVersion>>);
  
  unspecified ListContainers(
      RetrieveContainerVersion, AsyncResult<std::vector<std::string>>);
  unspecified ListContainers(
      RetrieveContainerVersion, std::regex filter, AsyncResult<std::vector<std::string>>);

  unspecified ListBlobs(
      RetreieveContainerVersion,
      AsyncResult<std::vector<std::pair<std::string, BlobVersion>>>);
  unspecified ListBlobs(
      RetreieveContainerVersion,
      std::regex filter,
      AsyncResult<std::vector<std::pair<std::string, BlobVersion>>>);
  
  unspecified CreateFile(std::string, AsyncResult<LocalBlob>);
  unspecified OpenFile(std::string, AsyncResult<LocalBlob>);
  
  // The File object can be from a different Storage object,
  // allowing copying between identities
  unspecified Copy(
      const LocalBlob& from, std::string to, ModifyVersion, AsyncResult<>);
};
```

### maidsafe::nfs::LocalBlob ###
> maidsafe/nfs/local_blob.h

Upon initial creation, `LocalBlob` represents a `Blob` stored at a key/version in the associated `Container` object. Write calls are reflected immediately in that object, but the `LocalBlob` becomes unversioned because it does not represent a `Blob` on the network. The current `LocalBlob` can be saved to the network with a call to a `LocalBlob::Commit`, and success of the async operation indicates that the `LocalBlob` now represents the new version returned. `LocalBlob` provides the strong-exception guarantee for all public methods.

Function |  State After Throw   | State After Return                         |State after Successful Async Operation
---------|----------------------|--------------------------------------------|-------------------------------
Read     | Valid and Unchanged. | Unchanged.                                 | Unchanged (buffer has requested contents from LocalBlob).
Write    | Valid and Unchanged. | Unversioned. Buffer is stored in LocalBlob.| Buffer is stored on network, but not visible to remote `Blob`s.
Truncate | Valid and Unchanged. | Unversioned. Data is changed in LocalBlob. | Data change is stored on network, but not visible to remote `Blob`s
Commit   | Valid and Unchanged. | Unchanged.                                 | Local changes are visible to remote `Blob`s. Version matches remote version.

Since write operations are reflected immediately in the `LocalBlob` object, users do not have to wait for the previous operation to complete to make additional read or write calls. The `SimpleAsyncResult` object provided to `LocalBlob::Write` or `LocalBlob::Truncate` calls is notified when the data has been stored to the network. Writes stored on the network are hidden from other clients until the async operation for `LocalBlob::Commit` succeeds.

- Need to specify when write calls will be sent to network - generally not until Commit or Copy call, but there needs to be a sized based event too.

If a `LocalBlob` is unversioned, the async operation for `LocalBlob::Commit` will wait for all uncompleted `LocalBlob::Write` or `LocalBlob::Truncate` calls to complete, and then try to store the new Blob version. If `LocalBlob::Commit` signals failure to the `AsyncResult<>`, all subsequent calls to `LocalBlob::Commit` will continue to fail, however subsequent `LocalBlob::Write` or `LocalBlob::Truncate` async operations can succeed because they indicate when the data has been stored to the network. Changes to the `LocalBlob` object can always be be stored with `Container::Copy`, which will wait for any remaining write calls to complete, and then commit a new version.
 
If multiple `LocalBlob` objects are opened within the same process, they are treated no differently than `LocalBlob` objects opened across different processes or even systems. Simultaneous reads can occur, and simultaneous writes will result in only one of the `LocalBlob` objects successfully writing to the network. All other `LocalBlob` objects become permanently unversioned.

Parameters labeled as `AsyncResult<T>` affect the return type of the function, and valid values are:
- A callback that accepts `boost::expected<T, std::error_code>`; return type is void
- A boost::asio::yield_context object; return type is `boost::expected<T, std::error_code>`.
- A boost::asio::use_future; return type is `boost::future<boost::expected<T, std::error_code>>`

```C++
class Blob {
 public:
  typedef detail::MetaData::TimePoint TimePoint;
  
  const std::string& key() const; // key associated with Blob
  std::uint64_t file_size() const;
  TimePoint creation_time() const;
  TimePoint write_time() const; // write time of this revision
  
  const std::string& user_metadata() const;
  void set_user_metadata(std::string);

  // Version at open/last successful commit
  const BlobVersion& head_version() const;

  // Version of Blob, or unversioned if empty
  boost::optional<BlobVersion> version() const;

  unspecified ListVersions(AsyncResult<std::vector<BlobVersion>>);


  std::uint64_t get_offset() const;
  void set_offset(std::uint64_t);

  // Offset is implied through setters above.
  unspecified Read(boost::asio::buffer, AsyncResult<std::uint64_t>);
  unspecified Write(boost::asio::buffer, AsyncResult<>);
  unspecified Truncate(std::uint64_t, AsyncResult<>);

  unspecified commit(AsyncResult<>);
};
```
