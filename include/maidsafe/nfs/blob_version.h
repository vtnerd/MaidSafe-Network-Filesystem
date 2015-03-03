/*  Copyright 2014 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */
#ifndef MAIDSAFE_NFS_BLOB_VERSION_H_
#define MAIDSAFE_NFS_BLOB_VERSION_H_

#include "maidsafe/common/hash/algorithms/sha.h"
#include "maidsafe/common/hash/hash_array.h"

namespace maidsafe {
namespace nfs {
class BlobVersion {
 public:
  BlobVersion();
  explicit BlobVersion(const maidsafe::SHA512::Digest& version);

  BlobVersion(const BlobVersion&) = default;
  BlobVersion& operator=(const BlobVersion&) = default;

  template<typename HashAlgorithm>
  void HashAppend(HashAlgorithm& hash) const { return hash(version_); }
  bool Equal(const BlobVersion& other) const;

 private:
  maidsafe::SHA512::Digest version_;
};

inline bool operator==(const BlobVersion& lhs, const BlobVersion& rhs) {
  return lhs.Equal(rhs);
}

inline bool operator!=(const BlobVersion& lhs, const BlobVersion& rhs) {
  return !lhs.Equal(rhs);
}
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_BLOB_VERSION_H_
