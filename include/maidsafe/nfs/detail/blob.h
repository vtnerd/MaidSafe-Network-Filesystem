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
#ifndef MAIDSAFE_NFS_DETAIL_BLOB_H_
#define MAIDSAFE_NFS_DETAIL_BLOB_H_

#include <cstdint>
#include <memory>
#include <mutex>

#include "boost/flyweight.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/hash/algorithms/siphash.h"
#include "maidsafe/common/hash/wrappers/seeded_hash.h"
#include "maidsafe/encrypt/data_map.h"
#include "maidsafe/nfs/blob_version.h"
#include "maidsafe/nfs/detail/detail_fwd.h"
#include "maidsafe/nfs/detail/meta_data.h"
#include "maidsafe/nfs/detail/network_data.h"

namespace cereal { class access; }

namespace maidsafe {
namespace nfs {
namespace detail {
class Blob {
 public:
  friend class cereal::access;

  Blob();
  Blob(const detail::PendingBlob& pending_blob);
  Blob(const detail::PendingBlob& pending_blob, Clock::time_point creation_time);

  Blob(const Blob&) = default;
  Blob(Blob&& other) MAIDSAFE_NOEXCEPT
    : contents_(std::move(other.contents_)) {
  }

  ~Blob();

  Blob& operator=(const Blob&) = default;
  Blob& operator=(Blob&& other) MAIDSAFE_NOEXCEPT {
    contents_ = std::move(other.contents_);
    return *this;
  }

  const MetaData& meta_data() const MAIDSAFE_NOEXCEPT { return contents_.get().meta_data(); }
  const encrypt::DataMap& data_map() const MAIDSAFE_NOEXCEPT { return contents_.get().data_map(); }
  const BlobVersion& version() const MAIDSAFE_NOEXCEPT { return contents_.get().version(); }

  std::shared_ptr<NetworkData::Buffer> GetBuffer(const std::weak_ptr<Network>& network) const {
    return contents_.get().GetBuffer(network);
  }

  /* The flyweight was done on an inner class so that the templated
     static factory methods of flyweight would be guaranteed to be in the TU
     for maidsafe binaries, and never in the client TU. */
  struct Contents {
   public:
    Contents();
    Contents(const PendingBlob& pending_blob);
    Contents(const PendingBlob& pending_blob, Clock::time_point creation_time);

    Contents(Contents&& other)
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
      return hash(version());
    }

    bool Equal(const Contents& other) const {
      return version() == other.version();
    }

    const BlobVersion& version() const MAIDSAFE_NOEXCEPT { return version_; }
    const MetaData& meta_data() const MAIDSAFE_NOEXCEPT { return meta_data_; }
    const encrypt::DataMap& data_map() const MAIDSAFE_NOEXCEPT { return data_map_; }

    std::shared_ptr<NetworkData::Buffer> GetBuffer(const std::weak_ptr<Network>& network) const;

   private:
    Contents(const Contents&) = delete;

    Contents& operator=(const Contents&) = delete;
    Contents& operator=(Contents&&) = delete;

   private:
    BlobVersion version_;  // unique SHA-512 id
    MetaData meta_data_;
    encrypt::DataMap data_map_;
    mutable std::weak_ptr<NetworkData::Buffer> buffer_;
    mutable std::mutex buffer_mutex_;
  };

  bool Equal(const Blob& other) const {
    return contents_ == other.contents_;
  }

 private:
  template<typename Archive>
  Archive& serialize(Archive& archive);

 private:
  boost::flyweight<
    Contents, boost::flyweights::hashed_factory<SeededHash<SipHash, Contents>>> contents_;
};

inline bool operator==(const Blob::Contents& lhs, const Blob::Contents& rhs) {
  return lhs.Equal(rhs);
}

inline bool operator!=(const Blob::Contents& lhs, const Blob::Contents& rhs) {
  return !lhs.Equal(rhs);
}

inline bool operator==(const Blob& lhs, const Blob& rhs) {
   return lhs.Equal(rhs);
}

inline bool operator!=(const Blob& lhs, const Blob& rhs) {
  return !lhs.Equal(rhs);
}
}  // detail
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_BLOB_H_
