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
#ifndef MAIDSAFE_NFS_DETAIL_CONTAINER_INFO_H_
#define MAIDSAFE_NFS_DETAIL_CONTAINER_INFO_H_

#include <utility>

#include "boost/flyweight.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/hash/algorithms/siphash.h"
#include "maidsafe/common/hash/wrappers/seeded_hash.h"
#include "maidsafe/common/types.h"
#include "maidsafe/nfs/detail/container_id.h"

namespace cereal { class access; }

namespace maidsafe {
namespace nfs {
namespace detail {
class ContainerInfo {
 public:
  friend class cereal::access;

  ContainerInfo();

  ContainerInfo(const ContainerInfo&) = default;
  ContainerInfo(ContainerInfo&& other) MAIDSAFE_NOEXCEPT
    : key_(std::move(other.key_)) {
  }

  ~ContainerInfo();

  ContainerInfo& operator=(const ContainerInfo&) = default;
  ContainerInfo& operator=(ContainerInfo&& other) MAIDSAFE_NOEXCEPT {
    key_ = std::move(other.key_);
    return *this;
  }

  void swap(ContainerInfo& other) MAIDSAFE_NOEXCEPT {
    using std::swap;
    swap(key_, other.key_);
  }

  const Identity& key() const MAIDSAFE_NOEXCEPT { return key_.get(); }
  ContainerId GetId() const;

 private:
  template<typename Archive>
  Archive& serialize(Archive& ar);

 private:
  boost::flyweight<
    Identity, boost::flyweights::hashed_factory<SeededHash<SipHash, Identity>>> key_;
};

inline void swap(ContainerInfo& lhs, ContainerInfo& rhs) MAIDSAFE_NOEXCEPT {
  lhs.swap(rhs);
}
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_CONTAINER_INFO_H_
