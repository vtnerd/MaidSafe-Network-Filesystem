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
#include "maidsafe/nfs/detail/container_instance.h"

#include "boost/throw_exception.hpp"
#include "boost/variant/apply_visitor.hpp"
#include "cereal/cereal.hpp"
#include "cereal/types/boost_variant.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/unordered_map.hpp"

#include "maidsafe/common/error.h"
#include "maidsafe/common/serialisation/serialisation.h"
#include "maidsafe/nfs/detail/blob.h"

namespace maidsafe {
namespace nfs {
namespace detail {
namespace {
template<typename Value>
struct CopyType : boost::static_visitor<Expected<Value>> {
  template<typename ValueIn>
  Expected<Value> operator()(const ValueIn&) const {
    return boost::make_unexpected(make_error_code(CommonErrors::invalid_conversion));
  }

  Expected<Value> operator()(const Value& value) const {
    return value;
  }
};

template<typename Entries>
auto GetInternal(Entries& entries, const ContainerKey& key)
    -> Expected<decltype(entries.find(key))> {
  const auto entry = entries.find(key);
  if (entry == entries.end()) {
    return boost::make_unexpected(make_error_code(CommonErrors::no_such_element));
  }
  return entry;
}
}  // namespace

ContainerInstance::ContainerInstance() : meta_data_(), entries_() {}
ContainerInstance::ContainerInstance(std::initializer_list<Entry>&& entries) : ContainerInstance() {
  for (auto& entry : entries) {
    entries_[std::move(entry.first)] = std::move(entry.second);
  }

  meta_data_.UpdateModificationTime();
}

Expected<ContainerInstance> ContainerInstance::Parse(const std::vector<byte>& serialised) {
  try {
    return ::maidsafe::Parse<ContainerInstance>(serialised);
  } catch (const cereal::Exception&) {
    return boost::make_unexpected(make_error_code(CommonErrors::parsing_error));
  }
}

std::vector<byte> ContainerInstance::Serialise() const {
  return ::maidsafe::Serialise(*this);
}

template<typename Archive>
Archive& ContainerInstance::serialize(Archive& archive, const std::uint32_t /*version*/) {
  return archive(meta_data_, entries_);
}

Expected<detail::Blob> ContainerInstance::ExpectBlob(const Value& value) {
  return boost::apply_visitor(CopyType<detail::Blob>{}, value);
}

Expected<detail::ContainerInfo> ContainerInstance::ExpectContainerInfo(const Value& value) {
  return boost::apply_visitor(CopyType<detail::ContainerInfo>{}, value);
}

Expected<detail::Blob> ContainerInstance::GetBlob(const ContainerKey& key) const {
  return Get(entries(), key).bind(
      [] (Entries::const_iterator entry) { return ExpectBlob(entry->second); });
}

Expected<detail::ContainerInfo> ContainerInstance::GetContainerInfo(const ContainerKey& key) const {
  return Get(entries(), key).bind(
      [] (Entries::const_iterator entry) { return ExpectContainerInfo(entry->second); });
}

Expected<ContainerInstance::Entries::const_iterator> ContainerInstance::Get(
    const Entries& entries, const ContainerKey& key) {
  return GetInternal(entries, key);
}

Expected<ContainerInstance::Entries::iterator> ContainerInstance::Get(
    Entries& entries, const ContainerKey& key) {
  return GetInternal(entries, key);
}

}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
