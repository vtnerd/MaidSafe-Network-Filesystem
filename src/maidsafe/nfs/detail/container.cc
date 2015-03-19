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
#include "maidsafe/nfs/detail/container.h"

#include <algorithm>

#include "boost/range/adaptor/sliced.hpp"
#include "boost/range/algorithm/equal.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/crypto.h"
#include "maidsafe/common/on_scope_exit.h"
#include "maidsafe/encrypt/data_map_encryptor.h"

namespace maidsafe {
namespace nfs {
namespace detail {

namespace {
template<typename Size>
boost::optional<Size> FindOverlap(
    const std::vector<ContainerVersion>& old_versions,
    const std::vector<ContainerVersion>& new_versions,
    const Size difference) {
  using boost::adaptors::sliced;
  static_assert(std::is_unsigned<Size>::value, "expected unsigned");
  static_assert(std::is_unsigned<decltype(old_versions.size())>::value, "expected unsigned");
  static_assert(std::is_unsigned<decltype(new_versions.size())>::value, "expected unsigned");

  // < instead of <= guarantees at least one element is matched between versions
  if (difference < new_versions.size()) {

    const auto overlap_size = new_versions.size() - difference;
    assert(0 < overlap_size);

    if (overlap_size <= old_versions.size() &&
        boost::range::equal(
            (new_versions | sliced(difference, new_versions.size())),
            (old_versions | sliced(0, overlap_size)))) {
      return overlap_size;
    }
  }
  return boost::none;
}
}  // namespace

MAIDSAFE_CONSTEXPR_OR_CONST std::uint16_t Container::kRefreshInterval;

Container::Container(std::weak_ptr<Network> network, ContainerInfo parent_info)
  : Container(std::move(network), std::move(parent_info), ContainerInfo{}) {
}

Container::Container(
    std::weak_ptr<Network> network,
    ContainerInfo parent_info,
    ContainerInfo container_info)
  : network_(std::move(network)),
    cached_versions_(),
    cached_instances_(),
    parent_info_(std::move(parent_info)),
    container_info_(std::move(container_info)),
    last_update_(),
    data_mutex_() {
}

std::system_error Container::MakeNullPointerException() {
  return MakeError(CommonErrors::null_pointer);
}

bool Container::IsVersionError(const std::error_code& error) {
  // Cannot_exceed_limit is the error code because the
  // SDV branch limit was set to 1 (which was exceeded).
  return error == CommonErrors::cannot_exceed_limit;
}

boost::optional<std::vector<ContainerVersion>> Container::GetCachedVersions() const {
  const std::lock_guard<std::mutex> lock{data_mutex_};
  if (last_update_) {
    if ((std::chrono::steady_clock::now() - *last_update_) < GetRefreshInterval()) {
      return std::vector<ContainerVersion>(cached_versions_);
    }
  }
  return boost::none;
}

void Container::UpdateCachedVersions(std::vector<ContainerVersion> remote_versions) {
  const std::lock_guard<std::mutex> lock{data_mutex_};
  on_scope_exit purge_cache([this, &lock] {
      PurgeInstanceCache(lock);
      PurgeVersionCache(lock);
    });

  if (!last_update_) {
    cached_versions_ = remote_versions;
    last_update_ = std::chrono::steady_clock::now();
  } else if (!cached_versions_.empty() && !remote_versions.empty()) {
    using Index = decltype(cached_versions_.front().index);
    static_assert(std::is_unsigned<Index>::value, "index must be unsigned for algorithm below");

    const Index difference = std::min<Index>(
        (cached_versions_.front().index - remote_versions.front().index),
        (remote_versions.front().index - cached_versions_.front().index));

    const auto overlap_size = FindOverlap(cached_versions_, remote_versions, difference);
    if (overlap_size) {
      using boost::adaptors::sliced;
      assert(*overlap_size <= cached_versions_.size());

      // quicker than cache pruning below, we know expired entries
      const auto expired_list = (cached_versions_ | sliced(*overlap_size, cached_versions_.size()));
      for (const auto& expired : expired_list) {
        PruneCache(cached_instances_.find(expired), lock);
      }

      cached_versions_ = std::move(remote_versions);
      last_update_ = std::chrono::steady_clock::now();
      purge_cache.Release();
      return;
    } else if (FindOverlap(remote_versions, cached_versions_, difference)) {
      // remote version is older than currently cached version
      purge_cache.Release();
      return;
    }
  }

  // Prune cache
  assert(remote_versions.size() <= Network::GetMaxVersions());
  std::sort(remote_versions.begin(), remote_versions.end());
  for (auto iter(cached_instances_.begin()); iter != cached_instances_.end();) {
    if (std::binary_search(remote_versions.begin(), remote_versions.end(), iter->first)) {
      ++iter;
    } else {
      iter = PruneCache(iter, lock);
    }
  }

  purge_cache.Release();
}

void Container::AddNewCachedVersion(
    const boost::optional<ContainerVersion>& old_version,
    ContainerVersion new_version,
    ContainerInstance instance) {
  const std::lock_guard<std::mutex> lock{data_mutex_};

  /* We have to be careful about updating the cached_versions_ list without
     receiving a specific list from the remote side. Only manually update our
     cached version history if it is certain that a new version was NOT posted
     and then pulled in GetVersions before acquiring the data_mutex_. Do NOT
     update the last_update_ timestamp; pulling down a update will synchronize
     the versions that have been aged out, and free some of the local cache. */

  bool out_of_sync = true;
  if (!old_version) {
    if (cached_versions_.empty()) {
      cached_versions_.insert(cached_versions_.begin(), new_version);
      out_of_sync = false;
    }
  } else {
    if (!cached_versions_.empty() && cached_versions_.front() == *old_version) {
      cached_versions_.insert(cached_versions_.begin(), new_version);
      out_of_sync = false;
    }
  }

  if (out_of_sync) {
    // force network request for next GetVersion
    PurgeVersionCache(lock);
  }

  // always add it to the cache
  AddCachedInstance(std::move(new_version), std::move(instance), lock);
}

void Container::AddCachedInstance(
    ContainerVersion new_version,
    ContainerInstance instance,
    const std::lock_guard<std::mutex>& data_mutex_lock) {
  on_scope_exit purge_cache([this, &data_mutex_lock] {
      PurgeInstanceCache(data_mutex_lock);
    });

  cached_instances_.insert(std::make_pair(std::move(new_version), std::move(instance)));
  purge_cache.Release();
}

boost::optional<ContainerInstance> Container::GetCachedInstance(
    const ContainerVersion& version) const {
  const std::lock_guard<std::mutex> lock{data_mutex_};
  const auto cached_version = cached_instances_.find(version);
  if (cached_version != cached_instances_.end()) {
    return ContainerInstance{cached_version->second};
  }

  return boost::none;
}

Expected<ImmutableData> Container::EncryptVersion(const encrypt::DataMap& data_map) const {
  try {
    return ImmutableData{
      encrypt::EncryptDataMap(parent_info().key(), container_info().key(), data_map).data};
  } catch (const std::system_error& e) {
    return boost::make_unexpected(e.code());
  }
}

Expected<ContainerInstance> Container::DecryptAndCacheInstance(
    std::shared_ptr<Network> network,
    ContainerVersion version,
    const ImmutableData& encrypted_version) {
  std::vector<byte> serialised_instance{};

  try {
    NetworkData data{
      encrypt::DecryptDataMap(
          parent_info().key(), container_info().key(), encrypted_version.data().string()),
      network_};

    serialised_instance.resize(data.encryptor().size());
    data.encryptor().Read(
        reinterpret_cast<char*>(&serialised_instance[0]), serialised_instance.size(), 0);
  } catch (const std::system_error& e) {
    return boost::make_unexpected(e.code());
  }

  return ContainerInstance::Parse(std::move(network), serialised_instance).bind(
      [this, &version] (ContainerInstance loaded_instance) {

        const std::lock_guard<std::mutex> lock{data_mutex_};
        AddCachedInstance(std::move(version), loaded_instance, lock);
        return loaded_instance;
      });
}

maidsafe::unordered_map<ContainerVersion, const ContainerInstance>::iterator Container::PruneCache(
    maidsafe::unordered_map<ContainerVersion, const ContainerInstance>::iterator prune_entry,
    const std::lock_guard<std::mutex>& /*data_mutex_*/) {
  if (prune_entry != cached_instances_.end()) {
    return cached_instances_.erase(prune_entry);
  }

  return cached_instances_.end();
}

void Container::PurgeVersionCache(const std::lock_guard<std::mutex>& /*data_mutex_*/) MAIDSAFE_NOEXCEPT {
  cached_versions_.clear();
  last_update_ = boost::none;
}

void Container::PurgeInstanceCache(const std::lock_guard<std::mutex>& /*data_mutex_*/) MAIDSAFE_NOEXCEPT {
  cached_instances_.clear();
}

}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
