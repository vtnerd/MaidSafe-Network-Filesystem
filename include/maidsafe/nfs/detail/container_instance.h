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
#ifndef MAIDSAFE_NFS_DETAIL_CONTAINER_INSTANCE_H_
#define MAIDSAFE_NFS_DETAIL_CONTAINER_INSTANCE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "boost/variant/variant.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/types.h"
#include "maidsafe/common/unordered_map.h"
#include "maidsafe/nfs/detail/blob.h"
#include "maidsafe/nfs/detail/container_info.h"
#include "maidsafe/nfs/detail/container_key.h"
#include "maidsafe/nfs/detail/meta_data.h"
#include "maidsafe/nfs/expected.h"

namespace cereal { class access; }

namespace maidsafe {
namespace nfs {
namespace detail {
class ContainerInstance {
 public:
  friend class cereal::access;

  using Value = boost::variant<detail::ContainerInfo, detail::Blob>;
  using Entry = std::pair<const ContainerKey, Value>;
  using Entries = maidsafe::unordered_map<ContainerKey, Value>;

  ContainerInstance();
  ContainerInstance(std::initializer_list<Entry>&& entries);

  ContainerInstance(const ContainerInstance&) = default;
  ContainerInstance(ContainerInstance&& other)
    : meta_data_(std::move(other.meta_data_)),
      entries_(std::move(other.entries_)) {
  }

  ContainerInstance& operator=(ContainerInstance other) MAIDSAFE_NOEXCEPT {
    swap(other);
    return *this;
  }

  void swap(ContainerInstance& other) MAIDSAFE_NOEXCEPT {
    using std::swap;
    swap(meta_data_, other.meta_data_);
    swap(entries_, other.entries_);
  }

  static Expected<ContainerInstance> Parse(
      std::shared_ptr<Network> network, const std::vector<byte>& serialised);
  std::vector<byte> Serialise() const;

  const Entries& entries() const { return entries_; }
  const MetaData& meta_data() const { return meta_data_; }

  static Expected<Entries::const_iterator> Get(const Entries& entries, const ContainerKey& key);
  static Expected<Entries::iterator> Get(Entries& entries, const ContainerKey& key);

  static Expected<detail::Blob> ExpectBlob(const Value& value);
  static Expected<detail::ContainerInfo> ExpectContainerInfo(const Value& value);

  Expected<detail::Blob> GetBlob(const ContainerKey& key) const;
  Expected<detail::ContainerInfo> GetContainerInfo(const ContainerKey& key) const;

  // Update callback must have syntax Expected<unspecified>(ContainerInstance::Entries&)
  template<typename Update>
  typename std::result_of<Update(Entries&)>::type UpdateEntries(const Update& update) {
    const auto result = update(entries_);
    if (result) {
      meta_data_.UpdateModificationTime();
    }
    return result;
  }

 private:
  template<typename Archive>
  Archive& serialize(Archive& archive, const std::uint32_t /*version*/);

 private:
  MetaData meta_data_;
  Entries entries_;
};

inline void swap(ContainerInstance& lhs, ContainerInstance& rhs) MAIDSAFE_NOEXCEPT {
  lhs.swap(rhs);
}
}  // detail
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_CONTAINER_INSTANCE_H_
