# Maidsafe NFS API Design #
![Maidsafe NFS API Design](https://rawgit.com/vtnerd/MaidSafe-Network-Filesystem/api_docs/docs/design.svg)

## NetworkInterface ##
> * Non-copyable

An interface that provides the ability to store/retrieve single chunks of data, and the ability to store/retrieve structured data version objects. Currently there are two implementations of this interface, one that uses the SAFE network, and one that uses the local filesystem (testing only).

## Account ##
> * Non-copyable

The `Account` class automatically fetches the root `Container`, which only stores information about nested top-level `Container` instances. The root `Container` is also versioned.

## Container ##
> * Non-copyable

`Container` is just a wrapper around a `shared_ptr<detail::Container>`, and mostly forwards its calls to that class. Separating them allows `LocalBlob` to call public functions in `detail::Container` for storage, which should not be exposed to clients.

## detail::Container ##
> * Non-copyable
> * shared_ptr instance required for asynchronous operations

`detail::Container` fetches/stores versions of a container (`ContainerInstance`), and caches the information for quicker access. Automatically prunes cached information when the network prunes old versions of containers.

## detail::ContainerInstance ##
`detail::ContainerInstance` stores information about a single container version that is stored on the SAFE network.

## LocalBlob ##
> * Non-copyable

Client interface for writing/reading contents of a blob. Creates new copy of `detail::LocalBlob` when user requests a commit, and sends old to `detail::Container` for serialising.

## detail::LocalBlob ##
> * Non-copyable
> * shared_ptr instance required for asynchronous operations

Every call to `LocalBlob::Commit` will send an object of this type to `detail::Container` for storage. The separation allows LocalBlob to take a "snapshot" of the current attempt at versioning the modified `Blob`. If the storage is successful, the associated `detail::Container` object will be updated with a new `detail::Blob` containing the data map, metadata, and buffer that was owner by the `detail::LocalBlob` object.

## detail::Blob ##
Represents an immutable `Blob` stored on the network. Automatically freed when a `detail::ContainerInstance` no longer references it (and whose delete function removes the cache entry in `unordered_map<BlobVersion, weak_ptr<detail::Blob>>`).

## detail::BlobBuffer ##
> * Non-copyable

Caches data chunks for related blob objects. Automatically freed when all related blobs are destroyed.
