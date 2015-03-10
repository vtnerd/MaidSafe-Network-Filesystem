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
#include "maidsafe/nfs/detail/container_key.h"

#include "boost/throw_exception.hpp"

#include "maidsafe/common/error.h"
#include "maidsafe/nfs/detail/nfs_input_archive.h"

namespace maidsafe {
namespace nfs {
namespace detail {
ContainerKey::ContainerKey() : value_(std::make_shared<std::string>()) {}
ContainerKey::ContainerKey(const std::shared_ptr<Network>& network, const std::string& key)
  : value_(Network::CacheInsert(network, key)) {
  if (value_ == nullptr) {
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::null_pointer));
  }
}

NfsInputArchive& ContainerKey::load(NfsInputArchive& archive) {
  assert(value_ != nullptr);
  archive(*value);
  value_ = Network::CacheInsert(archive.network(), std::move(*value));

  if (value_ == nullptr) {
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::null_pointer));
  }
  
  return archive;
}
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
