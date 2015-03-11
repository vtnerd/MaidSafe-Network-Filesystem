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

#include <memory>
#include <utility>

#include "cereal/types/memory.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/types.h"
#include "maidsafe/nfs/detail/container_id.h"
#include "maidsafe/nfs/detail/detail_fwd.h"

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

  ContainerInfo& operator=(const ContainerInfo&) = default;
  ContainerInfo& operator=(ContainerInfo&& other) MAIDSAFE_NOEXCEPT {
    key_ = std::move(other.key_);
    return *this;
  }

  void swap(ContainerInfo& other) MAIDSAFE_NOEXCEPT {
    using std::swap;
    swap(key_, other.key_);
  }

  ContainerId GetId() const;
  const Identity& key() const MAIDSAFE_NOEXCEPT {
    assert(key_ != nullptr);
    return *key_;
  }

 private:
  NfsInputArchive& load(NfsInputArchive& archive);

  template<typename Archive>
  Archive& save(Archive& archive) const {
    // Serialise shared_ptr. This will allow for multiple keys to be associated
    // with a single pointer in the future. This costs 4 bytes, but nested
    // containers are expected to be rare.
    return archive(key_);
  }

 private:
  std::shared_ptr<const Identity> key_;
};

inline void swap(ContainerInfo& lhs, ContainerInfo& rhs) MAIDSAFE_NOEXCEPT {
  lhs.swap(rhs);
}
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_CONTAINER_INFO_H_
