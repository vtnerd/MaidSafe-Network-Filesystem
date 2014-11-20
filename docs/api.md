# Maidsafe App API #
The Maidsafe API strives to be easy to use, consistent, and flexible for advanced users requiring performance. Some of the design choices may seem to oppose the ease of use goal, so the rational behind their use is discussed.

## Storage Abstractions ##
### File ###
Data on the SAFE network is stored in Files. A File can contain text or binary data, and the SAFE network has no upward limit on size. However, local system contraints may apply to maximum File size. Every File also has a name, which is represented by a string of UTF-8 characters.

### Directory ###
A directory is a virtual object containing other Directories or Files, and must contain at least one Directory or File (a Directory must have 1 File or Directory, and can have 0-∞ Files and 0-∞ Directories). Every Directory has a name, which is represented by a string of UTF-8 characters.

### Path ###
The SAFE network uses the same concept as Posix filesystems to specify directories and files. A file or directory in the SAFE network is referenced by a string that corresponds to a virtual location on the network. Directories are separated `/` and the group of characters after the last `/` represents the filename. For example: `/file1` references a file named `file1` in the top-level (unnamed root) directory, whereas `/directory1/file1` references a file named `file1` in the directory `directory1`. Despite having the same name, these two files are **not** the same because they are in different directories.

Every SAFE identity has its own unique unnamed root. For example `/directory/file1` in the identity representing `user1` is **not** the same file as `/directory/file1` in the identity representing `user2`. This allows users to store data securely and separately from other users on the network. Developers *should* be aware that different SAFE Apps using the same identity can see the files stored by other SAFE Apps. Until otherwise stated, if a user wants to keep files hidden from a specific SAFE App, a different identity will have to be used.

#### Network Path Auto-Creation and Deletion ####
The SAFE API has no functions for creating directories, instead the directories are created as-needed in an `Open` or `Put` call. Directories are also automatically deleted when they contain no files or folders. The Delete function in the API will work with files or directories; in the latter case all child directories and files are removed from the directory and then the directory is removed.

## Versioning ##
Every file and directory in the SAFE network is stored as a revision, so that conflicts between SAFE Apps (or multiple instances of the same SAFE App) can be detected. Every SAFE storage operation that modifies data (file or directory contents) requires a Version object, and the operation will fail if the Version object represents an outdated Version from the one currently on the network. SAFE App developers will be responsible for handling version conflicts, no generic solution exists.

Every operation (both modification and read-only operations), will return a Version object which will be the most-up-to-date version known to the SAFE API.

## Futures ##


## Expected ##

## Operation ##

## Basic Usage ##

## Classes ##
### maidsafe::nfs::Error ###
### maidsafe::nfs::Version ###


### maidsafe::nfs::Future<T> ###


### maidsafe::nfs::Expected<T> ###
Expect object T, but on failure maidsafe::nfs::Error is provided instead.

### maidsafe::nfs::Operation<T> ###
All requests to the SAFE network, if successful, will yield an Operation<T> object. Every Operation object will have a version, and a result value (which can be void).

```c++
template<typename T = void>
class Operation {
  const Version& version() const;
  const T& result() const; // if T != void
};
```

### maidsafe::nfs::FutureExpectedOperation<T> ###
This is an alias for `maidsafe::nfs::Future<maidsafe::nfs::Expected<maidsafe::nfs::Operation<T>>>`.

### maidsafe::nfs::Storage ###

### maidsafe::nfs::File ###