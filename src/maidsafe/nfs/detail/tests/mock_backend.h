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
#ifndef MAIDSAFE_NFS_DETAIL_TESTS_MOCK_BACKEND_H_
#define MAIDSAFE_NFS_DETAIL_TESTS_MOCK_BACKEND_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "gmock/gmock.h"

#include "maidsafe/common/data_types/immutable_data.h"
#include "maidsafe/common/test.h"
#include "maidsafe/nfs/container_version.h"
#include "maidsafe/nfs/detail/container_id.h"
#include "maidsafe/nfs/detail/network.h"

namespace maidsafe {
namespace nfs {
namespace detail {
namespace test {

class MockBackend : public Network {
 public:
  explicit MockBackend(std::shared_ptr<Network> real);
  virtual ~MockBackend();

  // Set mock interface behavior to invoke real version
  void SetDefaultDoCreateSDV();
  void SetDefaultDoPutSDVVersion();
  void SetDefaultDoGetBranches();
  void SetDefaultDoGetBranchVersions();
  void SetDefaultDoPutChunk();
  void SetDefaultDoGetChunk();
  void SetDefaults();

  MOCK_METHOD5(
      DoCreateSDV,
      void(
          std::function<void(Expected<void>)> callback,
          const ContainerId& container_id,
          const ContainerVersion& initial_version,
          std::uint32_t max_versions,
          std::uint32_t max_branches));
  MOCK_METHOD4(
      DoPutSDVVersion,
      void(
          std::function<void(Expected<void>)> callback,
          const ContainerId& container_id,
          const ContainerVersion& old_version,
          const ContainerVersion& new_version));
  MOCK_METHOD2(
      DoGetBranches,
      void(
          std::function<void(Expected<std::vector<ContainerVersion>>)> callback,
          const ContainerId& container_id));
  MOCK_METHOD3(
      DoGetBranchVersions,
      void(
          std::function<void(Expected<std::vector<ContainerVersion>>)> callback,
          const ContainerId& container_id,
          const ContainerVersion& tip));
  MOCK_METHOD2(
      DoPutChunk,
      void(std::function<void(Expected<void>)> callback, const ImmutableData& data));
  MOCK_METHOD2(
      DoGetChunk,
      void(
          std::function<void(Expected<ImmutableData>)> callback,
          const ImmutableData::NameAndTypeId& name));

 private:
  MockBackend(const MockBackend&) = delete;
  MockBackend(MockBackend&&) = delete;

  MockBackend& operator=(const MockBackend&) = delete;
  MockBackend& operator=(MockBackend&&) = delete;

 private:
  const std::shared_ptr<Network> real_;
};

}  // namespace test
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_TESTS_MOCK_BACKEND_H_
