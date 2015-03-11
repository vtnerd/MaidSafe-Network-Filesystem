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
#ifndef MAIDSAFE_NFS_DETAIL_OBJECT_CACHE_H_
#define MAIDSAFE_NFS_DETAIL_OBJECT_CACHE_H_

#include <memory>
#include <mutex>
#include <utility>

#include "maidsafe/common/hash/algorithms/sha.h"
#include "maidsafe/common/hash/hash_array.h"
#include "maidsafe/common/hash/wrappers/unseeded_hash.h"
#include "maidsafe/common/unordered_map.h"

namespace maidsafe {
namespace nfs {
namespace detail {
template<typename CachedType>
class ObjectCache {
 public:
  ObjectCache()
    : cache_(),
      mutex_() {
  }

  // Insert object into the cache, or retrieve the existing version.
  template<typename InType, typename Deleter>
  std::shared_ptr<const CachedType> Insert(InType&& object, Deleter&& deleter) {
    auto key = UnseededHash<SHA512>{}(object);
    std::shared_ptr<const CachedType> instance{};
    {
      const std::lock_guard<std::mutex> lock{mutex_};
      std::weak_ptr<const CachedType>& cached_entry = cache_[std::move(key)];
      instance = cached_entry.lock();
      if (instance == nullptr || *instance != object) {
        instance = std::shared_ptr<const CachedType>{
          new CachedType{std::forward<InType>(object)}, std::forward<Deleter>(deleter)};
        cached_entry = instance;
      }
    }
    assert(instance != nullptr);
    return instance;
  }

  // If the weak_ptr to object has no references, the entry is removed
  void Erase(const CachedType& object) {
    const auto key = UnseededHash<SHA512>{}(object);
    {
      const std::lock_guard<std::mutex> lock{mutex_};
      const auto entry = cache_.find(key);
      if (entry != cache_.end()) {
        if (entry->second.expired()) {
          cache_.erase(entry);
        }
      }
    }
  }

 private:
  ObjectCache(const ObjectCache&) = delete;
  ObjectCache& operator=(const ObjectCache&) = delete;

 private:
  unordered_map<SHA512::Digest, std::weak_ptr<const CachedType>> cache_;
  std::mutex mutex_;
};
}  // detail
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_OBJECT_CACHE_H_
