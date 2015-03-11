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
#ifndef MAIDSAFE_NFS_DETAIL_BLOB_CONTENTS_H_
#define MAIDSAFE_NFS_DETAIL_BLOB_CONTENTS_H_

#include <cstdint>
#include <memory>
#include <mutex>

#include "maidsafe/common/config.h"
#include "maidsafe/encrypt/data_map.h"
#include "maidsafe/nfs/blob_version.h"
#include "maidsafe/nfs/detail/detail_fwd.h"
#include "maidsafe/nfs/detail/meta_data.h"
#include "maidsafe/nfs/detail/network_data.h"

namespace cereal { class access; }

namespace maidsafe {
namespace nfs {
namespace detail {
/* Blob has a shared_ptr<const BlobContents>. BlobContents is not an inner
   type so the Network class can use a forward declare. */
class BlobContents {
 public:
  friend class cereal::access;

  BlobContents();
  BlobContents(const PendingBlob& pending_blob);
  BlobContents(const PendingBlob& pending_blob, Clock::time_point creation_time);

  BlobContents(BlobContents&& other)
    : version_(std::move(other.version_)),
      meta_data_(std::move(other.meta_data_)),
      data_map_(std::move(other.data_map_)),
      buffer_(std::move(other.buffer_)),
      buffer_mutex_() {
  }

  // Re-calculates version, clears buffer
  void Refresh();

  template<typename Archive>
  Archive& load(Archive& ar, const std::uint32_t /*version*/) {
    ar(meta_data_, data_map_);
    Refresh();
    return ar;
  }

  template<typename Archive>
  Archive& save(Archive& archive, const std::uint32_t /*version*/) const {
    return archive(meta_data_, data_map_);
  }

  template<typename HashAlgorithm>
  void HashAppend(HashAlgorithm& hash) const {
    hash(version());
  }

  bool Equal(const BlobContents& other) const {
    return version() == other.version();
  }

  const BlobVersion& version() const MAIDSAFE_NOEXCEPT { return version_; }
  const MetaData& meta_data() const MAIDSAFE_NOEXCEPT { return meta_data_; }
  const encrypt::DataMap& data_map() const MAIDSAFE_NOEXCEPT { return data_map_; }

  std::shared_ptr<NetworkData::Buffer> GetBuffer(const std::weak_ptr<Network>& network) const;

 private:
  BlobContents(const BlobContents&) = delete;

  BlobContents& operator=(const BlobContents&) = delete;
  BlobContents& operator=(BlobContents&&) = delete;

 private:
  BlobVersion version_;  // unique SHA-512 id
  MetaData meta_data_;
  encrypt::DataMap data_map_;
  mutable std::weak_ptr<NetworkData::Buffer> buffer_;
  mutable std::mutex buffer_mutex_;
};

inline bool operator==(const BlobContents& lhs, const BlobContents& rhs) {
  return lhs.Equal(rhs);
}

inline bool operator!=(const BlobContents& lhs, const BlobContents& rhs) {
  return !lhs.Equal(rhs);
}
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_BLOB_CONTENTS_H_
