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
#include "maidsafe/nfs/blob_version.h"
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
      detail_blob_(std::move(blob)) {
  }

  Blob(const Blob&) = default;
  Blob(Blob&& other) MAIDSAFE_NOEXCEPT
    : key_(std::move(other.key_)),
      detail_blob_(std::move(other.detail_blob_)) {
  }

  Blob& operator=(const Blob&) = default;
  Blob& operator=(Blob&& other) MAIDSAFE_NOEXCEPT {
    key_ = std::move(other.key_);
    detail_blob_ = std::move(other.detail_blob_);
    return *this;
  }

  void swap(Blob& other) MAIDSAFE_NOEXCEPT {
    using std::swap;
    swap(key_, other.key_);
    swap(detail_blob_, other.detail_blob_);
  }

  const std::string& key() const MAIDSAFE_NOEXCEPT { return key_.value(); }
  const BlobVersion& version() const MAIDSAFE_NOEXCEPT { return detail_blob_.version(); }
  std::uint64_t size() const MAIDSAFE_NOEXCEPT { return detail_blob_.data_map().size(); }

  Clock::time_point creation_time() const { return detail_blob_.meta_data().creation_time(); }
  Clock::time_point modification_time() const {
    return detail_blob_.meta_data().modification_time();
  }

  Expected<std::string> data() const;
  const std::string& user_meta_data() const MAIDSAFE_NOEXCEPT {
    return detail_blob_.meta_data().user_meta_data().value();
  }

  bool Equal(const Blob& other) const MAIDSAFE_NOEXCEPT {
    return key_ == other.key_ && detail_blob_ == other.detail_blob_;
  }

  // Intended for internal usage only
  explicit operator const detail::Blob&() const { return detail_blob_; }

 private:
  detail::ContainerKey key_;
  detail::Blob detail_blob_;
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
