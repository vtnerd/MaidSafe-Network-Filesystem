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
#include "maidsafe/nfs/detail/pending_blob.h"

#include <utility>

namespace maidsafe {
namespace nfs {
namespace detail {

PendingBlob::PendingBlob(
    UserMetaData user_meta_data,
    encrypt::DataMap data_map,
    std::shared_ptr<NetworkData::Buffer> buffer)
  : user_meta_data_(std::move(user_meta_data)),
    data_map_(std::move(data_map)),
    buffer_(std::move(buffer)) {
}

PendingBlob::PendingBlob(PendingBlob&& other)
  : user_meta_data_(std::move(other.user_meta_data_)),
    data_map_(std::move(other.data_map_)),
    buffer_(std::move(other.buffer_)) {
}

void PendingBlob::swap(PendingBlob& other) MAIDSAFE_NOEXCEPT {
  using std::swap;
  swap(user_meta_data_, other.user_meta_data_);
  swap(data_map_, other.data_map_);
  swap(buffer_, other.buffer_);
}

}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
