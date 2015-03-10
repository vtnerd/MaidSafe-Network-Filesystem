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
#ifndef MAIDSAFE_NFS_DETAIL_USER_META_DATA_H_
#define MAIDSAFE_NFS_DETAIL_USER_META_DATA_H_

#include <cstdint>
#include <string>
#include <utility>

#include "cereal/types/string.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/hash/hash_string.h"
#include "maidsafe/nfs/expected.h"

namespace cereal { class access; }

namespace maidsafe {
namespace nfs {
namespace detail {
class UserMetaData {
 public:
  friend class cereal::access;

  UserMetaData();

  UserMetaData(const UserMetaData&) = default;
  UserMetaData(UserMetaData&& other) MAIDSAFE_NOEXCEPT
    : value_(std::move(other.value_)) {
  }

  UserMetaData& operator=(UserMetaData other) MAIDSAFE_NOEXCEPT {
    swap(other);
    return *this;
  }

  void swap(UserMetaData& other) MAIDSAFE_NOEXCEPT {
    using std::swap;
    swap(value_, other.value_);
  }

  template<typename HashAlgorithm>
  void HashAppend(HashAlgorithm& hash) const { hash(value()); }

  const std::string& value() const MAIDSAFE_NOEXCEPT { return value_; }
  Expected<void> set_value(std::string value);

 private:
  template<typename Archive>
  Archive& serialize(Archive& archive) { return archive(value_); }

 private:
  std::string value_;
};

inline void swap(UserMetaData& lhs, UserMetaData& rhs) MAIDSAFE_NOEXCEPT {
  lhs.swap(rhs);
}
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_USER_META_DATA_H_
