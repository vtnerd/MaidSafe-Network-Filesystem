# Maidsafe App API #
The Maidsafe API strives to be easy to use, consistent, and flexible for advanced users requiring performance. Some of the design choices may seem to oppose the ease of use goal, so the rational behind their use is discussed.

## Storage Abstractions ##
### File ###
Data on the SAFE network is stored in Files. A File can contain text or binary data, and the SAFE network has no upward limit on File size. However, local system contraints may apply to maximum File size. Every File also has a name, which is represented by a string of UTF-8 characters.

### Directory ###
A Directory is a virtual object containing other Directories or Files, and must contain at least one Directory or File (a Directory must have 1 File or Directory, and can have 0-∞ Files and 0-∞ Directories). Every Directory has a name, which is represented by a string of UTF-8 characters.

### Path ###
The SAFE network uses the same concept as Posix filesystems to specify Directories and Files. A Directory or File in the SAFE network is referenced by a string that corresponds to a virtual location on the network. Directories are separated `/` and the group of characters after the last `/` represents the filename. For example: `/file1` references a file named `file1` in the top-level (unnamed root) directory, whereas `/directory1/file1` references a file named `file1` in the directory `directory1`. Despite having the same name, these two files are **not** the same because they are in different directories.

Every SAFE identity has its own unique unnamed root. For example `/directory/file1` in the identity representing `user1` is **not** the same file as `/directory/file1` in the identity representing `user2`. This allows users to store data securely and separately from other users on the network. Developers *should* be aware that different SAFE Apps using the same identity can see the files stored by other SAFE Apps. Until otherwise stated, if a user wants to keep files hidden from a specific SAFE App, a different identity will have to be used.

#### Network Path Auto-Creation and Deletion ####
The SAFE API has no functions for creating directories, instead the directories are created as-needed in an `Open` or `Put` call. Directories are also automatically deleted when they contain no files or folders. The Delete function in the API will work with files or directories; in the latter case all child directories and files are removed from the directory and then the directory is removed.

## Versioning ##
Every file and directory in the SAFE network is stored as a revision, so that conflicts between SAFE Apps (or multiple instances of the same SAFE App) can be detected. Every SAFE storage operation that modifies data (file or directory contents) requires a Version object, and the operation will fail if the Version object represents an outdated Version from the one currently on the network. SAFE App developers will be responsible for handling version conflicts, no generic solution exists.

Every operation (both modification and read-only operations), will return a Version object which will be the most-up-to-date version known to the SAFE API.

## Futures ##
Every function in the basic API returns a `Future<T>` to some type `T` (the advanced API has a few more options). While this may seem antithetical to the ease-of-use approach, it more accurately represents the behavior of the API functions. For example, typical write calls to the local filesystem are generally not written to the underlying hard-disk when the function returns, and instead are cached at various levels. The client has to make additional function calls to ensure that data reaches disk, and if a disk write fails - which write calls made it to disk? The SAFE API returning a `Future<T>` therefore better represents the behavior - the function stored the necessary information to complete the operation at some later point in time, and the `Future<T>` object will notify the client when that operation completed. In the SAFE API, clients can assume that a requested operation has completed to the SAFE network when the value T can be retrieved from the `Future<T>`.

Signalling errors in Futures (whether Boost or std) is done with exceptions. Since SAFE network will have a high probability of failure (lack of storage space for user, version error, etc.), the SAFE API will only use the exceptions in the futures for *fatal errors*. Fatal errors can (but are not limited to), out-of-memory issues when trying to set the future value, a Future that can never be set due to an internal bug (broken promise). Instead, the SAFE API will return an object that contains the result of the operation or a non-fatal error, but never both (see [Expected](#expected)). Separating between fatal and non-fatal errors allows clients of the SAFE API to choose whether they want to use exceptions for common errors (unlikely), or throw exceptions.

### Example Future Usage ###
```c++
void PrintFile(maidsafe::nfs::Storage& storage, boost::filesystem::path path) {
  // get() blocks until operation is complete
  const auto retrieval_result = storage.Get(path).get(); 
  // ... continued throughout tutorial
}
```
Since the future only uses exceptions for fatal errors, its usage is quite easy. Calling .get() on the Future<T> will block until the operation completes. The function will only throw in fatal conditions, otherwise retrieval_result will contain the result of the operation or a non-fatal error, but never both.

## Expected ##
Every operation in the NFS API (basic or advanced) will provide an `Expected<Operation<T>>` object when complete (see [operation](#operation) section below). If a non-fatal error ocurred during the operation, the Expected object will have an `Operation<Error>` object instead of an object of type `Operation<T>`. Using `Expected<T>`, instead of exceptions with the `Future<T>`, allows for non-fatal errors to be checked in a functional way. 

### Example Expected Usage ###
```c++
void PrintFile(maidsafe::nfs::Storage& storage, boost::filesystem::path path) {
  // get() blocks until operation is complete
  const auto retrieval_result = storage.Get(path).get(); 
  if (retrieval_result) {
    std::cout << "Contents of " << path << " : " << 
                 retrieval_result->result() << std::endl;
  }
  else {
    std::cerr << "Could not retrieve " << path << " : " << 
                 retrieval_result.error().result() << std::endl;
  }
}
```
The `Expected<T>` object has a conversion to bool operator for use with conditional statements, and overloads `operator*` and `operator->`. The easiest way to use the object is like a pointer, but advanced users are encouraged to [read information](https://github.com/ptal/std-expected-proposal) about this object being proposed for a future revision of C++.

Note: the example above used `retrieval_result->result()` instead of `*retrieval_result` because every operation returns an [Expected Operation ](#operation).

## Operation ##
Every operation (failed and successful) in the NFS API (basic or advanced) will provide an `Operation<T>` object when complete (`Operation<Error>` on error). The `Operation<T>` contains the most up-to-date `Version` for the File or Directory, and the result of the operation, `T`. If the operation had no type to return, `Operation<>` is returned, and no result is available.

### Example Operation Usage ###
```c++
void PrintFileThenDelete(maidsafe::nfs::Storage& storage, boost::filesystem::path path) {
  // get() blocks until operation is complete
  const auto retrieval_result = storage.Get(path).get(); 
  if (retrieval_result) {
    std::cout << "Contents of " << path << " : " << 
                 retrieval_result->result() << std::endl;
    const auto deletion_result = storage.Delete(path, retrieval_result->version()).get();
    if (!deletion_result) {
      std::cerr << "Could not delete " << path << " : " << 
                   deletion_result.error().result() << std::endl;
    }
  }
  else {
    std::cerr << "Could not retrieve " << path << " : " << 
                 retrieval_result.error().result() << std::endl;
  }
}
```

## Basic API Usage ##
### Put ###
### Get ###
### Delete ###

## Classes ##
### maidsafe::nfs::Error ###
Alias for maidsafe::NfsErrors, which is an enum.
```c++
enum class Error {
  timed_out,
  version_error,
  insufficient_por,
  insufficient_space
};
```

### maidsafe::nfs::Version ###
Currently an alias for StructuredDataVersions::VersionName in common. The user should never have to manipulate this object (except for copying or moving), so no API for this class is listed.


### maidsafe::nfs::Future<T> ###
A type conforming to [std::future<T>](http://en.cppreference.com/w/cpp/thread/future). [boost::future<T>](http://www.boost.org/doc/libs/1_57_0/doc/html/thread/synchronization.html#thread.synchronization.futures) is currently the type being used, but a type supporting non-allocating future promises may be used eventually. Extensions in the `boost::future<T>` implementation are **not** guaranteed to be available in future releases, so use at your own risk. Additionally, non-member extension functions are **not** guaranteed to be available in future releases. It is therefore recommended to only use `maidsafe::nfs::Future<T>` as-if it were a C++11 `std::future<T>` object.


### maidsafe::nfs::Expected<T> ###
A type conforming to the proposed [expected<T>](https://github.com/ptal/std-expected-proposal) interface. All functionality listed in [N4109](http://isocpp.org/blog/2014/07/n4109) can be assumed to be available in future releases.

### maidsafe::nfs::Operation<T> ###
All requests to the SAFE network will yield an Operation<T> object. Every Operation object will have a version, and a result value (which can be void).

```c++
template<typename T = void>
class Operation {
 public:
  const Version& version() const;
  const T& result() const; // if T != void
};
```

### maidsafe::nfs::FutureExpectedOperation<T> ###
This is an alias for `maidsafe::nfs::Future<maidsafe::nfs::Expected<maidsafe::nfs::Operation<T>>>`. Returned by every function in the basic API, and every function in the advanced API when `maidsafe::nfs::use_future` is provided instead of a callback.

### maidsafe::nfs::Storage ###


### maidsafe::nfs::File ###
Represents a file stored at a path in the identity of the associated `Storage` object. Read operations on a `File` are never complete until the `AsyncResult<T>` (discussed below) object provided in the read call is notified. Write operations on a `File` are reflected in the local `File` object immediately, but are not stored on the network until the `AsyncResult<T>` object provided in the write call is notified. If a `File` object has write calls pending network confirmation, the `File` object is in an unversioned state. Once all pending write calls succeed in network storage, the `File` is "updated" to the new version (it is the newest version). If one or more pending write calls fails in network storage, the `File` remains unversioned and can never store data on the network again. Additional write calls *can* be done on an object permanently in the unversioned state, but they will only be reflected locally. The `Copy` methods in the `Storage` class **do** work on `File` objects in the permanently unversioned state, so writing to a failed `File` is not useless. Since `File` objects cannot be downgraded or upgraded in version manually, the `Storage` class will have to be used to retrieve alternate stored versions of the `File`.

 State           |  State after Write Call  | Will Writes Succeed                        
 ----------------|--------------------------|--------------------------------------------
 Current Version |   Unversioned            | Always
 Old Version     |   Unversioned            | Never
 Unversioned     |   Unversioned            | Only if previous state was Current Version
 
Since write operations are reflected immediately in the local `File` object, users do not have to wait for the previous operation to complete to make additional read or write calls. Internally, the class will automatically group writes together if possible, and will otherwise wait if some writes are in-progress. Since subsequent writes fail, a user can use the overloads that do not take an `AsyncResult<T>` object (no notification of network storage) on all but the last write. The last `AsyncResult<T>` will indicate whether all previous writes are successfully stored on the network. However, this style of implementation loses some granular error reporting; writes after the first failure will return a more generic error code.
 
If multiple `File` objects are opened within the same process, they are treated no differently than `File` objects opened across different processes or even systems. Simultaneous reads can occur, and simultaneous writes will result in only one of the `File` objects successfully writing to the network. All other `File` objects become permanently unversioned.

Parameters labeled as `AsyncResult<T>` affect the return type of the function, and valid values are:
- A callback that accepts `maidsafe::nfs::Expected<maidsafe::nfs::Operation<T>>`; return type is void
- A boost::asio::yield_context object; return type is `maidsafe::nfs::Expected<maidsafe::nfs::Operation<T>>`.
- A maidsafe::nfs::use_future; return type is `maidsafe::nfs::FutureExpectedOperation<T>`.

```C++
class File {
 public:
  typedef detail::MetaData::TimePoint TimePoint;
  
  // If document is modified locally, but not yet stored to
  // network, empty optional is returned (no _actual_ version
  // currently exists).
  boost::optional<Version> version() const;

  const boost::filesystem::path& name() const; // full-path name
  std::uint64_t file_size() const;
  TimePoint creation_time() const;
  TimePoint write_time() const; // write time of this revision
  
  std::uint64_t get_offset() const;
  void set_offset(std::uint64_t);
  
  // Offset is implied through setters above.
  unspecified Read(boost::asio::buffer, AsyncResult<std::uint64_t>);
  unspecified Write(boost::asio::buffer, AsyncResult<>);
  unspecified Truncate(std::uint64_t, AsyncResult<>);
  
  void Write(boost::asio::buffer);
  void Truncate(std::uint64_t);
};
```
