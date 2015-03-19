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
#ifndef MAIDSAFE_NFS_BLOB_H_
#define MAIDSAFE_NFS_BLOB_H_

#include <cstdint>

#include "maidsafe/common/clock.h"
#include "maidsafe/common/config.h"
#include "maidsafe/nfs/detail/blob.h"
#include "maidsafe/nfs/detail/container_key.h"
#include "maidsafe/nfs/detail/meta_data.h"
#include "maidsafe/nfs/expected.h"

namespace maidsafe {
namespace nfs {
class Blob {
 public:
  Blob(detail::ContainerKey key, detail::Blob blob) MAIDSAFE_NOEXCEPT
    : key_(std::move(key)),
      blob_(std::move(blob)) {
  }

  // No move construction/assignment. This would create null-pointers in
  // detail::ContainerKey and detail::Blob. Do not give clients the bullet.
  Blob(const Blob&) = default;
  Blob& operator=(const Blob&) = default;

  void swap(Blob& other) MAIDSAFE_NOEXCEPT {
    using std::swap;
    swap(key_, other.key_);
    swap(blob_, other.blob_);
  }

  const std::string& key() const MAIDSAFE_NOEXCEPT { return key_.value(); }
  std::uint64_t size() const MAIDSAFE_NOEXCEPT { return blob_.data_map().size(); }

  Clock::time_point creation_time() const { return blob_.meta_data().creation_time(); }
  Clock::time_point modification_time() const {
    return blob_.meta_data().modification_time();
  }

  const std::string& user_meta_data() const MAIDSAFE_NOEXCEPT {
    return blob_.meta_data().user_meta_data().value();
  }

  bool Equal(const Blob& other) const MAIDSAFE_NOEXCEPT {
    return key_ == other.key_ && blob_ == other.blob_;
  }

  // Not for client usage, but no harm leaving public exposure
  struct Detail {
    static const detail::ContainerKey& key(const Blob& blob) { return blob.key_; }
    static const detail::Blob& blob(const Blob& blob) { return blob.blob_; }
  };

 private:
  detail::ContainerKey key_;
  detail::Blob blob_;
};

inline void swap(Blob& lhs, Blob& rhs) MAIDSAFE_NOEXCEPT {
  lhs.swap(rhs);
}

inline bool operator==(const Blob& lhs, const Blob& rhs) MAIDSAFE_NOEXCEPT {
   return lhs.Equal(rhs);
}

inline bool operator!=(const Blob& lhs, const Blob& rhs) MAIDSAFE_NOEXCEPT {
  return !lhs.Equal(rhs);
}
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_BLOB_H_
