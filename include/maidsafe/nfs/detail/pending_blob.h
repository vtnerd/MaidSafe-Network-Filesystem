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
#ifndef MAIDSAFE_NFS_DETAIL_PENDING_BLOB_H_
#define MAIDSAFE_NFS_DETAIL_PENDING_BLOB_H_

#include <memory>

#include "maidsafe/common/config.h"
#include "maidsafe/encrypt/data_map.h"
#include "maidsafe/nfs/detail/detail_fwd.h"
#include "maidsafe/nfs/detail/network_data.h"
#include "maidsafe/nfs/detail/user_meta_data.h"

namespace maidsafe {
namespace nfs {
namespace detail {
class PendingBlob {
 public:
  PendingBlob(
      UserMetaData user_meta_data,
      encrypt::DataMap data_map,
      std::shared_ptr<NetworkData::Buffer> buffer);

  PendingBlob(const PendingBlob&) = default;
  PendingBlob(PendingBlob&& other);

  PendingBlob& operator=(PendingBlob other) MAIDSAFE_NOEXCEPT {
    swap(other);
    return *this;
  }

  void swap(PendingBlob& other) MAIDSAFE_NOEXCEPT;

  const UserMetaData& user_meta_data() const { return user_meta_data_; }
  const encrypt::DataMap& data_map() const { return data_map_; }
  const std::shared_ptr<NetworkData::Buffer>& buffer() const { return buffer_; }

 private:
  UserMetaData user_meta_data_;
  encrypt::DataMap data_map_;
  std::shared_ptr<detail::NetworkData::Buffer> buffer_;
};

inline void swap(PendingBlob& lhs, PendingBlob& rhs) MAIDSAFE_NOEXCEPT {
  lhs.swap(rhs);
}
}  // detail
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_PENDING_BLOB_H_
