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

#ifndef MAIDSAFE_NFS_CLIENT_FAKE_STORE_H_
#define MAIDSAFE_NFS_CLIENT_FAKE_STORE_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "boost/filesystem/path.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#include "boost/thread/future.hpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/log.h"
#include "maidsafe/common/types.h"
#include "maidsafe/common/data_types/data_name_variant.h"
#include "maidsafe/common/data_types/structured_data_versions.h"
#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace nfs {

class FakeStore {
 public:
  typedef boost::future<std::vector<StructuredDataVersions::VersionName>> VersionNamesFuture;

  FakeStore(const boost::filesystem::path& disk_path, DiskUsage max_disk_usage);
  ~FakeStore();

  template <typename DataName>
  boost::future<typename DataName::data_type> Get(
      const DataName& data_name,
      const std::chrono::steady_clock::duration& timeout = std::chrono::seconds(10));

  template <typename Data>
  boost::future<void> Put(const Data& data);

  template <typename DataName>
  boost::future<void> Delete(const DataName& data_name);

  void IncrementReferenceCount(const std::vector<ImmutableData::Name>& data_names);
  void DecrementReferenceCount(const std::vector<ImmutableData::Name>& data_names);

  template <typename DataName>
  boost::future<void> CreateVersionTree(const DataName& data_name,
                         const StructuredDataVersions::VersionName& version_name,
                         uint32_t max_versions, uint32_t max_branches,
                         const std::chrono::steady_clock::duration& timeout =
                             std::chrono::seconds(10));

  template <typename DataName>
  VersionNamesFuture GetVersions(const DataName& data_name,
                                 const std::chrono::steady_clock::duration& timeout =
                                     std::chrono::seconds(10));

  template <typename DataName>
  VersionNamesFuture GetBranch(const DataName& data_name,
                               const StructuredDataVersions::VersionName& branch_tip,
                               const std::chrono::steady_clock::duration& timeout =
                                   std::chrono::seconds(10));

  template <typename DataName>
  boost::future<void> PutVersion(
      const DataName& data_name,
      const StructuredDataVersions::VersionName& old_version_name,
      const StructuredDataVersions::VersionName& new_version_name);

  template <typename DataName>
  boost::future<void> DeleteBranchUntilFork(
      const DataName& data_name,
      const StructuredDataVersions::VersionName& branch_tip);

  void SetMaxDiskUsage(DiskUsage max_disk_usage);

  DiskUsage GetMaxDiskUsage() const;
  DiskUsage GetCurrentDiskUsage() const;

 private:
  typedef DataNameVariant KeyType;
  typedef boost::promise<std::vector<StructuredDataVersions::VersionName>> VersionNamesPromise;

  FakeStore(const FakeStore&);
  FakeStore(FakeStore&&);
  FakeStore& operator=(FakeStore);

  NonEmptyString DoGet(const KeyType& key) const;
  void DoPut(const KeyType& key, const NonEmptyString& value);
  void DoDelete(const KeyType& key);
  void DoIncrement(const std::vector<ImmutableData::Name>& data_names);
  void DoDecrement(const std::vector<ImmutableData::Name>& data_names);

  boost::filesystem::path GetFilePath(const KeyType& key) const;
  bool HasDiskSpace(uint64_t required_space) const;
  boost::filesystem::path KeyToFilePath(const KeyType& key, bool create_if_missing) const;
  uint32_t GetReferenceCount(const boost::filesystem::path& path) const;
  void Write(const boost::filesystem::path& path, const NonEmptyString& value,
             const uintmax_t& size);
  uintmax_t Remove(const boost::filesystem::path& path);
  uintmax_t Rename(const boost::filesystem::path& old_path,
                   const boost::filesystem::path& new_path);

  std::unique_ptr<StructuredDataVersions> ReadVersions(const KeyType& key) const;
  void WriteVersions(
      const KeyType& key, const StructuredDataVersions& versions, const bool creation);

  AsioService asio_service_;
  const boost::filesystem::path kDiskPath_;
  DiskUsage max_disk_usage_, current_disk_usage_;
  const uint32_t kDepth_;
  mutable std::mutex mutex_;
  GetIdentityVisitor get_identity_visitor_;
};

// ==================== Implementation =============================================================
template <typename DataName>
boost::future<typename DataName::data_type> FakeStore::Get(
    const DataName& data_name,
    const std::chrono::steady_clock::duration& /*timeout*/) {
  LOG(kVerbose) << "Getting: " << HexSubstr(data_name.value);
  auto promise(std::make_shared<boost::promise<typename DataName::data_type>>());
  auto async_future(boost::async([=] {
    try {
      auto result(this->DoGet(KeyType(data_name)));
      typename DataName::data_type data(data_name,
                                        typename DataName::data_type::serialised_type(result));
      LOG(kVerbose) << "Got: " << HexSubstr(data_name.value) << "  " << HexSubstr(result);
      promise->set_value(data);
    }
    catch (const std::exception& e) {
      LOG(kError) << boost::diagnostic_information(e);
      promise->set_exception(boost::current_exception());
    }
  }));
  static_cast<void>(async_future);
  return promise->get_future();
}

template <typename Data>
boost::future<void> FakeStore::Put(const Data& data) {
  LOG(kVerbose) << "Putting: " << HexSubstr(data.name().value) << "  "
                << HexSubstr(data.Serialise().data);
  const auto promise(std::make_shared<boost::promise<void>>());
  asio_service_.service().post([this, data, promise] {
    try {
      DoPut(KeyType(data.name()), data.Serialise());
      promise->set_value();
    }
    catch (const std::exception& e) {
      LOG(kWarning) << "Put failed: " << boost::diagnostic_information(e);
      promise->set_exception(boost::current_exception());
    }
  });
  return promise->get_future();
}

template <typename DataName>
boost::future<void> FakeStore::Delete(const DataName& data_name) {
  LOG(kVerbose) << "Deleting: " << HexSubstr(data_name.value);
  const auto promise(std::make_shared<boost::promise<void>>());
  asio_service_.service().post([this, data_name, promise] {
    try {
      DoDelete(KeyType(data_name));
      promise->set_value();
    }
    catch (const std::exception& e) {
      LOG(kWarning) << "Delete failed: " << boost::diagnostic_information(e);
      promise->set_exception(boost::current_exception());
    }
  });

  return promise->get_future();
}

template <typename DataName>
boost::future<void> FakeStore::CreateVersionTree(const DataName& data_name,
                       const StructuredDataVersions::VersionName& version_name,
                       uint32_t max_versions, uint32_t max_branches,
                       const std::chrono::steady_clock::duration& /*timeout*/) {
  LOG(kVerbose) << "Create Version " << HexSubstr(data_name.value);
  auto promise(std::make_shared<boost::promise<void>>());
  try {
    KeyType key(data_name);
    StructuredDataVersions versions(max_versions, max_branches);
    std::lock_guard<std::mutex> lock(this->mutex_);
    versions.Put(StructuredDataVersions::VersionName(), version_name);
    WriteVersions(key, versions, true);
    promise->set_value();
  }
  catch (const std::exception& e) {
    LOG(kError) << "Failed creating versions: " << e.what();
    promise->set_exception(boost::current_exception());
  }
  return promise->get_future();
}

template <typename DataName>
FakeStore::VersionNamesFuture FakeStore::GetVersions(
    const DataName& data_name, const std::chrono::steady_clock::duration& /*timeout*/) {
  LOG(kVerbose) << "Getting versions: " << HexSubstr(data_name.value);
  auto promise(std::make_shared<VersionNamesPromise>());
  auto async_future(boost::async([=] {
    try {
      KeyType key(data_name);
      std::lock_guard<std::mutex> lock(this->mutex_);
      auto versions(this->ReadVersions(key));
      if (!versions)
        BOOST_THROW_EXCEPTION(MakeError(VaultErrors::no_such_account));
      promise->set_value(versions->Get());
    }
    catch (const std::exception& e) {
      LOG(kError) << "Failed getting versions: " << boost::diagnostic_information(e);
      promise->set_exception(boost::current_exception());
    }
  }));
  static_cast<void>(async_future);
  return promise->get_future();
}

template <typename DataName>
FakeStore::VersionNamesFuture FakeStore::GetBranch(
    const DataName& data_name, const StructuredDataVersions::VersionName& branch_tip,
    const std::chrono::steady_clock::duration& /*timeout*/) {
  LOG(kVerbose) << "Getting branch: " << HexSubstr(data_name.value) << ".  Tip: "
                << branch_tip.index << "-" << HexSubstr(branch_tip.id.value);
  auto promise(std::make_shared<VersionNamesPromise>());
  auto async_future(boost::async([=] {
    try {
      KeyType key(data_name);
      std::lock_guard<std::mutex> lock(this->mutex_);
      auto versions(this->ReadVersions(key));
      if (!versions)
        BOOST_THROW_EXCEPTION(MakeError(CommonErrors::no_such_element));
      promise->set_value(versions->GetBranch(branch_tip));
    }
    catch (const std::exception& e) {
      LOG(kError) << "Failed getting branch: " << boost::diagnostic_information(e);
      promise->set_exception(boost::current_exception());
    }
  }));
  static_cast<void>(async_future);
  return promise->get_future();
}

template <typename DataName>
boost::future<void> FakeStore::PutVersion(
    const DataName& data_name,
    const StructuredDataVersions::VersionName& old_version_name,
    const StructuredDataVersions::VersionName& new_version_name) {
  LOG(kVerbose) << "Putting version: " << HexSubstr(data_name.value) << ".  Old: "
                << (old_version_name.id.value.IsInitialised() ?
                       (std::to_string(old_version_name.index) + "-" +
                           HexSubstr(old_version_name.id.value)) : "N/A") << "  New: "
                << new_version_name.index << "-" << HexSubstr(new_version_name.id.value);
  try {
    KeyType key(data_name);
    std::lock_guard<std::mutex> lock(mutex_);
    auto versions(ReadVersions(key));
    if (!versions) {
      LOG(kError) << "Failed to read versions";
      return boost::make_exceptional_future<void>(MakeError(VaultErrors::no_such_account));
    }
    versions->Put(old_version_name, new_version_name);
    WriteVersions(key, *versions, false);
  }
  catch (const std::exception& e) {
    LOG(kError) << "Failed putting version: " << boost::diagnostic_information(e);
    return boost::make_exceptional_future<void>(boost::current_exception());
  }

  return boost::make_ready_future();
}

template <typename DataName>
boost::future<void> FakeStore::DeleteBranchUntilFork(
    const DataName& data_name,
    const StructuredDataVersions::VersionName& branch_tip) {
  LOG(kVerbose) << "Deleting branch: " << HexSubstr(data_name.value) << ".  Tip: "
                << branch_tip.index << "-" << HexSubstr(branch_tip.id.value);
  try {
    KeyType key(data_name);
    std::lock_guard<std::mutex> lock(mutex_);
    auto versions(ReadVersions(key));
    if (!versions) {
      return boost::make_exceptional_future<void>(MakeError(CommonErrors::no_such_element));
    }
    versions->DeleteBranchUntilFork(branch_tip);
    WriteVersions(key, *versions, false);
  }
  catch (const std::exception& e) {
    LOG(kError) << "Failed deleting branch: " << boost::diagnostic_information(e);
    return boost::make_exceptional_future<void>(boost::current_exception());
  }

  return boost::make_ready_future();
}

}  // namespace nfs

}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_CLIENT_FAKE_STORE_H_
