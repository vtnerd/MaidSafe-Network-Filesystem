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
#include "maidsafe/nfs/detail/meta_data.h"

namespace maidsafe {
namespace nfs {
namespace detail {

MetaData::MetaData()
  : user_meta_data_(),
    creation_time_(Clock::now()),
    modification_time_(creation_time()) {
}

MetaData::MetaData(UserMetaData user_meta_data)
  : user_meta_data_(std::move(user_meta_data)),
    creation_time_(Clock::now()),
    modification_time_(creation_time()) {
}

MetaData::MetaData(UserMetaData user_meta_data, Clock::time_point creation_time)
  : user_meta_data_(std::move(user_meta_data)),
    creation_time_(creation_time),
    modification_time_(Clock::now()) {
}

MetaData::MetaData(MetaData&& other)
  : user_meta_data_(std::move(other.user_meta_data_)),
    creation_time_(std::move(other.creation_time_)),
    modification_time_(std::move(other.modification_time_)) {
}

void MetaData::swap(MetaData& other) MAIDSAFE_NOEXCEPT {
  using std::swap;
  swap(user_meta_data_, other.user_meta_data_);
  swap(creation_time_, other.creation_time_);
  swap(modification_time_, other.modification_time_);
}

Expected<void> MetaData::set_user_meta_data(std::string user) {
  return user_meta_data_.set_value(std::move(user)).bind([this] { UpdateModificationTime(); });
}

void MetaData::UpdateModificationTime() MAIDSAFE_NOEXCEPT {
  modification_time_ = Clock::now();
}

}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
