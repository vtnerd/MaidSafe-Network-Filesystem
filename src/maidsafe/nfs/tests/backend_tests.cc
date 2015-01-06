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
#include <memory>

#include "asio/use_future.hpp"

#include "maidsafe/common/test.h"
#include "maidsafe/nfs/detail/disk_backend.h"
#include "maidsafe/nfs/detail/container_key.h"

namespace maidsafe {
namespace nfs {
namespace detail {
namespace test {

namespace {

const DiskUsage kDefaultMaxDiskUsage(2000);

class BackendTest : public ::testing::Test {
 protected:
  BackendTest() :
    ::testing::Test(),
    storage_location_(::maidsafe::test::CreateTestPath("MaidSafe_Test_FakeStore")),
    network_(std::make_shared<DiskBackend>(*storage_location_, kDefaultMaxDiskUsage)) {
  }

  static ContainerVersion MakeContainerVersion() {
    return ContainerVersion{
      0, ImmutableData::Name{Identity{RandomString(64)}}};
  }

  static ImmutableData MakeChunk() {
    return ImmutableData{NonEmptyString{RandomString(100)}};
  }

  const std::shared_ptr<DiskBackend>& network() const { return network_; }

 private:
  const ::maidsafe::test::TestPath storage_location_;
  const std::shared_ptr<DiskBackend> network_;
};
}  // namespace

TEST_F(BackendTest, BEH_CreateSDV) {
  const ContainerKey container_key{};
  const ContainerVersion container_version{MakeContainerVersion()};
  {
    const auto versions =
      NetworkInterface::GetSDVVersions(network(), container_key.GetId(), asio::use_future).get();
    ASSERT_FALSE(versions.valid());
    EXPECT_EQ(make_error_code(CommonErrors::no_such_element), versions.error());
  }
  auto sdv = network()->CreateSDV(container_key.GetId(), container_version, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  const auto versions =
    NetworkInterface::GetSDVVersions(network(), container_key.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  ASSERT_EQ(1u, versions->size());
  EXPECT_EQ(container_version, versions->front());
}

TEST_F(BackendTest, BEH_ExistingSDVFailure) {
  const ContainerKey container_key{};
  const ContainerVersion container_version{MakeContainerVersion()};

  auto sdv = network()->CreateSDV(container_key.GetId(), container_version, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = network()->CreateSDV(container_key.GetId(), container_version, asio::use_future).get();
  ASSERT_FALSE(sdv.valid());
  EXPECT_EQ(make_error_code(VaultErrors::data_already_exists), sdv.error());

  const auto versions =
    NetworkInterface::GetSDVVersions(network(), container_key.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  ASSERT_EQ(1u, versions->size());
  EXPECT_EQ(container_version, versions->front());
}

TEST_F(BackendTest, BEH_UpdateExistingSDV) {
  const ContainerKey container_key{};
  const ContainerVersion container_version1{MakeContainerVersion()};
  const ContainerVersion container_version2{MakeContainerVersion()};

  auto sdv = network()->CreateSDV(container_key.GetId(), container_version1, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = network()->PutSDVVersion(
      container_key.GetId(), container_version1, container_version2, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  const auto versions =
    NetworkInterface::GetSDVVersions(network(), container_key.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  EXPECT_EQ(2u, versions->size());
  EXPECT_EQ(container_version2, (*versions)[0]);
  EXPECT_EQ(container_version1, (*versions)[1]);
}

TEST_F(BackendTest, BEH_UpdateExistingSDVBranchFailure) {
  const ContainerKey container_key{};
  const ContainerVersion container_version1{MakeContainerVersion()};
  const ContainerVersion container_version2{MakeContainerVersion()};
  const ContainerVersion container_version3{MakeContainerVersion()};

  auto sdv = network()->CreateSDV(container_key.GetId(), container_version1, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = network()->PutSDVVersion(
      container_key.GetId(), container_version1, container_version2, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = network()->PutSDVVersion(
      container_key.GetId(), container_version1, container_version3, asio::use_future).get();
  ASSERT_FALSE(sdv.valid());
  EXPECT_EQ(make_error_code(CommonErrors::cannot_exceed_limit), sdv.error());

  const auto versions =
    NetworkInterface::GetSDVVersions(network(), container_key.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  EXPECT_EQ(2u, versions->size());
  EXPECT_EQ(container_version2, (*versions)[0]);
  EXPECT_EQ(container_version1, (*versions)[1]);
}

TEST_F(BackendTest, BEH_UpdateExistingSDVBadRootFailure) {
  const ContainerKey container_key{};
  const ContainerVersion container_version1{MakeContainerVersion()};
  const ContainerVersion container_version2{MakeContainerVersion()};
  const ContainerVersion container_version3{MakeContainerVersion()};

  auto sdv = network()->CreateSDV(container_key.GetId(), container_version1, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = network()->PutSDVVersion(
      container_key.GetId(), container_version3, container_version2, asio::use_future).get();
  ASSERT_FALSE(sdv.valid());
  EXPECT_EQ(make_error_code(CommonErrors::cannot_exceed_limit), sdv.error());

  const auto versions =
    NetworkInterface::GetSDVVersions(network(), container_key.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  ASSERT_EQ(1u, versions->size());
  EXPECT_EQ(container_version1, versions->front());
}

TEST_F(BackendTest, BEH_UpdateExistingSDVSameTip) {
  const ContainerKey container_key{};
  const ContainerVersion container_version{MakeContainerVersion()};

  auto sdv = network()->CreateSDV(container_key.GetId(), container_version, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = network()->PutSDVVersion(
      container_key.GetId(), container_version, container_version, asio::use_future).get();
  ASSERT_FALSE(sdv.valid());
  EXPECT_EQ(make_error_code(CommonErrors::invalid_parameter), sdv.error());

  const auto versions =
    NetworkInterface::GetSDVVersions(network(), container_key.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  ASSERT_EQ(1u, versions->size());
  EXPECT_EQ(container_version, versions->front());
}

TEST_F(BackendTest, BEH_TwoSDVs) {
  const ContainerKey container_key1{};
  const ContainerKey container_key2{};
  const ContainerVersion container_version1{MakeContainerVersion()};
  const ContainerVersion container_version2{MakeContainerVersion()};

  auto sdv = network()->CreateSDV(container_key1.GetId(), container_version1, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = network()->CreateSDV(container_key2.GetId(), container_version1, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());


  sdv = network()->PutSDVVersion(
      container_key1.GetId(), container_version1, container_version2, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = network()->PutSDVVersion(
      container_key2.GetId(), container_version1, container_version2, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());


  auto versions =
    NetworkInterface::GetSDVVersions(network(), container_key1.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  EXPECT_EQ(2u, versions->size());
  EXPECT_EQ(container_version2, (*versions)[0]);
  EXPECT_EQ(container_version1, (*versions)[1]);

  versions =
    NetworkInterface::GetSDVVersions(network(), container_key2.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  EXPECT_EQ(2u, versions->size());
  EXPECT_EQ(container_version2, (*versions)[0]);
  EXPECT_EQ(container_version1, (*versions)[1]);
}

  TEST_F(BackendTest, BEH_PutChunk) {
  const ImmutableData chunk_data{MakeChunk()};
  const auto put_chunk = network()->PutChunk(chunk_data, asio::use_future).get();
  EXPECT_TRUE(put_chunk.valid());

  const auto get_chunk = network()->GetChunk(chunk_data.name(), asio::use_future).get();
  ASSERT_TRUE(get_chunk.valid());
  EXPECT_EQ(chunk_data.name(), get_chunk->name());
  EXPECT_EQ(chunk_data.data(), get_chunk->data());
}

}  // namespace test
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
