/*  Copyright 2015 MaidSafe.net limited

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
#include "maidsafe/nfs/posix_container.h"

#include "boost/algorithm/string/predicate.hpp"

namespace maidsafe {
namespace nfs {
PosixContainer::PosixContainer(std::shared_ptr<detail::Container> container)
  : container_(std::move(container)) {
  if (container_ == nullptr) {
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::null_pointer));
  }
}

PosixContainer PosixContainer::OpenChildContainer(const ContainerInfo& child_info) const {
  assert(container_ != nullptr);
  return PosixContainer{
    std::make_shared<detail::Container>(
        container_->network(),
        container_->container_info(),
        ContainerInfo::Detail::info(child_info))};
}

LocalBlob PosixContainer::CreateLocalBlob() const {
  return LocalBlob{container_->network()};
}

LocalBlob PosixContainer::OpenLocalBlob(const Blob& blob) const {
  return {container_->network(), Blob::Detail::blob(blob)};
}

std::vector<ContainerInfo> PosixContainer::GetContainers::operator()(
    const detail::ContainerInstance& instance, const std::string& prefix) const {

  std::vector<ContainerInfo> containers{};
  containers.reserve(instance.entries().size());

  for (const auto& entry : instance.entries()) {
    detail::ContainerInstance::ExpectContainerInfo(entry.second).bind(
        [&] (detail::ContainerInfo info) {
          if (prefix.empty() || boost::algorithm::starts_with(entry.first.value(), prefix)) {
            containers.emplace_back(entry.first, std::move(info));
          }
        });
  }

  return containers;
}

Expected<ContainerInfo> PosixContainer::GetContainerInfo::operator()(
    const detail::ContainerInstance& instance, const detail::ContainerKey& key) const {
  return instance.GetContainerInfo(key).bind(
      [&] (const detail::ContainerInfo child_info) {
        return ContainerInfo{key, std::move(child_info)};
      });
}

Expected<PosixContainer> PosixContainer::GetPosixContainer::operator()(
    const std::shared_ptr<detail::Container>& container,
    const detail::ContainerInstance& instance,
    const detail::ContainerKey& key) const {
  return instance.GetContainerInfo(key).bind(
      [&container] (detail::ContainerInfo child_info) {
        return PosixContainer{
          std::make_shared<detail::Container>(
              container->network(), container->container_info(), std::move(child_info))};
      });
}

Expected<PosixContainer> PosixContainer::AddContainer::operator()(
    detail::ContainerInstance& instance,
    detail::ContainerKey new_key,
    const std::shared_ptr<detail::Container>& new_container) const {
  assert(new_container != nullptr);

  return instance.UpdateEntries(
      [&] (detail::ContainerInstance::Entries& entries) -> Expected<PosixContainer> {

        const auto insert_attempt =
          entries.insert(
              detail::ContainerInstance::Entry{
                std::move(new_key),
                new_container->container_info()
              });
        if (insert_attempt.second) {
          return PosixContainer{new_container};
        } else {
          return boost::make_unexpected(make_error_code(NfsErrors::bad_modify_version));
        }
      });
}

Expected<void> PosixContainer::RemoveContainer::operator()(
    detail::ContainerInstance& instance, const ContainerInfo& child_info) const {
  using detail::ContainerInstance;
  
  return instance.UpdateEntries(
      [&] (ContainerInstance::Entries& entries) {

        return ContainerInstance::Get(entries, ContainerInfo::Detail::key(child_info)).bind(
            [&] (ContainerInstance::Entries::iterator entry) {

              return ContainerInstance::ExpectContainerInfo(entry->second).bind(
                  [&] (detail::ContainerInfo current_info) -> Expected<void> {

                    if (current_info == ContainerInfo::Detail::info(child_info)) {
                      entries.erase(entry);
                      return Expected<void>{boost::expect};
                    } else {
                      return boost::make_unexpected(
                          make_error_code(NfsErrors::bad_modify_version));
                    }
                  });
            });
      });
}

std::vector<Blob> PosixContainer::GetBlobs::operator()(
    const detail::ContainerInstance& instance, const std::string& prefix) const {
  std::vector<Blob> blobs{};
  blobs.reserve(instance.entries().size());

  for (const auto& entry : instance.entries()) {
    detail::ContainerInstance::ExpectBlob(entry.second).bind(
        [&] (detail::Blob blob) {
          if (prefix.empty() || boost::algorithm::starts_with(entry.first.value(), prefix)) {
            blobs.emplace_back(entry.first, std::move(blob));
          }
        });
  }

  return blobs;
}

Expected<Blob> PosixContainer::GetBlobInternal::operator()(
    const detail::ContainerInstance& instance,
    const detail::ContainerKey& key) const {
  return instance.GetBlob(key).bind([&] (detail::Blob blob) { return Blob{key, std::move(blob)}; });
}
  
Expected<LocalBlob> PosixContainer::GetLocalBlob::operator()(
    const std::shared_ptr<detail::Container>& container,
    const detail::ContainerInstance& instance,
    const detail::ContainerKey& key) const {
  return instance.GetBlob(key).bind(
      [&container] (detail::Blob blob) {
        return LocalBlob{container->network(), std::move(blob)};
      });
}

Expected<Blob> PosixContainer::AddBlob::operator()(
    const std::shared_ptr<detail::Container>& container,
    detail::ContainerInstance& instance,
    const Blob& from,
    detail::ContainerKey to) const {
  return instance.UpdateEntries(
      [&] (detail::ContainerInstance::Entries& entries) -> Expected<Blob> {

        const auto& source = Blob::Detail::blob(from);
        detail::Blob copied_blob{
          container->network().lock(),
          source.meta_data().user_meta_data(),
          source.data_map(),
          nullptr
        };
        const auto insert_attempt = entries.emplace(to, copied_blob);
        if (insert_attempt.second) {
          return Blob{std::move(to), std::move(copied_blob)};
        } else {
          return boost::make_unexpected(make_error_code(NfsErrors::bad_modify_version));
        }
      });
}

Expected<void> PosixContainer::RemoveBlob::operator()(
    detail::ContainerInstance& instance, const Blob& remove) const {
  return instance.UpdateEntries(
      [&] (detail::ContainerInstance::Entries& entries) {

        return detail::ContainerInstance::Get(entries, Blob::Detail::key(remove)).bind(
            [&] (detail::ContainerInstance::Entries::iterator entry) {

              return detail::ContainerInstance::ExpectBlob(entry->second).bind(
                  [&] (detail::Blob current_blob) -> Expected<void> {

                    if (current_blob == Blob::Detail::blob(remove)) {
                      entries.erase(entry);
                      return Expected<void>{boost::expect};
                    } else {
                      return boost::make_unexpected(
                          make_error_code(NfsErrors::bad_modify_version));
                    }
                  });
            });
      });
}
}  // namespace nfs
}  // namespace maidsafe
