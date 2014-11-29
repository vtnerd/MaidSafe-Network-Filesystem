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
Nested `Container` objects are only recommended for advanced users. A `Container` pointer can be deleted, and then a new `Container` pointer with the same key can be created. If an application is treating a `Container` like a directory, this can give the appearance of a "forked" Blob. For example, given the sequence:

```
CREATE TOP-MOST "example" -> CONTAINER(1)
STORE ["dir1"->CONTAINER(2)] in CONTAINER(1)
STORE ["file1"->"BLAH"] in CONTAINER(2)
STORE ["dir1"->CONTAINER(3)] in CONTAINER(1)
STORE [file1"->"NO_BLAH"] in CONTAINER(3)
```
An application could write to CONTAINER(2)["file1"] and not be aware that CONTAINER(1) no longer pointed to CONTAINER(2). If an application were to treat this like a directory structure, it would not be aware of the "fork" consequences. In other words, CONTAINER(1) has two pointers for "dir1" in its versioned history, so clients can see two different histories for "dir1"/"file1". Thus, containers shall not be treated like directories.

#### Container Keys != Filesystem Paths ####
Containers are nested, but they cannot be used like paths. You cannot open "/container1/container2/document"; a "/" character has no special meaning in a `Container` key. This is intentional, [nested containers are complicated](#nested-containers-and-blob-forks), and should generally be avoided.

## Versioning ##
Every key in a `Container` is stored as a revision, so that conflicts between SAFE Apps (or multiple instances of the same SAFE App) can be detected. Every SAFE storage operation that modifies the contents stored at a key requires a `Version` object, and the operation will fail if the `Version` object represents an outdated `Version` from the one currently stored in the `Container`. SAFE App developers will be responsible for handling version conflicts, no generic solution exists.

Every operation will return a `Version` object which will be the most-up-to-date version known to the SAFE API.

## Futures ##
Every function in the basic API returns a `Future<T>` to some type `T` (the advanced API has a few more options). While this may seem antithetical to the ease-of-use approach, it more accurately represents the behavior of the API functions. For example, typical write calls to the local filesystem are generally not written to the underlying hard-disk when the function returns, and instead are cached at various levels. The client has to make additional function calls to ensure that data reaches disk, and if a disk write fails - which write calls made it to disk? The SAFE API returning a `Future<T>` therefore better represents the behavior - the function stored the necessary information to complete the operation at some later point in time, and the `Future<T>` object will notify the client when that operation completed. In the SAFE API, clients can assume that a requested operation has completed to the SAFE network when the value T can be retrieved from the `Future<T>`.

Signalling errors in Futures (whether Boost or std) is done with exceptions. Since SAFE network will have a high probability of failure (lack of storage space for user, version error, etc.), the SAFE API will indicate network failures in the `Expected<T>` object (see [Expected](#expected)). System/programming errors (out-of-memory, broken promise) will use the exceptions feature in the `Future<T>`. Signalling network errors in the `Expected<T>` means clients of the API can safely assume that exceptions in the API are a rare event (most can assume that an exception is a fatal event that should stop the process).

### Example Future Usage ###
```c++
void PrintFile(maidsafe::nfs::Container& container, std::string key) {
  // get() blocks until operation is complete
  const auto retrieval_result = storage.Get(
      key, maidsafe::nfs::RetrievalVersion::Latest()).get(); 
  // ... continued throughout tutorial
}
```
Since the future only uses exceptions for fatal errors, its usage is quite easy. Calling .get() on the Future<T> will block until the operation completes. The function will only throw in fatal conditions, otherwise retrieval_result will contain the result of the operation or a non-fatal error, but never both.

## Expected ##
Every operation in the NFS API (basic or advanced) will provide an `Expected<Operation<T>>` object when complete (see [operation](#operation) section below). If a non-fatal error ocurred during the operation, the Expected object will have an `Operation<Error>` object instead of an object of type `Operation<T>`. Using `Expected<T>`, instead of exceptions with the `Future<T>`, allows for non-fatal errors to be checked in a functional way. 

### Example Expected Usage ###
```c++
void PrintFile(maidsafe::nfs::Container& container, std::string key) {
  // get() blocks until operation is complete
  const auto retrieval_result = container.Get(
      key, maidsafe::nfs::RetrievalVersion::Latest()).get(); 
  if (retrieval_result) {
    std::cout << "Contents of " << key << " : " << 
                 retrieval_result->result() << std::endl;
  }
  else {
    std::cerr << "Could not retrieve " << key << " : " << 
                 retrieval_result.error().result() << std::endl;
  }
}
```
- Note: the example above used `retrieval_result->result()` and `retrieval_result.error().result()` because the `Expected` class wraps an [Operation ](#operation) on success or failure.

The `Expected<T>` object has a conversion to bool operator for use with conditional statements, and overloads `operator*` and `operator->`. The easiest way to use the object is like a pointer, but advanced users are encouraged to [read information](https://github.com/ptal/std-expected-proposal) about this object being proposed for a future revision of C++. The `E` in `expected<T,E>` will **always** be `Operation<Error>`, thus `maidsafe::nfs::Expected<maidsafe::nfs::Operation<T>>` is shorthand for `expected<maidsafe::nfs::Operation<T>, maidsafe::nfs::Operation<madisafe::nfs::Error>>`. A `Operation<T>` is provided in success or error so the user can grab the latest `Version` of the file.



## Operation ##
Every operation (failed and successful) in the NFS API (basic or advanced) will provide an `Operation<T>` object when complete (`Operation<Error>` on error). The `Operation<T>` contains the key for the operation, the most up-to-date `Version` for the `Container` key, and the result of the operation, `T`. If the operation had no type to return, `Operation<>` is returned, and no result is available.

### Example Operation Usage ###
```c++
void PrintFileThenDelete(maidsafe::nfs::Container& container, std::string key) {
  // get() blocks until operation is complete
  const auto retrieval_result = container.Get(
      key, maidsafe::nfs::RetrievalVersion::Latest()).get(); 
  if (retrieval_result) {
    std::cout << "Contents of " << key << " : " << 
                 retrieval_result->result() << std::endl;
    const auto deletion_result = container.Delete(key, retrieval_result->version()).get();
    if (!deletion_result) {
      std::cerr << "Could not delete " << key << " : " << 
                   deletion_result.error().result() << std::endl;
    }
  }
  else {
    std::cerr << "Could not retrieve " << key << " : " << 
                 retrieval_result.error().result() << std::endl;
  }
}
```

## Basic API ##
```c++
struct BlobVersion { /* all private */ };

template<typename T = void>
class Operation {
  const Version& version() const; // LEE NOTE: Now screwed with multiple Version types
  const T& result() const; // iff T != void
};

template<typename T>
using Future = boost::future<T>;

template<typename T = void>
using Expected = boost::expected<Operation<T>, Operation<std::error_code>>;

template<typename T>
using FutureExpectedOperation = Future<Expected<Operation<T>>>

class ModifyBlobVersion {
  ModifyBlobVersion(BlobVersion);
  static ModifyBlobVersion New();
};

class RetrieveBlobVersion {
  RetrieveBlobVersion(BlobVersion);
  static RetrieveBlobVersion Latest();
};

template<typename Result>
class Pagination {
  FutureExpectedOperation<std::vector<Result>> GetNextRange(std::size_t);
};

class Account {
  template<typename Fob>
  explicit Account(const Fob& fob);
  
  FutureExpectedOperation<std::vector<std::string>> ListAllContainers();
  Pagination<std::string>              ListContainers();
  FutureExpectedOperation<Container>   OpenContainer(std::string);
  FutureExpectedOperation<>            DeleteContainer(std::string);
};

class Container {
  FutureExpectedOperation<std::vector<std::pair<std::string, BlobVersion>>> ListAllBlobs();
  Pagination<std::pair<std::string, BlobVersion>>> ListBlobs();

  FutureExpectedOperation<>            Put(std::string key, std::string, ModifyBlobVersion);
  FutureExpectedOperation<std::string> Get(std::string key, RetrieveBlobVersion);
  FutureExpectedOperation<>            Delete(std::string key, BlobVersion);
  
  FutureExpectedOperation<std::string> GetRange(
      std::string key, std::uint64_t offset, std::size_t length, RetrieveBlobVersion);

  FutureExpectedOperation<> Copy(
      std::string from, RetrieveBlobVersion, std::string to, ModifyBlobVersion);
};
```
This isn't as daunting as it looks! Lets go over a quick example, that uses the local filesystem first (**now outdated -- update!**)

### Local Filesystem Hello World ###
```c++
int main() {
  maidsafe::nfs::Container test_storage(
      "/home/user/test_safe_storage", MaxDiskUsage(10485760));
  
  const auto put_result = test_storage.Put(
      "/simple_example", "hello world", maidsafe::nfs::ModifyVersion::New()).get();
      
  if (put_result) {
    const auto get_result = test_storage.Get("simple_example", put_result->version()).get();
    if (get_result) {
      std::cout << get_result->result() << std::endl;
      return EXIT_SUCCESS;
    }
  }
  return EXIT_FAILURE;
}
```
The `test_storage` object is created with a `boost::filesystem::path` object - meaning writes through this object will be stored at that local path (in 3 encrypted chunks). After running this example, you should see files in the directory given to the constructor of `Container`. The `Put` call uses `ModifyVersion::New()` to indicate that it is creating and storing a new file. If an existing file exists at `test_storage:/test_example`, then `if (put_result)` will return false because `put_result` contains an error (so after running this program once, all subsequent runs should fail). The `Get` call uses the `Version` returned by the `Put`, guaranteeing that the contents from the original `Put` ("hello world"), are retrieved. Alternatively, `RetrieveVersion::Latest()` could've been used instead, but if another process or thread updated the file, the new contents would be returned, which may not be "hello world" as desired.

The `.get()` calls after the `Put` and `Get` indicate that the process should wait until the SAFE network (in this example, the local filesystem), successfully completes the requested operation. The `Future<T>` object allows a process to make additional requests before prior requests have completed. If the above example issued the `Get` call without waiting for the `Put` `Future<T>` to signal completion, the `Get` could've failed. So the `Future<T>` will signal when the result of that operation can be seen by calls locally or remotely.

### Local Filesystem Hello World Concatenation ###
```c++
int main() {
  maidsafe::nfs::Container test_storage(
      "/home/user/test_safe_storage", MaxDiskUsage(10485760));
      
  const auto put_part1 = test_storage.Put(
      "split_example/part1", "hello ", maidsafe::nfs::ModifyVersion::New());
  const auto put_part2 = test_storage.Put(
      "split_example/part2", "world", maidsafe::nfs::ModifyVersion::New());
      
  const auto put_part1_result = put_part1.get();
  const auto put_part2_result = put_part2.get();
  
  if (put_part1_result && put_part2_result) {
    const auto get_part1 = test_storage.Get(
        "split_example/part1", put_part1_result->version());
    const auto get_part2 = test_storage.Get(
        "split_example/part2", put_part2_result->version());
        
    const auto get_part1_result = get_part1.get();
    const auto get_part2_result = get_part2.get();
    
    if (get_part1_result && get_part2_result) {
      std::cout << get_part1_result->result() << get_part2_result->result() << std::endl;
      return EXIT_SUCCESS;
    }
  }

  return EXIT_FAILURE;
}
```
In this example, both `Put` calls are done in parallel, and both `Get` calls are done in parallel. Unfortunately this waits for both `Put` calls to complete before issuing a single `Get` call. Also, these files are **not** stored in a child `Container` called "split_example", but are stored in the "test_storage" `Container`, under the keys "split_example/part1" and "split_example/part2".

## Advanced Information ##
### maidsafe::nfs::Version ###
Currently an alias for StructuredDataVersions::VersionName in common. The user should never have to manipulate this object (except for copying or moving), so no API for this class is listed.

### maidsafe::nfs::RetrieveVersion ###
A Version that has a special state to indicate latest version. Allows `Get`, `Copy`, or `Open` calls to retrieve a specific version, or just the newest one.

```c++
class RetrieveVersion {
 public:
  static RetrieveVersion Latest();
  RetrieveVersion(Version);
  
  bool is_latest() const; // True if constructed with Latest();
  const Version& version() const; // throw if is_latest();
};
```

### maidsafe::nfs::ModifyVersion ###
A Version that has a special state to indicate initial version. Allows `Put`, or `Copy` calls to overwrite a specific version, or create a new one.

```c++
class ModifyVersion {
 public:
  static ModifyVersion New();
  ModifyVersion(Version);
  
  bool is_new() const; // True if constructed with New();
  const Version& version() const; // throw if is_new();
};
```

### maidsafe::nfs::Future<T> ###
A type conforming to [std::future<T>](http://en.cppreference.com/w/cpp/thread/future). [boost::future<T>](http://www.boost.org/doc/libs/1_57_0/doc/html/thread/synchronization.html#thread.synchronization.futures) is currently the type being used, but a type supporting non-allocating future promises may be used eventually. Extensions in the `boost::future<T>` implementation are **not** guaranteed to be available in future releases, so use at your own risk. Additionally, non-member extension functions are **not** guaranteed to be available in future releases. It is therefore recommended to only use `maidsafe::nfs::Future<T>` as-if it were a C++11 `std::future<T>` object.


### maidsafe::nfs::Expected<T> ###
A type conforming to the proposed [expected<T>](https://github.com/ptal/std-expected-proposal) interface. All functionality listed in [N4109](http://isocpp.org/blog/2014/07/n4109) can be assumed to be available in future releases.

### maidsafe::nfs::Operation<T> ###
All requests to the SAFE network will yield an Operation<T> object. Every Operation object will have a path, version, and a result value (which can be void).

```c++
template<typename T = void>
class Operation {
 public:
  std::shared_ptr<Container> container() const;
 
  const std::string& key() & const;
  std::string key() &&;
  
  const Version& version() & const;
  Version&& version() &&;
  
  const T& result() & const; // if T != void
  T&& result() &&; // if T != void
};
```

### maidsafe::nfs::FutureExpectedOperation<T> ###
This is an alias for `maidsafe::nfs::Future<maidsafe::nfs::Expected<maidsafe::nfs::Operation<T>>>`. Returned by every function in the basic API, and every function in the advanced API when `maidsafe::nfs::use_future` is provided instead of a callback.

### maidsafe::nfs::Container ###
The `Container` class stores `Blob` objects or pointers to a `Container` at SAFE network. Construction of a `Container` object requires a FOB object for identifying an identity on the network, or a parent `Container`. There is an additional constructor for test purposes only - it takes a local filesystem path for storing data. A `Container` object constructed in that mode will never store data on the SAFE network.

The `Put` and `Get` methods will move the content `std::string` to the `Future<T>` upon success. This allows advanced users to re-use buffers (and explains why the `Put` method returns a std::string as a result). The `Get` overload that does not accept a std::string as a parameter will create a new std::string as needed.

Parameters labeled as `AsyncResult<T>` affect the return type of the function, and valid values are:
- A callback that accepts `maidsafe::nfs::Expected<maidsafe::nfs::Operation<T>>`; return type is void
- A boost::asio::yield_context object; return type is `maidsafe::nfs::Expected<maidsafe::nfs::Operation<T>>`.
- A maidsafe::nfs::use_future; return type is `maidsafe::nfs::FutureExpectedOperation<T>`.

```c++
class Container {
public:
  //
  // Basic API
  //

  // SAFE network storage under fob
  template<typename Fob>
  explicit Container(const Fob& fob)
  
  // Local filesystem test storage
  explicit Container(boost::filesystem::path, MaxDiskUsage);
  
  FutureExpectedOperation<Container> GetContainer(std::string, ModifyVersion);
  
  FutureExpectedOperation<> Put(std::string, std::string, ModifyVersion);
  FutureExpectedOperation<std::string> Get(std::string, RetrieveVersion);
  FutureExpectedOperation<>            Delete(std::string, Version);
  
  FutureExpectedOperation<> Copy(
      std::string from, RetrieveVersion, std::string to, ModifyVersion);
      
  //
  // Advanced API
  //
  FutureExpectedOperation<std::string> Get(std::string, std::string, RetrieveVersion);
  
  unspecified CreateFile(std::string, AsyncResult<std::shared_ptr<LocalBlob>>);
  unspecified OpenFile(std::string, AsyncResult<std::shared_ptr<LocalBlob>>);
  
  // The File object can be from a different Storage object,
  // allowing copying between identities
  unspecified Copy(
      std::shared_ptr<LocalBlob> from, std::string to, ModifyVersion, AsyncResult<>);
};
```

### maidsafe::nfs::LocalBlob ###
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

Parameters labeled as `SimpleAsyncResult` affect the return type of the function, and valid values are:
- A callback that accepts `maidsafe::nfs::Expected<>`; return type is void
- A boost::asio::yield_context object; return type is `maidsafe::nfs::Expected<>`.
- A maidsafe::nfs::use_future; return type is `maidsafe::nfs::Future<maidsafe::nfs::Expected<>>`.

Parameters labeled as `AsyncResult<T>` affect the return type of the function, and valid values are:
- A callback that accepts `maidsafe::nfs::Expected<maidsafe::nfs::Operation<T>>`; return type is void
- A boost::asio::yield_context object; return type is `maidsafe::nfs::Expected<maidsafe::nfs::Operation<T>>`.
- A maidsafe::nfs::use_future; return type is `maidsafe::nfs::FutureExpectedOperation<T>`.

```C++
class Blob {
 public:
  typedef detail::MetaData::TimePoint TimePoint;
  
  const boost::filesystem::path& key() const; // key associated with Blob
  std::uint64_t file_size() const;
  TimePoint creation_time() const;
  TimePoint write_time() const; // write time of this revision

  // Version at open/last successful commit
  BlobVersion head_version() const;

  // Version of Blob, or unversioned if empty
  boost::optional<BlobVersion> version() const;

  unspecified ListVersions(AsyncResult<>);


  std::uint64_t get_offset() const;
  void set_offset(std::uint64_t);

  // Offset is implied through setters above.
  unspecified Read(boost::asio::buffer, AsyncResult<std::uint64_t>);
  unspecified Write(boost::asio::buffer, SimpleAsyncResult<>);
  unspecified Truncate(std::uint64_t, SimpleAsyncResult<>);

  unspecified commit(AsyncResult<>);
};
```
