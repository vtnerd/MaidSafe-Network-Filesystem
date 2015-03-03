/*  Copyright 2013 MaidSafe.net limited

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

namespace maidsafe {
namespace nfs {
PosixContainer::PosixContainer(std::shared_ptr<detail::Container> container)
  : container_(std::move(container)) {
  if (container_ == nullptr) {
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::null_pointer));
  }
}

PosixContainer PosixContainer::OpenContainer(ContainerInfo child_container_info) const {
  assert(container_ != nullptr);
  return PosixContainer{
    std::make_shared<detail::Container>(
        container_->network(),
        container_->container_info(),
        static_cast<detail::ContainerInfo>(std::move(child_container_info)))};
}

LocalBlob PosixContainer::CreateLocalBlob() const {
  return LocalBlob{container_->network()};
}

LocalBlob PosixContainer::OpenBlob(const Blob& blob) const {
  return {container_->network(), static_cast<detail::Blob>(blob)};
}

std::vector<ContainerInfo> PosixContainer::ExtractContainers::operator()(
    const detail::ContainerInstance& instance) const {

  std::vector<ContainerInfo> containers{};
  containers.reserve(instance.entries().size());

  for (const auto& entry : instance.entries()) {
    detail::ContainerInstance::ExpectContainerInfo(entry.second).bind(
        [&] (detail::ContainerInfo info) {
          containers.emplace_back(entry.first, std::move(info));
        });
  }

  return containers;
}

Expected<PosixContainer> PosixContainer::GetContainer::operator()(
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

Expected<void> PosixContainer::AddContainer::operator()(
    detail::ContainerInstance& instance,
    const detail::ContainerKey& key,
    const detail::ContainerInfo& new_container) const {
  return instance.UpdateEntries(
      [&] (detail::ContainerInstance::Entries& entries) -> Expected<void> {

        const auto insert_attempt =
          entries.insert(detail::ContainerInstance::Entry{key, new_container});
        if (insert_attempt.second) {
          return Expected<void>{boost::expect};
        } else {
          return boost::make_unexpected(make_error_code(NfsErrors::bad_modify_version));
        }
      });
}

Expected<void> PosixContainer::RemoveContainer::operator()(
    detail::ContainerInstance& instance,
    const detail::ContainerKey& key,
    const ContainerVersion& current_version,
    const RetrieveContainerVersion& replace) const {
  return instance.UpdateEntries(
      [&] (detail::ContainerInstance::Entries& entries) {

        return detail::ContainerInstance::Get(entries, key).bind(
            [&] (detail::ContainerInstance::Entries::iterator entry) {

              return detail::ContainerInstance::ExpectContainerInfo(entry->second).bind(
                  [&] (detail::ContainerInfo) -> Expected<void> {

                    if (replace == current_version ||
                        replace == RetrieveContainerVersion::Latest()) {
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

std::vector<Blob> PosixContainer::ExtractBlobs::operator()(
    const detail::ContainerInstance& instance) const {
  std::vector<Blob> blobs{};
  blobs.reserve(instance.entries().size());

  for (const auto& entry : instance.entries()) {
    detail::ContainerInstance::ExpectBlob(entry.second).bind(
        [&] (detail::Blob blob) {
          blobs.emplace_back(entry.first, std::move(blob));
        });
  }

  return blobs;
}

Expected<void> PosixContainer::RemoveBlob::operator()(
    detail::ContainerInstance& instance,
    const detail::ContainerKey& key,
    const RetrieveBlobVersion& replace) const {
  return instance.UpdateEntries(
      [&] (detail::ContainerInstance::Entries& entries) {

        return detail::ContainerInstance::Get(entries, key).bind(
            [&] (detail::ContainerInstance::Entries::iterator entry) {

              return detail::ContainerInstance::ExpectBlob(entry->second).bind(
                  [&] (detail::Blob current_blob) -> Expected<void> {

                    if (replace == current_blob.version() ||
                        replace == RetrieveBlobVersion::Latest()) {
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
