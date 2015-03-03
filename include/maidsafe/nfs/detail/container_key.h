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
#ifndef MAIDSAFE_NFS_DETAIL_CONTAINER_KEY_H_
#define MAIDSAFE_NFS_DETAIL_CONTAINER_KEY_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "boost/flyweight.hpp"
#include "cereal/types/string.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/hash/algorithms/siphash.h"
#include "maidsafe/common/hash/hash_numeric.h"
#include "maidsafe/common/hash/hash_string.h"
#include "maidsafe/common/hash/wrappers/seeded_hash.h"
#include "maidsafe/common/serialisation/types/boost_flyweight.h"

namespace cereal { class access; }

namespace maidsafe {
namespace nfs {
namespace detail {
// Type used to represent the KEY in a Container (see ContainerInstance)
class ContainerKey {
 public:
  friend class cereal::access;

  ContainerKey();
  ContainerKey(std::string value);

  ContainerKey(const ContainerKey&) = default;
  ContainerKey(ContainerKey&& other) MAIDSAFE_NOEXCEPT
    : value_(std::move(other.value_)) {
  }

  ~ContainerKey();

  ContainerKey& operator=(const ContainerKey&) = default;
  ContainerKey& operator=(ContainerKey&& other) MAIDSAFE_NOEXCEPT {
    value_ = std::move(other.value_);
    return *this;
  }

  template<typename HashAlgorithm>
  void HashAppend(HashAlgorithm& hash) const {
    // Each flyweight pointer is already unique;
    // this type is only used in unordered_map key.
    hash(std::uintptr_t(std::addressof(value())));
  }

  bool Equal(const ContainerKey& other) const MAIDSAFE_NOEXCEPT {
    return value_ == other.value_;
  }

  void swap(ContainerKey& other) MAIDSAFE_NOEXCEPT {
    using std::swap;
    swap(value_, other.value_);
  }

  const std::string& value() const MAIDSAFE_NOEXCEPT { return value_; }

 private:
  template<typename Archive>
  Archive& load(Archive& archive);

  // Skip flyweight serialisation entirely, and save 4 bytes of space per key. A
  // container key can never appear twice in a single ContainerInstance.
  template<typename Archive>
  Archive& save(Archive& archive) const {
    return archive(value());
  }

 private:
  boost::flyweight<
    std::string, boost::flyweights::hashed_factory<SeededHash<SipHash, std::string>>> value_;
};

inline void swap(ContainerKey& lhs, ContainerKey& rhs) MAIDSAFE_NOEXCEPT {
  lhs.swap(rhs);
}

inline bool operator==(const ContainerKey& lhs, const ContainerKey& rhs) MAIDSAFE_NOEXCEPT {
  return lhs.Equal(rhs);
}

inline bool operator!=(const ContainerKey& lhs, const ContainerKey& rhs) MAIDSAFE_NOEXCEPT {
  return !lhs.Equal(rhs);
}
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_CONTAINER_KEY_H_
