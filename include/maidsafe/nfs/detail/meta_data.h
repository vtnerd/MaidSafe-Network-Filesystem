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
#ifndef MAIDSAFE_NFS_DETAIL_META_DATA_H_
#define MAIDSAFE_NFS_DETAIL_META_DATA_H_

#include <cstdint>
#include <string>
#include <utility>

#include "cereal/types/chrono.hpp"

#include "maidsafe/common/clock.h"
#include "maidsafe/common/config.h"
#include "maidsafe/common/hash/hash_chrono.h"

#include "maidsafe/nfs/detail/user_meta_data.h"
#include "maidsafe/nfs/expected.h"

namespace cereal { class access; }

namespace maidsafe {
namespace nfs {
namespace detail {
class MetaData {
 public:
  friend class cereal::access;

  MetaData();
  MetaData(UserMetaData user_meta_data);
  MetaData(UserMetaData user_meta_data, Clock::time_point creation_time);

  MetaData(const MetaData&) = default;
  MetaData(MetaData&& other);

  MetaData& operator=(MetaData other) MAIDSAFE_NOEXCEPT {
    swap(other);
    return *this;
  }

  void swap(MetaData& other) MAIDSAFE_NOEXCEPT;

  template<typename HashAlgorithm>
  void HashAppend(HashAlgorithm& hash) const {
    hash(user_meta_data(), creation_time(), modification_time());
  }

  const UserMetaData& user_meta_data() const MAIDSAFE_NOEXCEPT { return user_meta_data_; }
  Expected<void> set_user_meta_data(std::string user);

  Clock::time_point creation_time() const { return creation_time_; }
  Clock::time_point modification_time() const { return modification_time_; }

  void UpdateModificationTime() MAIDSAFE_NOEXCEPT;

 private:
  template<typename Archive>
  Archive& serialize(Archive& archive) {
    return archive(user_meta_data_, creation_time_, modification_time_);
  }

 private:
  UserMetaData user_meta_data_;
  // Creation time is the timestamp of when a key was
  // first associated with that type.
  Clock::time_point creation_time_;
  Clock::time_point modification_time_;
};

inline void swap(MetaData& lhs, MetaData& rhs) MAIDSAFE_NOEXCEPT {
  lhs.swap(rhs);
}
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_META_DATA_H_
