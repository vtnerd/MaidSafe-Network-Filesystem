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

## Examples ##
Network operations in the Posix API are done asynchronously so that client code doesn't block while network operations are being performed. The Posix API uses the [AsyncResult](http://www.boost.org/doc/libs/1_57_0/doc/html/boost_asio/reference/async_result.html) framework from `boost::asio`. This allows clients to use callbacks, stackful co-routines, or futures as the completion signalling mechanism.

### Hello World (Callbacks) ###
```c++
bool HelloWorld(maidsafe::nfs::Storage& storage) {
  using ExpectedContainer =
      boost::expected<maidsafe::nfs::Container, std::error_code>;
  using ExpectedBlob =
      boost::expected<maidsafe::nfs::LocalBlob, std::error_code>;
  using ExpectedVersion =
      boost::expected<maidsafe::nfs::BlobVersion, std::error_code>;
  using FutureExpectedString =
      maidsafe::nfs::Future<maidsafe::nfs::ExpectedBlobOperation<std::string>>;


  boost::promise<boost::expected<std::string, std::error_code>> result;

  storage.OpenContainer(
      "example_container",
      maidsafe::nfs::ModifyContainerVersion::Create(),
      [&result](ExpectedContainer container) {

        if (!container) {
          result.set_value(boost::make_unexpected(container.error()));
          return;
        }

        container->OpenBlob(
            "example_blob",
            maidsafe::nfs::ModifyBlobVersion::Create(),
            [&result, container](ExpectedBlob blob) {

              if (!blob) {
                result.set_value(boost::make_unexpected(blob.error()));
                return;
              }

              blob->Write(boost::asio::buffer("hello world"), []{});
              blob->Commit(
                  [&result, container](ExpectedVersion version) {

                    if (!version) {
                      result.set_value(boost::make_unexpected(version.error()));
                      return;
                    }

                    container->Read("example_blob", *std::move(version)).then(
                        [&result](FutureExpectedString future_read) {

                          const auto read = future_read.get();
                          if (!read) {
                            result.set_value(boost::make_unexpected(read.error().code());
                            return;
                          }

                          result.set_value(std::move(read)->result());
                        });
                  });
            });
      });

  const auto value = result.get_promise().get();
  if (!value) {
    std::cerr << value.error().message() << std::endl;
    return false;
  }

   std::cout << *value << std::endl;
   return true;
}
```
### Hello World (Stackful Co-routines and Monadic) ###
```c++
bool HelloWorld(maidsafe::nfs::Storage& storage) {
  maidsafe::nfs::Future<boost::expected<std::string, std::error_code>> result;

  boost::asio::spawn([]{}, [&result](boost::asio::yield_context yield) {
    result.set_value(
        storage.OpenContainer(
            "example_container", maidsafe::nfs::ModifyContainerVersion::Create(), yield).bind(

                [&yield](maidsafe::nfs::Container container) {
                  return container.OpenBlob(
                      "example_blob", maidsafe::nfs::ModifyBlobVersion::Create(), yield);
                }

        ).bind([&yield](maidsafe::nfs::LocalBlob blob) {
          blob.Write(boost::asio::buffer("hello world"), []{});
          return blob.Commit(yield).bind(

              [&yield, &blob](maidsafe::nfs::BlobVersion) {
                std::string buffer;
                buffer.resize(blob.size());
                return blob.Read(boost::asio::buffer(&buffer[0], buffer.size()), yield).bind(

                    [&yield, &buffer](const std::size_t read_size) {
                      buffer.resize(read_size);
                      return buffer;
                    });
              });
        }));
    });

  const auto value = result.get();
  if (!value) {
    std::cerr << "Error: " << value.error().message() << std::endl;
    return false;
  }

  std::cout << *value << std::endl;
  return true;
}
```

## Posix Style API ##
All public functions in this API provide the strong exception guarantee. All public const methods are thread-safe.

### maidsafe::nfs::ContainerInfo ###
> maidsafe/nfs/container_info.h

- [x] Thread-safe Public Functions
- [x] Copyable
- [ ] Movable

References a Container stored on the network. Allows for quicker opening of child containers, than using `std::string` because the network locations are already known. Object is immutable.

```c++
class ContainerInfo {
    const std::string& key() const noexcept;
    bool Equal(const ContainerInfo& other) const noexcept;
};

bool operator==(const ContainerInfo&, const ContainerInfo&) noexcept;
bool operator!=(const ContainerInfo&, const ContainerInfo&) noexcept;
```
- **key()**
  - Returns the key associated with `this` ContainerInfo.
- **Equal(const ContainerInfo& other)**
  - Returns true if `other` is equivalent to `this` ContainerInfo.
- The non-member operator overloads call the corresponding Equal function.

### maidsafe::nfs::Blob ###
> maidsafe/nfs/blob.h

- [x] Thread-safe Public Functions
- [x] Copyable
- [ ] Movable

Represents a single stored Blob on the network. Can be given to any valid [`PosixContainer`](#maidsafenfsposixcontainer) so that the contents can be read - this object stores pointers to the data on the network for quicker access. Object is immutable.

> The network currently has no time server of its own, so the timestamps are from the clients. If a client has a misconfigured clock, the timestamps stored will also be incorrect.

```c++
class Blob {
    const BlobVersion& version() const noexcept;
    const std::string& key() const noexcept;
    std::uint64_t size() const noexcept;
    Clock::time_point creation_time() const;
    Clock::time_point modification_time() const;
    const std::string& user_meta_data() const noexcept;
    
    bool Equal(const Blob& other) const noexcept;
};

bool operator==(const Blob&, const blob&) noexcept;
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
- **Equal(const Blob& other)**
  - Returns true if `other` is equivalent to `this` Blob.
- The non-member operator overloads call the corresponding Equal function.

### maidsafe::nfs::Future&lt;T> ###
> maidsafe/nfs/future.h

- [x] Thread-safe Public Functions
- [ ] Copyable
- [x] Movable

Currently `maidsafe::nfs::Future` is a `std::future` object, but this may be changed to a non-allocating design. It is recommended that you use the typedef (`maidsafe::nfs::Future`) in case the implementation changes.

In the Posix API, the `Future` will only throw exceptions on non-network related errors (std::bad_alloc, std::bad_promise, etc.). Values and network related errors are returned in a `boost::expected` object.

```c++
template<typename T>
using Future = std::future<T>;
```

### maidsafe::nfs::PosixContainer ###
> maidsafe/nfs/posix_container.h

- [x] Thread-safe Public Functions
- [x] Copyable
- [x] Movable

> This object has a single shared_ptr, and is shallow-copied. This makes it extremely quick to copy.

An interface for the [`Container`](#container) abstraction listed above. The process for opening a container is still a work-in-progress, as it involves the launcher application being concurrently developed.

Parameters labeled as `AsyncResult<T>` affect the return type of the function, and valid values are:
- A callback in the form `void(boost::expected<T, std::error_code>)`; return type is void
- A boost::asio::yield_context object; return type is `boost::expected<T, std::error_code>`.
- A maidsafe::nfs::use_future; return type is `maidsafe::nfs::Future<boost::expected<T, std::error_code>>`.

```c++
class PosixContainer {
  // Child Container Operations
  unspecified ListChildContainers(
      AsyncResult<std::vector<ContainerInfo>> result, std::string prefix = std::string());
      
  unspecified    CreateChildContainer(const std::string& key, AsyncResult<PosixContainer> result);
  PosixContainer OpenChildContainer(const ContainerInfo& child);
  unspecified    OpenChildContainer(const std::string& key, AsyncResult<PosixContainer> result);

  unspecified DeleteChildContainer(const ContainerInfo& child, AsyncResult<void>);


  // Blob Operations
  unspecified ListBlobs(AsyncResult<std::vector<Blob>> result, std::string prefix = std::string());
  
  unspecified GetBlobHistory(const std::string& key, AsyncResult<std::vector<Blob>> result);
  
  LocalBlob   CreateLocalBlob() const;
  LocalBlob   OpenLocalBlob(const Blob& blob) const;
  unspecified OpenLocalBlob(const std::string& key, AsyncResult<LocalBlob> result);

  unspecified CopyBlob(const Blob& from, const std::string& to, AsyncResult<Blob> result);
  unspecified WriteBlob(LocalBlob& from, const std::string& to, AsyncResult<Blob> result);
  unspecified UpdateBlob(LocalBlob& from, const Blob& to, AsyncResult<Blob> result);
      
  unspecified DeleteBlob(const Blob& blob, AsyncResult<void>);
};
```
> A key can only store a Blob or a nested Container at a given point in time.

- **ListChildContainers(AsyncResult&lt;std::vector&lt;ContainerInfo>> result, std::string prefix)**
  - Request the list of nested child Containers.
  - `prefix` will filter the returned values - only child Containers whose key has the same prefix as `prefix` will be returned. The empty string indicates that all child Containers should be returned.
  - `result` is given handles to the child containers. The ordering in the vector is unspecified.
- **CreateChildContainer(const std::string& key, AsyncResult&lt;PosixContainer> result)**
  - Create a new child container at `key`.
  - Fails if `key` currently references a Blob or another child Container.
  - `result` is given the new child Container.
- **OpenChildContainer(const ContainerInfo& child)**
  - Open the container referenced by `child`.
- **OpenChildContainer(const std::string& key, AsyncResult&lt;PosixContainer> result)**
  - Make a request to open a container at `key`.
  - `result` is given the child Container.
- **DeleteChildContainer(const ContainerInfo& child, AsyncResult&lt;void>)**
  - Make a request to remove `child.key()` from the latest Container listings.
  - Fails if `child.key()` does not currently reference `child`.
- **ListBlobs(AsyncResult&lt;std::vector&lt;Blob>> result, std::string prefix)**
  - Request the list of Blobs.
  - `prefix` will filter the returned values - only Blobs whose key has the same prefix as `prefix` will be returned. The empty string indicates that all Blobs should be returned.
  - `result` is given handles to the Blob objects. The ordering is unspecified.
- **GetBlobHistory(const std::string& key, AsyncResult&lt;std::vector&lt;Blob>> result)**
  - Retrieve every Blob referenced by `key`, stopping at the creation of the first Blob or the end of the finite history stored by the network. 
  - `result` is given the history for `key`.
    - The front() of the std::vector will contain the newest Blob, while the back() of the vector will contain the oldest known Blob.
- **CreateLocalBlob()**
  - A LocalBlob is returned with `size() == 0` and `user_meta_data().empty()`.
- **OpenLocalBlob(const Blob& blob)**
  - Immediately returns a LocalBlob whose initial contents are identical to `blob`.
- **OpenLocalBlob(const std::string& key, AsyncResult&lt;LocalBlob> result)**
  - Make a request to open a Blob.
  - `result` is given a `LocalBlob` that has the contents and user meta data referenced by `key`.
- **CopyBlob(const Blob& from, std::string to, ModifyBlobVersion, AsyncResult&lt;Blob> result)**
  - Make a request to copy the contents and user meta data of `from` to a new key referenced by `to`.
  - Fails if `to` currently references a Blob or child Container.
  - `result` is given a handle to the Blob that was stored on the network.
- **WriteBlob(LocalBlob& from, std::string to, AsyncResult&lt;Blob> result)**
  - Make a request to write the contents and user meta data of `from` to a new key referenced by `to`.
  - Fails if `to` currently references a Blob or child Container.
  - Do not invoke if Read, Write, or Truncate calls have not completed on `from`.
  - `result` is given a handle to the Blob that was stored on the network.
- **UpdateBlob(LocalBlob& from, const Blob& to, AsyncResult&lt;Blob> result)**
  - Make a request to replace `to` with the contents and user meta data of `from`.
  - Do not invoke if Read, Write, or Truncate calls have not completed on `from`.
  - Fails if `to.key()` does not currently reference `to`.
  - `result` is given a handle to the Blob that was stred on the network.
- **DeleteBlob(const Blob& blob, AsyncResult&lt;void>)**
  - Make a request to remove `blob.key()` from the latest Container listings.
  - Fails if `blob.key()` does not currently reference `blob`.

### maidsafe::nfs::LocalBlob ###
> maidsafe/nfs/local_blob.h

- [ ] Thread-safe Public Functions
- [ ] Copyable
- [x] Movable

Upon initial creation, `LocalBlob` inherits the data and user meta data from the Blob that was used to open it. If LocalBlob was created without a Blob object (`PosixContainer::CreateLocalBlob()`), then the data and user meta data are completely empty. The `offset()` is always initially set to zero.

Only a single `Read`, `Write`, or `Truncate` operation can take place at a given time. The `AsyncResult<T>` must be signalled before _any_ of these three functions can be invoked. A LocalBlob is stored by calling `PosixContainer::Write`, which can be invoked on any `PosixContainer` object, even if the LocalBlob was not opened on that PosixContainer.

Parameters labeled as `AsyncResult<T>` affect the return type of the function, and valid values are:
- A callback in the form `void(boost::expected<T, std::error_code>)`; return type is void
- A boost::asio::yield_context object; return type is `boost::expected<T, std::error_code>`.
- A maidsafe::nfs::use_future; return type is `maidsafe::nfs::Future<boost::expected<T, std::error_code>>`

```C++
class LocalBlob {
  const std::string& user_meta_data() const noexcept;
  Expected<void> set_user_meta_data(std::string);
  
  std::uint64_t size() const;
  std::uint64_t offset() const;
  void set_offset(std::uint64_t);

  unspecified Read(asio::buffer, AsyncResult<std::uint64_t>);
  unspecified Write(asio::buffer, AsyncResult<void>);
  void Truncate(std::uint64_t, AsyncResult<void>);
};
```
- **user_meta_data()**
  - Return the current user meta data associated with the LocalBlob.
- **set_user_meta_data(std::string)**
  - Update the `user_meta_data()`. Size must be less than 64kB (CommonErrors::cannot_exceed_limit is returned in expected if this is not held).
- **offset()**
  - Returns the offset that will be used by the next Read, Write, or Truncate call.
  - Cannot be invoked if a `Read`, `Write`, or `Truncate` call has not-yet completed.
- **set_offset(std::uint64_t)**
  - Change the value returned by `offset()`.
  - Cannot be invoked if a `Read`, `Write`, or `Truncate` call has not-yet completed.
- **Read(asio::buffer, AsyncResult<std::uint64_t>)**
  - Read from the `LocalBlob` starting at `offset()` into the provided buffer. The buffer must remain valid until AsyncResult returns.
  - `offset()` is immediately updated to `min(size() - offset(), offset() + buffer::size())`
  - AsyncResult is given the number of bytes actually read.
  - Cannot be invoked if a `Read`, `Write`, or `Truncate` call has not-yet completed.
- **Write(boost::asio::buffer, AsyncResult<void>)**
  - Write to the `LocalBlob` starting at `offset()` from the provided buffer. The buffer must remain valid until AsyncResult returns.
  - `offset()` is immediately updated to `offset() + buffer::size()`
  - Cannot be invoked if a `Read`, `Write`, or `Truncate` call has not-yet completed.
- **Truncate(std::uint64_t size, AsyncResult<void>)**
  - Change the size of the `LocalBlob` to `size` bytes.
  - `offset()` is immediately updated to `size`
  - Cannot be invoked if a `Read`, `Write`, or `Truncate` call has not-yet completed.
