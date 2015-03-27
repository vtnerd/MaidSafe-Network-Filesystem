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
#include "maidsafe/nfs/detail/disk_backend.h"

#include "boost/expected/conversions/expected_to_future.hpp"
#include "boost/filesystem/operations.hpp"

#include "maidsafe/common/make_unique.h"
#include "maidsafe/common/utils.h"

namespace maidsafe {
namespace nfs {
namespace detail {
namespace {
const unsigned kDepth = 5;

DiskUsage InitialiseDiskRoot(const boost::filesystem::path& disk_root) {
  if (!boost::filesystem::exists(disk_root)) {
    boost::system::error_code error{};
    boost::filesystem::create_directories(disk_root, error);
    if (error) {
      LOG(kError) << "Can't create disk root at " << disk_root << ": " << error.message();
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::uninitialised));
    }
  }
  return DiskUsage{0};
}
}

DiskBackend::DiskBackend(const boost::filesystem::path& disk_path, DiskUsage max_disk_usage)
  : Network(),
    disk_path_(disk_path),
    max_disk_usage_(max_disk_usage),
    current_disk_usage_(InitialiseDiskRoot(disk_path_)),
    mutex_() {
}

DiskBackend::~DiskBackend() {}

void DiskBackend::DoCreateSDV(
    std::function<void(Expected<void>)> callback,
    const ContainerId& container_id,
    const ContainerVersion& initial_version,
    std::uint32_t max_versions,
    std::uint32_t max_branches) {
  Expected<void> result{};
  StructuredDataVersions versions{max_versions, max_branches};
  versions.Put(StructuredDataVersions::VersionName{}, initial_version);
  {
    const std::lock_guard<std::mutex> lock{mutex_};
    result = WriteVersions(container_id.data, versions, true);
  }
  callback(std::move(result));
}

void DiskBackend::DoPutSDVVersion(
    std::function<void(Expected<void>)> callback,
    const ContainerId& container_id,
    const ContainerVersion& old_version,
    const ContainerVersion& new_version) {
  Expected<void> result{};
  {
    const std::lock_guard<std::mutex> lock{mutex_};
    result = ReadVersions(container_id.data).bind(
        [&] (std::unique_ptr<StructuredDataVersions> versions) -> Expected<void> {
          try {
            versions->Put(old_version, new_version);
          } catch (const std::system_error& error) {
            return boost::make_unexpected(error.code());
          }
          return WriteVersions(container_id.data, *versions, false);
        });
  }
  callback(std::move(result));
}

void DiskBackend::DoGetBranches(
    std::function<void(Expected<std::vector<ContainerVersion>>)> callback,
    const ContainerId& container_id) {
  Expected<std::unique_ptr<StructuredDataVersions>> versions{};
  {
    const std::lock_guard<std::mutex> lock{mutex_};
    versions = ReadVersions(container_id.data);
  }

  callback(
      versions.bind(
          [] (std::unique_ptr<StructuredDataVersions> versions)
              -> Expected<std::vector<ContainerVersion>> {
            try {
              return versions->Get();
            } catch (const std::system_error& error) {
              return boost::make_unexpected(error.code());
            }
          }));
}

void DiskBackend::DoGetBranchVersions(
    std::function<void(Expected<std::vector<ContainerVersion>>)> callback,
    const ContainerId& container_id,
    const ContainerVersion& tip) {
  Expected<std::unique_ptr<StructuredDataVersions>> versions{};
  {
    const std::lock_guard<std::mutex> lock{mutex_};
    versions = ReadVersions(container_id.data);
  }

  callback(
      versions.bind(
          [&] (std::unique_ptr<StructuredDataVersions> versions)
              -> Expected<std::vector<ContainerVersion>> {
            try {
              return versions->GetBranch(tip);
            } catch (const std::system_error& error) {
              return boost::make_unexpected(error.code());
            }
          }));
}

void DiskBackend::DoPutChunk(
    std::function<void(Expected<void>)> callback, const ImmutableData& data) {
  Expected<void> result{};
  {
    const std::lock_guard<std::mutex> lock{mutex_};
    if (!boost::filesystem::exists(disk_path_)) {
      result = boost::make_unexpected(make_error_code(CommonErrors::filesystem_io_error));
    } else {
      result = KeyToFilePath(data.NameAndType(), true).bind(
          [&] (boost::filesystem::path file_path) {

            if (!boost::filesystem::exists(file_path)) {
              return Write(file_path, data.Value());
            }
            return Expected<void>{boost::expect};
          });
    }
  }
  callback(std::move(result));
}

void DiskBackend::DoGetChunk(
    std::function<void(Expected<ImmutableData>)> callback,
    const ImmutableData::NameAndTypeId& name) {
  Expected<std::vector<byte>> result{};
  {
    const std::lock_guard<std::mutex> lock{mutex_};
    result = KeyToFilePath(name, false).bind(
        [] (boost::filesystem::path file_path) -> Expected<std::vector<byte>> {

          const auto data = ReadFile(file_path);
          if (data) {
            return *data;
          }
          return boost::make_unexpected(make_error_code(CommonErrors::no_such_element));
        });
  }

  callback(
      result.bind(
          [](std::vector<byte> data) {
            return ImmutableData{NonEmptyString{std::move(data)}};
          }));
}

bool DiskBackend::HasDiskSpace(std::uintmax_t required_space) const {
  assert(current_disk_usage_ <= max_disk_usage_);
  return (max_disk_usage_ - current_disk_usage_) >= required_space;
}

Expected<boost::filesystem::path> DiskBackend::KeyToFilePath(
    const Data::NameAndTypeId& key, bool create_if_missing) const {
  const auto file_name = ::maidsafe::detail::GetFileName(key).string();

  if (file_name.empty()) {
    return boost::make_unexpected(make_error_code(CommonErrors::outside_of_bounds));
  }

  boost::filesystem::path disk_path{disk_path_};
  auto character = file_name.begin();
  {
    const auto last_character = file_name.end() - 1;
    for (unsigned i = 0; i < kDepth && character != last_character; ++i, ++character) {
      disk_path.append(character, character + 1);
    }
  }

  if (create_if_missing) {
    boost::system::error_code error{};
    boost::filesystem::create_directories(disk_path, error);
    if (error) {
      return boost::make_unexpected(make_error_code(CommonErrors::filesystem_io_error));
    }
  }

  return disk_path.append(character, file_name.end());
}

Expected<std::unique_ptr<StructuredDataVersions>> DiskBackend::ReadVersions(
      const Data::NameAndTypeId& key) const {
  return KeyToFilePath(key, false).bind(
      [] (boost::filesystem::path file_path) -> Expected<std::unique_ptr<StructuredDataVersions>> {
        file_path += ".ver";
        if (boost::filesystem::exists(file_path)) {
          auto file_data = ReadFile(file_path);
          if (!file_data) {
            return boost::make_unexpected(std::error_code(file_data.error().code()));
          }
          return maidsafe::make_unique<StructuredDataVersions>(
              StructuredDataVersions::serialised_type{NonEmptyString{std::move(*file_data)}});
        }

        return boost::make_unexpected(make_error_code(VaultErrors::no_such_account));
      });
}

Expected<void> DiskBackend::WriteVersions(
    const Data::NameAndTypeId& key, const StructuredDataVersions& versions, const bool creation) {
  if (!boost::filesystem::exists(disk_path_)) {
    return boost::make_unexpected(make_error_code(CommonErrors::filesystem_io_error));
  }

  return KeyToFilePath(key, true).bind(
      [&] (boost::filesystem::path file_path) -> Expected<void> {
        file_path += ".ver";
        if (boost::filesystem::exists(file_path)) {
          if (creation) {
            return boost::make_unexpected(make_error_code(VaultErrors::data_already_exists));
          }
          current_disk_usage_.data -= boost::filesystem::file_size(file_path);
        }
        return Write(file_path, versions.Serialise());
      });
}

Expected<void> DiskBackend::Write(
    const boost::filesystem::path& path, const NonEmptyString& value) {
  const auto value_size = value.string().size();

  if (!HasDiskSpace(value_size)) {
    LOG(kError) << "Out of space";
    return boost::make_unexpected(make_error_code(CommonErrors::cannot_exceed_limit));
  }
  if (!WriteFile(path, value.string())) {
    LOG(kError) << "Write failed";
    return boost::make_unexpected(make_error_code(CommonErrors::filesystem_io_error));
  }

  current_disk_usage_.data += value_size;
  return Expected<void>{boost::expect};
}

}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
