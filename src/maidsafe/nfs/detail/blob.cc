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
#include "maidsafe/common/serialisation/serialisation.h"
#include "maidsafe/common/serialisation/types/boost_flyweight.h"
#include "maidsafe/nfs/detail/pending_blob.h"

namespace maidsafe {
namespace nfs {
namespace detail {

/* Keep constructor, destructor, and serialize method in cc file. These
   instantiate a templated singleton object for flyweight, and the easiest
   way to keep these in maidsafe DSOs is to keep them in maidsafe TU. Otherwise,
   multiple flyweight registries will exist.*/

Blob::Blob() : contents_() {}
Blob::Blob(const PendingBlob& pending_blob) : contents_(pending_blob) {}
Blob::Blob(const PendingBlob& pending_blob, Clock::time_point creation_time)
  : contents_(pending_blob, creation_time) {
}

Blob::~Blob() {}

template<typename Archive>
Archive& Blob::serialize(Archive& archive) {
  return archive(contents_);
}

template BinaryInputArchive& Blob::serialize<BinaryInputArchive>(BinaryInputArchive&);
template BinaryOutputArchive& Blob::serialize<BinaryOutputArchive>(BinaryOutputArchive&);

Blob::Contents::Contents()
  : version_(),
    meta_data_(),
    buffer_(),
    buffer_mutex_() {
  Refresh();
}

Blob::Contents::Contents(const PendingBlob& pending_blob)
  : version_(),
    meta_data_(pending_blob.user_meta_data()),
    data_map_(pending_blob.data_map()),
    buffer_(),
    buffer_mutex_() {
  Refresh();
  buffer_ = pending_blob.buffer();
}

Blob::Contents::Contents(const PendingBlob& pending_blob, Clock::time_point creation_time)
  : version_(),
    meta_data_(pending_blob.user_meta_data(), creation_time),
    data_map_(pending_blob.data_map()),
    buffer_(),
    buffer_mutex_() {
  Refresh();
  buffer_ = pending_blob.buffer();
}

void Blob::Contents::Refresh() {
  const std::lock_guard<std::mutex> lock{buffer_mutex_};
  buffer_.reset();
  maidsafe::SHA512 hash{};
  hash(meta_data_, data_map_);
  version_ = BlobVersion{hash.Finalize()};
}

std::shared_ptr<NetworkData::Buffer> Blob::Contents::GetBuffer(
    const std::weak_ptr<Network>& network) const {
  const std::lock_guard<std::mutex> lock{buffer_mutex_};
  std::shared_ptr<NetworkData::Buffer> buffer{buffer_.lock()};
  if (buffer == nullptr) {
    buffer = NetworkData::MakeBuffer(network);
    buffer_ = buffer;
  }

  return buffer;
}

}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
