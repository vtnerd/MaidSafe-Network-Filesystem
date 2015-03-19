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
#include "maidsafe/nfs/detail/blob.h"

#include "maidsafe/common/hash/algorithms/sha.h"
#include "maidsafe/common/hash/hash_numeric.h"
#include "maidsafe/common/hash/wrappers/unseeded_hash.h"

namespace maidsafe {
namespace nfs {
namespace detail {
BlobContents::BlobContents()
  : buffer_(),
    meta_data_(),
    data_map_(),
    version_(),
    buffer_mutex_() {
  Refresh();
}

BlobContents::BlobContents(
    UserMetaData user,
    encrypt::DataMap data_map,
    std::shared_ptr<NetworkData::Buffer> buffer)
  : buffer_(),
    meta_data_(std::move(user)),
    data_map_(std::move(data_map)),
    version_(),
    buffer_mutex_() {
  // Do not delegate to other constructor, we need both metadata timestamps to be equal.
  Refresh();
  buffer_ = std::move(buffer);
}

BlobContents::BlobContents(
    Clock::time_point creation_time,
    UserMetaData user,
    encrypt::DataMap data_map,
    std::shared_ptr<NetworkData::Buffer> buffer)
  : buffer_(),
    meta_data_(std::move(user), creation_time),
    data_map_(std::move(data_map)),
    version_(),
    buffer_mutex_() {
  Refresh();
  buffer_ = std::move(buffer);
}

BlobContents::BlobContents(BlobContents&& other)
  : buffer_(std::move(other.buffer_)),
    meta_data_(std::move(other.meta_data_)),
    data_map_(std::move(other.data_map_)),
    version_(std::move(other.version_)),
    buffer_mutex_() {
}

bool BlobContents::Equal(const BlobContents& other) const MAIDSAFE_NOEXCEPT {
  return version() == other.version();
}

std::shared_ptr<NetworkData::Buffer> BlobContents::GetBuffer(
    const std::weak_ptr<Network>& network) const {
  const std::lock_guard<std::mutex> lock{buffer_mutex_};
  std::shared_ptr<NetworkData::Buffer> buffer{buffer_.lock()};
  if (buffer == nullptr) {
    buffer = NetworkData::MakeBuffer(network);
    buffer_ = buffer;
  }

  return buffer;
}
  
void BlobContents::Refresh() {
  const std::lock_guard<std::mutex> lock{buffer_mutex_};
  buffer_.reset();
  version_ = UnseededHash<SHA512>{}(meta_data_, data_map_);
} 
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
