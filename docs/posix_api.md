# Maidsafe App Posix API [DRAFT] #
> NOTE: This API is subject to change.

The Maidsafe Posix API strives to be consistent, and high performant.

## Storage Abstractions ##
### Blobs ###
Data on the SAFE network is stored in Blobs. A Blob can contain text or binary data, and the SAFE network has no upward limit on size. However, local system contraints may apply to maximum size. Each Blob is immutable, once it is stored it cannot be modified.

### Container ###
A Container stores Blobs or a pointer to another Container at keys that have no restrictions (any sequence of bytes are valid). Each key is versioned, so past Blobs or Container pointers can be retrieved (which gives the appearance of mutability since a new `Blob`s can be stored at an existing key).

> Most users should **not** use nested Containers, see [nested containers](#nested-containers-and-blob-forks).

#### Nested Containers and Blob Forks ####
The chunk information for each Blob is stored directly in the Container, but only a reference ID (a pointer) is stored for child Containers. Since a child Container is a pointer to another Container on the network, a key can have multiple reference IDs stored in its history for a child Container. If the client treats children Containers as directories on a local filesystem, the result can be a fork in the history. The problem is if a child Container is deleted and re-created while another process is writing to the same Container:
```
                        Container(users)-->V1["user1":foo]-->V2["user1:foo2]
                        /
Container(root)-->V1["users"]-->V2[]-->V3["users"]
                                              \
                                             Container(users)-->V1["user1":bar]
```
If treated as a filepath, "/users/user1" would have two different histories depending on what version of the root was retrieved. Clients are encouraged to only create a container at the top-level, and rarely delete them. Advanced clients will have to handle these data fork issues; no mechanism for detecting forks and reacting to them currently exists.

#### Container Keys != Filesystem Paths ####
Containers are nested, but they cannot be used like paths. You cannot open "/container1/container2/document"; a "/" character has no special meaning in a Container key. This is intentional, [nested containers are complicated](#nested-containers-and-blob-forks), and should generally be avoided.

### Storage ###
Storage has 0 more Containers. The Storage can be public, private, or privately-shared.

### StorageID ###
A StorageID identifies a particular Storage instance on the SAFE network, and contains the necessary information to decrypt the contents.



## Posix Style API ##
All public functions in this API provide the strong exception guarantee.

### StorageID ###
> maidsafe/nfs/storage_id.h

- [ ] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

Represents the [`StorageID`](#storageid) abstraction listed above. Obtaining relevant `StorageID` objects are out of the scope of this document.

```c++
class StorageID { /* No Public Elements */ };
```

### BlobVersion ###
> maidsafe/nfs/blob_version.h

- [ ] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

Blobs stored at the same key are differentiated/identified by a `BlobVersion` object. The `BlobVersion` allows REST API users to retrieve older revisions of Blobs, or place constraints on operations that change the blob associated with a key.

```c++
class BlobVersion {
  static BlobVersion Defunct();
};
```
- **Defunct()**
  - Returns a `BlobVersion` that is used to indicate a deleted Blob. This is never returned by a [`BlobOperation`](#bloboperation), and is only used when retrieving the history of the Blobs stored at a key.

### ContainerVersion ###
> maidsafe/nfs/container_version.h

- [ ] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

Containers are also versioned, but none of the REST API functions accept a ContainerVersion. This class is mentioned/returned by `Container` operations for users that wish to use the [Posix API](posix_api.md) in some situations.

```c++
class ContainerVersion { /* No Public Elements */ };
```

### ModifyBlobVersion ###
> maidsafe/nfs/modfy_blob_version.h

- [ ] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

Operations in [`Container`](#container-1) that change the Blob stored at a key require a ModifyBlobVersion object.

```c++
class ModifyBlobVersion {
  static ModifyBlobVersion New();
  static ModifyBlobVersion Latest();
  ModifyBlobVersion(BlobVersion);
};
```
- **New()**
  - Returns an object that indicates the REST API should only succeed if the specified key is unused.
- **Latest()**
  - Returns an object that indicates the REST API should overwrite any existing Blob at the specified key.
- **ModifyBlobVersion(BlobVersion)**
  - Creates an object that indicates the REST API should only overwrite the Blob at the specified key if it matches the BlobVersion.

### RetrieveBlobVersion ###
> maidsafe/nfs/retrieve_blob_version.h

- [ ] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

Operations in [`Container`](#container-1) that retrieve a Blob stored at a key require a RetrieveBlobVersion object.

```c++
class RetrieveBlobVersion {
  static RetrieveBlobVersion Latest();
  RetrieveBlobVersion(BlobVersion);
};
```
- **Latest()**
  - Returns an object that indicates the REST API should retrieve the latest Blob stored at the specified key.
- **RetrieveBlobVersion(BlobVersion)**
  - Creates an object that indicates the REST API needs to retrieve a specific Blob version stored at the specified key.

### maidsafe::nfs::Future<T> ###
> maidsafe/nfs/future.h

- [x] Thread-safe Public Functions
- [ ] Copyable
- [x] Movable

Returned by all basic API functions that required network access. `Future<T>` is a type conforming to [std::future<T>](http://en.cppreference.com/w/cpp/thread/future). [boost::future<T>](http://www.boost.org/doc/libs/1_57_0/doc/html/thread/synchronization.html#thread.synchronization.futures) is currently the type being used, but a type supporting non-allocating future promises may be used eventually.

```c++
template<typename T>
using Future = boost::future<T>;
```

### maidsafe::nfs::Storage ###
> maidsafe/nfs/storage.h

- [x] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

> This object has a single shared_ptr, and is shallow-copied. This makes it extremely quick to copy.

Represents the [`Storage`](#storage) abstraction listed above. Constructing a `Storage` object requires a `StorageID` object.

Parameters labeled as `AsyncResult<T>` affect the return type of the function, and valid values are:
- A callback in the form `void(boost::expected<T, std::error_code>)`; return type is void
- A boost::asio::yield_context object; return type is `boost::expected<T, std::error_code>`.
- A boost::asio::use_future; return type is `boost::future<boost::expected<T, std::error_code>>`.

```c++
class Storage {
  unspecified GetVersions(AsyncResult<std::vector<ContainerVersion>>);
  
  unspecified GetContainers(
      RetrieveContainerVersion, AsyncResult<std::vector<std::string>>);

  unspecified OpenContainer(std::string, ModifyContainerVersion, AsyncResult<Container>);
  unspecified DeleteContainer(std::string, RetrieveContainerVersion, AsyncResult<>);
};
```
- **GetVersions(AsyncResult<std::vector<ContainerVersion>>)**
  - Request the version history of Storage.
  - AsyncResult is given the version history of Storage. A new version is created each time a Container is created or deleted. Oldest `ContainerVersion` is always `ContainerVersion::Defunct()`, and is used subsequently when the key had no associated Container for some period of time. `std::vector::begin()` will be the newest `ContainerVersion`, and `std::vector::end() - 1` will have the oldest `ContainerVersion` (which is always `ContainerVersion::Defunct()`).
- **GetContainers(RetrieveContainerVersion, AsyncResult<std::vector<std::string>>)**
  - Request the list of nested Containers.
  - AsyncResult is given the list of nested containers.
- **OpenContainer(std::string, ModifyContainerVersion, AsyncResult<Container>)**
  - Make a request to open a container at the specified key.
  - AsyncResult is given the nested `Container` with the specified name.
- **DeleteContainer(std::string, RetrieveContainerVersion, AsyncResult<>)**
  - Make a request to delete a container at the specified key.

### maidsafe::nfs::Container ###
> maidsafe/nfs/container.h

- [x] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

> This object has a single shared_ptr, and is shallow-copied. This makes it extremely quick to copy.

Represents the [`Container`](#container) abstraction listed above. Constructing a `Container` object cannot be done directly; `Container` objects can only be retrieved from `Storage::OpenContainer`.

Parameters labeled as `AsyncResult<T>` affect the return type of the function, and valid values are:
- A callback in the form `void(boost::expected<T, std::error_code>)`; return type is void
- A boost::asio::yield_context object; return type is `boost::expected<T, std::error_code>`.
- A boost::asio::use_future; return type is `boost::future<boost::expected<T, std::error_code>>`.

```c++
class Container {
  unspecified GetVersions(AsyncResult<std::vector<ContainerVersion>>);
  
  unspecified GetContainers(
      RetrieveContainerVersion, AsyncResult<std::vector<std::string>>);

  unspecified GetBlobs(
      RetreieveContainerVersion,
      AsyncResult<std::vector<std::pair<std::string, BlobVersion>>>);
  
  unspecified OpenContainer(std::string, ModifyContainerVersion, AsyncResult<Container>);
  unspecified OpenBlob(std::string, ModifyBlobVersion, AsyncResult<LocalBlob>);
  
  unspecified DeleteContainer(std::string, RetrieveContainerVersion, AsyncResult<>);
  unspecified DeleteBlob(std:string, ModifyBlobVersion, AsyncResult<>);
  
  unspecified Copy(
      const LocalBlob& from, std::string to, ModifyVersion, AsyncResult<LocalBlob>);
};
```
> A key can only store a Blob or a nested Container at a given point in time.

- **GetVersions(AsyncResult<std::vector<ContainerVersion>>)**
  - Request the version history of Container.
  - AsyncResult is given the version history of Container. A new version is created each time a Container is created or deleted. Oldest `ContainerVersion` is always `ContainerVersion::Defunct()`, and is used subsequently when the key had no associated Container for some period of time. `std::vector::begin()` will be the newest `ContainerVersion`, and `std::vector::end() - 1` will have the oldest `ContainerVersion` (which is always `ContainerVersion::Defunct()`).
- **GetContainers(RetrieveContainerVersion, AsyncResult<std::vector<std::string>>)**
  - Request the list of nested Containers.
  - AsyncResult is given the list of nested containers.
- **GetBlobs(RetreieveContainerVersion, AsyncResult<std::vector<std::pair<std::string, BlobVersion>>>)**
  - Request the list of Blobs.
  - AsyncResult Retrieves the names of Blobs in the Container. The BlobVersion is provided for each Blob.
- **OpenContainer(std::string, ModifyContainerVersion, AsyncResult<Container>)**
  - Make a request to open a container at the specified key.
  - AsyncResult is given the nested `Container` with the specified name.
- **OpenBlob(std::string, ModifyBlobVersion, AsyncResult<LocalBlob>)**
  - Make a request to open a Blob at the specified key.
  - AsyncResult is given a `LocalBlob` that represents the Blob at the specified key.
- **DeleteContainer(std::string, RetrieveContainerVersion, AsyncResult<>)**
  - Make a request to delete the Container at the specified key.
- **DeleteBlob(std:string, ModifyBlobVersion, AsyncResult<>)**
  - Make a request to delete the Blob at the specified key.
- **Copy(const LocalBlob& from, std::string to, ModifyVersion, AsyncResult<LocalBlob>)**
  - Make a request to copy the contents of the `LocalBlob` to the specified key.
  - AsyncResult is given the new `LocalBlob` associated with the destination of the copy.

### maidsafe::nfs::LocalBlob ###
> maidsafe/nfs/local_blob.h

- [ ] Thread-safe Public Functions
- [ ] Copyable
- [x] Movable

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
- A callback in the form `void(boost::expected<T, std::error_code>)`; return type is void
- A boost::asio::yield_context object; return type is `boost::expected<T, std::error_code>`.
- A boost::asio::use_future; return type is `boost::future<boost::expected<T, std::error_code>>`

```C++
class LocalBlob {
 public:
  typedef detail::MetaData::TimePoint TimePoint;
  
  const std::string& key() const; // key associated with Blob
  std::uint64_t file_size() const;
  TimePoint creation_time() const;
  TimePoint head_write_time() const; // write time of this revision
  
  const std::string& user_metadata() const;
  void set_user_metadata(std::string);

  // Version at open
  const BlobVersion& head_version() const;

  unspecified GetVersions(AsyncResult<std::vector<BlobVersion>>);

  std::uint64_t get_offset() const;
  void set_offset(std::uint64_t);

  unspecified Read(boost::asio::buffer, AsyncResult<std::uint64_t>);
  unspecified Write(boost::asio::buffer, AsyncResult<>);
  unspecified Truncate(std::uint64_t, AsyncResult<>);

  unspecified commit(AsyncResult<BlobVersion>);
};
```
> The network currently has no time server of its own, so the timestamps are from the clients. If a client has a misconfigured clock, the timestamps stored will also be incorrect.

- **key()**
  - Returns the key associated with the Blob
- **file_size()**
  - Returns the file size of the `LocalBlob`. This is *not* necessarily the size of any `Blob` stored on the network.
- **creation_time()**
  - Returns the timestamp of when `key()` last went from storing nothing to storing a Blob.
- **head_write_time()**
  - Returns the timestamp of when the head_version() was stored.
- **user_metadata()**
  - Returns the user metadata being stored.
- **set_user_metadata(std::string)**
  - Sets the user metadata. Binary data is allowed.
- **head_version()**
  - Returns the version from when the `LocalBlob` was opened. This is **not** updated after a `Commit` succeeds.
- **GetVersions(AsyncResult<std::vector<BlobVersion>>)**
  - Request the version history of the Blob.
  - AsyncResult is given the version history of `BlobVersion`s at the key. Oldest `BlobVersion` is always `BlobVersion::Defunct()`, and is used subsequently when the key had no associated Blob for some period of time. `std::vector::begin()` will be the newest `BlobVersion`, and `std::vector::end() - 1` will have the oldest BlobVersion (which is always `BlobVersion::Defunct()`).
- **get_offset()**
  - Returns the offset that will be used by the next Read, Write, or Truncate call.
- **set_offset(std::uint64_t)**
  - Change the value returned by `get_offset()`.
- **Read(boost::asio::buffer, AsyncResult<std::uint64_t>)**
  - Read from the `LocalBlob` starting at `get_offset()` into the provided buffer. The buffer must remain valid until AsyncResult returns.
  - `get_offset()` is immediately updated to `min(file_size() - get_offset(), get_offset() + buffer::size())`
  - AsyncResult is given the number of bytes actually read.
  - Can be invoked before other calls to `Read`, `Write`, `Truncate`, or `Commit` complete.
- **Write(boost::asio::buffer, AsyncResult<>)**
  - Write to the `LocalBlob` starting at `get_offset()` from the provided buffer. The buffer must remain valid until AsyncResult returns.
  - `get_offset()` is immediately updated to `get_offset() + buffer::size()`
  - Can be invoked before other calls to `Read`, `Write`, `Truncate`, or `Commit` complete.
- **Truncate(std::uint64_t, AsyncResult<>)**
  - Change the size of the `LocalBlob` to get_offset() + size bytes.
  - `get_offset()` is immediately updated to `get_offset() + size`
  - Can be invoked before other calls to `Read`, `Write`, `Truncate`, or `Commit` complete.
- **commit(AsyncResult<BlobVersion>)**
  - Make a request to store the contents of the `LocalBlob` at `key()`
  - Storing a  `LocalBlob` at `key()` will fail if another `LocalBlob` modified `key()` since `head_version()`.
  - AsyncResult is given the `BlobVersion` of the new Blob stored on the network.
  - Can be invoked before other calls to `Read`, `Write`, `Truncate`, or `Commit` complete.
