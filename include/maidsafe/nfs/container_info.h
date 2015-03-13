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
#ifndef MAIDSAFE_NFS_CONTAINER_INFO_H_
#define MAIDSAFE_NFS_CONTAINER_INFO_H_

#include <cstdint>

#include "maidsafe/common/config.h"
#include "maidsafe/nfs/detail/container_info.h"
#include "maidsafe/nfs/detail/container_key.h"

namespace maidsafe {
namespace nfs {
class ContainerInfo {
 public:
  ContainerInfo(detail::ContainerKey key, detail::ContainerInfo info) MAIDSAFE_NOEXCEPT
    : key_(std::move(key)),
      detail_info_(std::move(info)) {
  }

  // No move construction/assignment. This would create null-pointers in
  // detail::ContainerKey and detail::Info. Do not give clients the bullet.
  ContainerInfo(const ContainerInfo&) = default;
  ContainerInfo& operator=(const ContainerInfo&) = default;

  void swap(ContainerInfo& other) MAIDSAFE_NOEXCEPT {
    using std::swap;
    swap(key_, other.key_);
    swap(detail_info_, other.detail_info_);
  }

  const std::string& key() const { return key_.value(); }

  // Intended for internal usage only
  explicit operator const detail::ContainerInfo&() const { return detail_info_; }

 private:
  detail::ContainerKey key_;
  detail::ContainerInfo detail_info_;
};

inline void swap(ContainerInfo& lhs, ContainerInfo& rhs) MAIDSAFE_NOEXCEPT {
  lhs.swap(rhs);
}
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_CONTAINER_INFO_H_
