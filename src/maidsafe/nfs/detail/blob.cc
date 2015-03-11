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

#include "cereal/types/common.hpp"

#include "maidsafe/nfs/detail/network.h"
#include "maidsafe/nfs/detail/nfs_binary_archive.h"

namespace maidsafe {
namespace nfs {
namespace detail {
namespace {
void VerifyContents(const std::shared_ptr<const BlobContents>& contents) {
  if (contents == nullptr) {
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::null_pointer));
  }
}
}  // namespace
  
Blob::Blob() : contents_(std::make_shared<BlobContents>()) {}
Blob::Blob(const std::shared_ptr<Network>& network, const PendingBlob& pending_blob)
  : contents_(Network::CacheInsert(network, BlobContents{pending_blob})) {
  VerifyContents(contents_);
}

Blob::Blob(
    const std::shared_ptr<Network>& network,
    const PendingBlob& pending_blob,
    Clock::time_point creation_time)
  : contents_(Network::CacheInsert(network, BlobContents{pending_blob, creation_time})) {
  VerifyContents(contents_);
}

NfsInputArchive& Blob::load(NfsInputArchive& archive) {
  {
    BlobContents contents{};
    archive(contents);
    contents_ = Network::CacheInsert(archive.network(), std::move(contents));
  }
  VerifyContents(contents_);
  return archive;
}
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
