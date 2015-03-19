/*  Copyright 2015 MaidSafe.net limited

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
#ifndef MAIDSAFE_NFS_DETAIL_BLOB_H_
#define MAIDSAFE_NFS_DETAIL_BLOB_H_

#include <memory>
#include <utility>

#include "maidsafe/common/clock.h"
#include "maidsafe/common/config.h"
#include "maidsafe/encrypt/data_map.h"
#include "maidsafe/nfs/detail/blob_contents.h"
#include "maidsafe/nfs/detail/detail_fwd.h"
#include "maidsafe/nfs/detail/network_data.h"

namespace cereal { class access; }

namespace maidsafe {
namespace nfs {
namespace detail {
class Blob {
 public:
  friend class cereal::access;

  Blob();

  // For new blob, shared_ptr can be nullptr
  Blob(
      const std::shared_ptr<Network>& network,
      UserMetaData user,
      encrypt::DataMap data_map,
      std::shared_ptr<NetworkData::Buffer> buffer);

  // For updating blob, shared_ptr can be nullptr
  Blob(
      const std::shared_ptr<Network>& network,
      Clock::time_point creation_time,
      UserMetaData user,
      encrypt::DataMap data_map,
      std::shared_ptr<NetworkData::Buffer> buffer);

  Blob(const Blob&) = default;
  Blob(Blob&& other) MAIDSAFE_NOEXCEPT
    : contents_(std::move(other.contents_)) {
  }

  Blob& operator=(const Blob&) = default;
  Blob& operator=(Blob&& other) MAIDSAFE_NOEXCEPT {
    contents_ = std::move(other.contents_);
    return *this;
  }

  const MetaData& meta_data() const MAIDSAFE_NOEXCEPT { return contents().meta_data(); }
  const encrypt::DataMap& data_map() const MAIDSAFE_NOEXCEPT { return contents().data_map(); }

  std::shared_ptr<NetworkData::Buffer> GetBuffer(const std::weak_ptr<Network>& network) const {
    return contents().GetBuffer(network);
  }

  bool Equal(const Blob& other) const MAIDSAFE_NOEXCEPT {
    return contents_ == other.contents_ || contents() == other.contents();
  }

 private:
  const BlobContents& contents() const MAIDSAFE_NOEXCEPT {
    assert(contents_ != nullptr);
    return *contents_;
  }

  NfsInputArchive& load(NfsInputArchive& archive);

  template<typename Archive>
  Archive& save(Archive& archive) const {
    // Blob contents contain timestamps, so its extremely unlikely that a single
    // ContainerInstance will contain duplicates. Save the 4 bytes.
    return archive(contents());
  }

 private:
  std::shared_ptr<const BlobContents> contents_;
};

inline bool operator==(const Blob& lhs, const Blob& rhs) MAIDSAFE_NOEXCEPT {
   return lhs.Equal(rhs);
}

inline bool operator!=(const Blob& lhs, const Blob& rhs) MAIDSAFE_NOEXCEPT {
  return !lhs.Equal(rhs);
}
}  // detail
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_BLOB_H_
