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

#include "network_fixture.h"

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/nfs/detail/container_key.h"

namespace maidsafe {
namespace nfs {
namespace detail {
namespace test {

namespace {

class BackendTest : public ::testing::Test, public NetworkFixture {
 protected:
  BackendTest()
    : ::testing::Test(),
      NetworkFixture() {
  }

  static ContainerVersion MakeContainerVersion() {
    return ContainerVersion{
      0, ImmutableData::Name{Identity{RandomString(64)}}};
  }

  static ImmutableData MakeChunk() {
    return ImmutableData{NonEmptyString{RandomString(100)}};
  }
};
}  // namespace

TEST_F(BackendTest, BEH_CreateSDV) {
  const ContainerKey container_key{};
  const ContainerVersion container_version{MakeContainerVersion()};

  EXPECT_CALL(
      GetNetworkMock(), DoCreateSDV(container_key.GetId(), container_version, 100, 1)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key.GetId())).Times(2);
  EXPECT_CALL(
      GetNetworkMock(), DoGetBranchVersions(container_key.GetId(), container_version)).Times(1);
  {
    const auto versions =
      network()->GetSDVVersions(container_key.GetId(), asio::use_future).get();
    ASSERT_FALSE(versions.valid());
    EXPECT_EQ(make_error_code(CommonErrors::no_such_element), versions.error());
  }
  auto sdv = network()->CreateSDV(container_key.GetId(), container_version, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  const auto versions = network()->GetSDVVersions(container_key.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  ASSERT_EQ(1u, versions->size());
  EXPECT_EQ(container_version, versions->front());
}

TEST_F(BackendTest, BEH_ExistingSDVFailure) {
  const ContainerKey container_key{};
  const ContainerVersion container_version{MakeContainerVersion()};

  EXPECT_CALL(
      GetNetworkMock(), DoCreateSDV(container_key.GetId(), container_version, 100, 1)).Times(2);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key.GetId())).Times(1);
  EXPECT_CALL(
      GetNetworkMock(), DoGetBranchVersions(container_key.GetId(), container_version)).Times(1);

  auto sdv = network()->CreateSDV(container_key.GetId(), container_version, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = network()->CreateSDV(container_key.GetId(), container_version, asio::use_future).get();
  ASSERT_FALSE(sdv.valid());
  EXPECT_EQ(make_error_code(VaultErrors::data_already_exists), sdv.error());

  const auto versions = network()->GetSDVVersions(container_key.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  ASSERT_EQ(1u, versions->size());
  EXPECT_EQ(container_version, versions->front());
}

TEST_F(BackendTest, BEH_UpdateExistingSDV) {
  const ContainerKey container_key{};
  const ContainerVersion container_version1{MakeContainerVersion()};
  const ContainerVersion container_version2{MakeContainerVersion()};

  EXPECT_CALL(
      GetNetworkMock(), DoCreateSDV(container_key.GetId(), container_version1, 100, 1)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key.GetId())).Times(1);
  EXPECT_CALL(
      GetNetworkMock(), DoGetBranchVersions(container_key.GetId(), container_version2)).Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoPutSDVVersion(container_key.GetId(), container_version1, container_version2)).Times(1);

  auto sdv = network()->CreateSDV(container_key.GetId(), container_version1, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = network()->PutSDVVersion(
      container_key.GetId(), container_version1, container_version2, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  const auto versions = network()->GetSDVVersions(container_key.GetId(), asio::use_future).get();
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

  EXPECT_CALL(
      GetNetworkMock(), DoCreateSDV(container_key.GetId(), container_version1, 100, 1)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key.GetId())).Times(1);
  EXPECT_CALL(
      GetNetworkMock(), DoGetBranchVersions(container_key.GetId(), container_version2)).Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoPutSDVVersion(container_key.GetId(), container_version1, container_version2)).Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoPutSDVVersion(container_key.GetId(), container_version1, container_version3)).Times(1);

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
    network()->GetSDVVersions(container_key.GetId(), asio::use_future).get();
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

  EXPECT_CALL(
      GetNetworkMock(), DoCreateSDV(container_key.GetId(), container_version1, 100, 1)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key.GetId())).Times(1);
  EXPECT_CALL(
      GetNetworkMock(), DoGetBranchVersions(container_key.GetId(), container_version1)).Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoPutSDVVersion(container_key.GetId(), container_version3, container_version2)).Times(1);

  auto sdv = network()->CreateSDV(container_key.GetId(), container_version1, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = network()->PutSDVVersion(
      container_key.GetId(), container_version3, container_version2, asio::use_future).get();
  ASSERT_FALSE(sdv.valid());
  EXPECT_EQ(make_error_code(CommonErrors::cannot_exceed_limit), sdv.error());

  const auto versions = network()->GetSDVVersions(container_key.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  ASSERT_EQ(1u, versions->size());
  EXPECT_EQ(container_version1, versions->front());
}

TEST_F(BackendTest, BEH_UpdateExistingSDVSameTip) {
  const ContainerKey container_key{};
  const ContainerVersion container_version{MakeContainerVersion()};

  EXPECT_CALL(
      GetNetworkMock(), DoCreateSDV(container_key.GetId(), container_version, 100, 1)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key.GetId())).Times(1);
  EXPECT_CALL(
      GetNetworkMock(), DoGetBranchVersions(container_key.GetId(), container_version)).Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoPutSDVVersion(container_key.GetId(), container_version, container_version)).Times(1);

  auto sdv = network()->CreateSDV(container_key.GetId(), container_version, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = network()->PutSDVVersion(
      container_key.GetId(), container_version, container_version, asio::use_future).get();
  ASSERT_FALSE(sdv.valid());
  EXPECT_EQ(make_error_code(CommonErrors::invalid_parameter), sdv.error());

  const auto versions = network()->GetSDVVersions(container_key.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  ASSERT_EQ(1u, versions->size());
  EXPECT_EQ(container_version, versions->front());
}

TEST_F(BackendTest, BEH_TwoSDVs) {
  const ContainerKey container_key1{};
  const ContainerKey container_key2{};
  const ContainerVersion container_version1{MakeContainerVersion()};
  const ContainerVersion container_version2{MakeContainerVersion()};

  EXPECT_CALL(
      GetNetworkMock(), DoCreateSDV(container_key1.GetId(), container_version1, 100, 1)).Times(1);
  EXPECT_CALL(
      GetNetworkMock(), DoCreateSDV(container_key2.GetId(), container_version1, 100, 1)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key1.GetId())).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key2.GetId())).Times(1);
  EXPECT_CALL(
      GetNetworkMock(), DoGetBranchVersions(container_key1.GetId(), container_version2)).Times(1);
  EXPECT_CALL(
      GetNetworkMock(), DoGetBranchVersions(container_key2.GetId(), container_version2)).Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoPutSDVVersion(container_key1.GetId(), container_version1, container_version2)).Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoPutSDVVersion(container_key2.GetId(), container_version1, container_version2)).Times(1);


  auto sdv1 = network()->CreateSDV(container_key1.GetId(), container_version1, asio::use_future);
  auto sdv2 = network()->CreateSDV(container_key2.GetId(), container_version1, asio::use_future);

  EXPECT_TRUE(sdv1.get().valid());
  EXPECT_TRUE(sdv2.get().valid());


  sdv1 = network()->PutSDVVersion(
      container_key1.GetId(), container_version1, container_version2, asio::use_future);
  sdv2 = network()->PutSDVVersion(
      container_key2.GetId(), container_version1, container_version2, asio::use_future);

  EXPECT_TRUE(sdv1.get().valid());
  EXPECT_TRUE(sdv2.get().valid());


  auto future_versions1 = network()->GetSDVVersions(container_key1.GetId(), asio::use_future);
  auto future_versions2 = network()->GetSDVVersions(container_key2.GetId(), asio::use_future);

  auto versions = future_versions1.get();
  ASSERT_TRUE(versions.valid());
  EXPECT_EQ(2u, versions->size());
  EXPECT_EQ(container_version2, (*versions)[0]);
  EXPECT_EQ(container_version1, (*versions)[1]);

  versions = future_versions2.get();
  ASSERT_TRUE(versions.valid());
  EXPECT_EQ(2u, versions->size());
  EXPECT_EQ(container_version2, (*versions)[0]);
  EXPECT_EQ(container_version1, (*versions)[1]);
}

TEST_F(BackendTest, BEH_PutChunk) {
  using ::testing::_;

  const ImmutableData chunk_data{MakeChunk()};

  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(chunk_data.name())).Times(1);

  const auto put_chunk = network()->PutChunk(chunk_data, asio::use_future).get();
  EXPECT_TRUE(put_chunk.valid());

  const auto get_chunk = network()->GetChunk(chunk_data.name(), asio::use_future).get();
  ASSERT_TRUE(get_chunk.valid());
  EXPECT_EQ(chunk_data.name(), get_chunk->name());
  EXPECT_EQ(chunk_data.data(), get_chunk->data());
}

TEST_F(BackendTest, BEH_PutChunkTwice) {
  using ::testing::_;

  const ImmutableData chunk_data{MakeChunk()};

  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(2);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(chunk_data.name())).Times(1);

  auto put_chunk1 = network()->PutChunk(chunk_data, asio::use_future);
  auto put_chunk2 = network()->PutChunk(chunk_data, asio::use_future);

  EXPECT_TRUE(put_chunk1.get().valid());
  EXPECT_TRUE(put_chunk2.get().valid());

  const auto get_chunk = network()->GetChunk(chunk_data.name(), asio::use_future).get();
  ASSERT_TRUE(get_chunk.valid());
  EXPECT_EQ(chunk_data.name(), get_chunk->name());
  EXPECT_EQ(chunk_data.data(), get_chunk->data());
}

}  // namespace test
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
