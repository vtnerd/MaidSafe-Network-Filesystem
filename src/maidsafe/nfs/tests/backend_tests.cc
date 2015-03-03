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
#include "maidsafe/common/utils.h"
#include "maidsafe/nfs/detail/container_info.h"
#include "maidsafe/nfs/tests/network_fixture.h"

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

  static ContainerVersion MakeRootContainerVersion() {
    return MakeContainerVersionRoot(ImmutableData::Name{Identity{RandomString(64)}});
  }
  static ContainerVersion MakeChildContainerVersion(const ContainerVersion& parent) {
    return MakeContainerVersionChild(parent, ImmutableData::Name{Identity{RandomString(64)}});
  }

  static ImmutableData MakeChunk() {
    return ImmutableData{NonEmptyString{RandomString(100)}};
  }
};
}  // namespace

TEST_F(BackendTest, BEH_CreateSDV) {
  const ContainerInfo container_key{};
  const ContainerVersion container_version{MakeRootContainerVersion()};

  EXPECT_CALL(
      GetNetworkMock(),
      DoCreateSDV(container_key.GetId(), container_version, Network::GetMaxVersions(), 1))
    .Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key.GetId())).Times(2);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(container_key.GetId(), container_version))
    .Times(1);
  {
    const auto versions = Network::GetSDVVersions(
        network(), container_key.GetId(), asio::use_future).get();
    ASSERT_FALSE(versions.valid());
    EXPECT_EQ(make_error_code(VaultErrors::no_such_account), versions.error());
  }
  auto sdv = Network::CreateSDV(
      network(), container_key.GetId(), container_version, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  const auto versions = Network::GetSDVVersions(
      network(), container_key.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  ASSERT_EQ(1u, versions->size());
  EXPECT_EQ(container_version, versions->front());
}

TEST_F(BackendTest, BEH_ExistingSDVFailure) {
  const ContainerInfo container_key{};
  const ContainerVersion container_version{MakeRootContainerVersion()};

  EXPECT_CALL(
      GetNetworkMock(),
      DoCreateSDV(container_key.GetId(), container_version, Network::GetMaxVersions(), 1))
    .Times(2);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key.GetId())).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(container_key.GetId(), container_version))
    .Times(1);

  auto sdv = Network::CreateSDV(
      network(), container_key.GetId(), container_version, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = Network::CreateSDV(
      network(), container_key.GetId(), container_version, asio::use_future).get();
  ASSERT_FALSE(sdv.valid());
  EXPECT_EQ(make_error_code(VaultErrors::data_already_exists), sdv.error());

  const auto versions = Network::GetSDVVersions(
      network(), container_key.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  ASSERT_EQ(1u, versions->size());
  EXPECT_EQ(container_version, versions->front());
}

TEST_F(BackendTest, BEH_UpdateExistingSDV) {
  const ContainerInfo container_key{};
  const ContainerVersion container_version1{MakeRootContainerVersion()};
  const ContainerVersion container_version2{MakeChildContainerVersion(container_version1)};

  EXPECT_CALL(
      GetNetworkMock(),
      DoCreateSDV(container_key.GetId(), container_version1, Network::GetMaxVersions(), 1))
    .Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key.GetId())).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(container_key.GetId(), container_version2))
    .Times(1);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(container_key.GetId(), container_version1,
                                                container_version2)).Times(1);

  auto sdv = Network::CreateSDV(
      network(), container_key.GetId(), container_version1, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = Network::PutSDVVersion(
      network(),
      container_key.GetId(), container_version1, container_version2, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  const auto versions = Network::GetSDVVersions(
      network(), container_key.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  ASSERT_EQ(2u, versions->size());
  EXPECT_EQ(container_version2, (*versions)[0]);
  EXPECT_EQ(container_version1, (*versions)[1]);
}

TEST_F(BackendTest, BEH_PutNonExistingSDVFailure) {
  const ContainerInfo container_key{};
  const ContainerVersion container_version1{MakeRootContainerVersion()};
  const ContainerVersion container_version2{MakeChildContainerVersion(container_version1)};

  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key.GetId())).Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoPutSDVVersion(container_key.GetId(), container_version1, container_version2))
    .Times(1);

  const auto sdv = Network::PutSDVVersion(
      network(),
      container_key.GetId(), container_version1, container_version2, asio::use_future).get();
  ASSERT_FALSE(sdv.valid());
  EXPECT_EQ(make_error_code(VaultErrors::no_such_account), sdv.error());

  const auto versions = Network::GetSDVVersions(
      network(), container_key.GetId(), asio::use_future).get();
  ASSERT_FALSE(versions.valid());
  EXPECT_EQ(make_error_code(VaultErrors::no_such_account), versions.error());
}

TEST_F(BackendTest, BEH_UpdateExistingSDVBranchFailure) {
  const ContainerInfo container_key{};
  const ContainerVersion container_version1{MakeRootContainerVersion()};
  const ContainerVersion container_version2{MakeChildContainerVersion(container_version1)};
  const ContainerVersion container_version3{MakeChildContainerVersion(container_version2)};

  EXPECT_CALL(
      GetNetworkMock(),
      DoCreateSDV(container_key.GetId(), container_version1, Network::GetMaxVersions(), 1))
    .Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key.GetId())).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(container_key.GetId(), container_version2))
    .Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoPutSDVVersion(container_key.GetId(), container_version1, container_version2))
    .Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoPutSDVVersion(container_key.GetId(), container_version1, container_version3))
    .Times(1);

  auto sdv = Network::CreateSDV(
      network(), container_key.GetId(), container_version1, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = Network::PutSDVVersion(
      network(),
      container_key.GetId(), container_version1, container_version2, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = Network::PutSDVVersion(
      network(),
      container_key.GetId(), container_version1, container_version3, asio::use_future).get();
  ASSERT_FALSE(sdv.valid());
  EXPECT_EQ(make_error_code(CommonErrors::cannot_exceed_limit), sdv.error());

  const auto versions =
    Network::GetSDVVersions(network(), container_key.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  ASSERT_EQ(2u, versions->size());
  EXPECT_EQ(container_version2, (*versions)[0]);
  EXPECT_EQ(container_version1, (*versions)[1]);
}

// See MAID-658 for DISABLED_
TEST_F(BackendTest, DISABLED_BEH_UpdateExistingSDVBadRootFailure) {
  const ContainerInfo container_key{};
  const ContainerVersion container_version1{MakeRootContainerVersion()};
  const ContainerVersion container_version2{MakeChildContainerVersion(container_version1)};
  const ContainerVersion container_version3{MakeChildContainerVersion(container_version2)};

  EXPECT_CALL(
      GetNetworkMock(),
      DoCreateSDV(container_key.GetId(), container_version1, Network::GetMaxVersions(), 1))
    .Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key.GetId())).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(container_key.GetId(), container_version1))
    .Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoPutSDVVersion(container_key.GetId(), container_version3, container_version2))
    .Times(1);

  auto sdv = Network::CreateSDV(
      network(), container_key.GetId(), container_version1, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = Network::PutSDVVersion(
      network(),
      container_key.GetId(), container_version3, container_version2, asio::use_future).get();
  ASSERT_FALSE(sdv.valid());
  EXPECT_EQ(make_error_code(CommonErrors::invalid_parameter), sdv.error());

  const auto versions = Network::GetSDVVersions(
      network(), container_key.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  ASSERT_EQ(1u, versions->size());
  EXPECT_EQ(container_version1, versions->front());
}

TEST_F(BackendTest, BEH_UpdateExistingSDVSameTip) {
  const ContainerInfo container_key{};
  const ContainerVersion container_version{MakeRootContainerVersion()};

  EXPECT_CALL(
      GetNetworkMock(),
      DoCreateSDV(container_key.GetId(), container_version, Network::GetMaxVersions(), 1))
    .Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key.GetId())).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(container_key.GetId(), container_version))
    .Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoPutSDVVersion(container_key.GetId(), container_version, container_version))
    .Times(1);

  auto sdv = Network::CreateSDV(
      network(), container_key.GetId(), container_version, asio::use_future).get();
  EXPECT_TRUE(sdv.valid());

  sdv = Network::PutSDVVersion(
      network(),
      container_key.GetId(), container_version, container_version, asio::use_future).get();
  ASSERT_FALSE(sdv.valid());
  EXPECT_EQ(make_error_code(CommonErrors::invalid_parameter), sdv.error());

  const auto versions = Network::GetSDVVersions(
      network(), container_key.GetId(), asio::use_future).get();
  ASSERT_TRUE(versions.valid());
  ASSERT_EQ(1u, versions->size());
  EXPECT_EQ(container_version, versions->front());
}

TEST_F(BackendTest, BEH_TwoSDVs) {
  const ContainerInfo container_key1{};
  const ContainerInfo container_key2{};
  const ContainerVersion container_version1{MakeRootContainerVersion()};
  const ContainerVersion container_version2{MakeChildContainerVersion(container_version1)};

  EXPECT_CALL(
      GetNetworkMock(),
      DoCreateSDV(container_key1.GetId(), container_version1, Network::GetMaxVersions(), 1))
    .Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoCreateSDV(container_key2.GetId(), container_version1, Network::GetMaxVersions(), 1))
    .Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key1.GetId())).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key2.GetId())).Times(1);
  EXPECT_CALL(
      GetNetworkMock(), DoGetBranchVersions(container_key1.GetId(), container_version2))
    .Times(1);
  EXPECT_CALL(
      GetNetworkMock(), DoGetBranchVersions(container_key2.GetId(), container_version2))
    .Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoPutSDVVersion(container_key1.GetId(), container_version1, container_version2))
    .Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoPutSDVVersion(container_key2.GetId(), container_version1, container_version2))
    .Times(1);


  auto sdv1 = Network::CreateSDV(
      network(), container_key1.GetId(), container_version1, asio::use_future);
  auto sdv2 = Network::CreateSDV(
      network(), container_key2.GetId(), container_version1, asio::use_future);

  EXPECT_TRUE(sdv1.get().valid());
  EXPECT_TRUE(sdv2.get().valid());


  sdv1 = Network::PutSDVVersion(
      network(), container_key1.GetId(), container_version1, container_version2, asio::use_future);
  sdv2 = Network::PutSDVVersion(
      network(), container_key2.GetId(), container_version1, container_version2, asio::use_future);

  EXPECT_TRUE(sdv1.get().valid());
  EXPECT_TRUE(sdv2.get().valid());


  auto future_versions1 = Network::GetSDVVersions(
      network(), container_key1.GetId(), asio::use_future);
  auto future_versions2 = Network::GetSDVVersions(
      network(), container_key2.GetId(), asio::use_future);

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

// See MAID-657 for DISABLED_
TEST_F(BackendTest, DISABLED_BEH_GetChunkFailure) {
  const ImmutableData chunk_data{MakeChunk()};
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(chunk_data.name())).Times(1);

  auto get_chunk = Network::GetChunk(network(), chunk_data.name(), asio::use_future).get();
  ASSERT_FALSE(get_chunk.valid());
  EXPECT_EQ(make_error_code(CommonErrors::no_such_element), get_chunk.error());
}

TEST_F(BackendTest, BEH_PutChunk) {
  using ::testing::_;

  const ImmutableData chunk_data{MakeChunk()};

  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(chunk_data.name())).Times(1);

  const auto put_chunk = Network::PutChunk(network(), chunk_data, asio::use_future).get();
  EXPECT_TRUE(put_chunk.valid());

  auto get_chunk = Network::GetChunk(network(), chunk_data.name(), asio::use_future).get();
  ASSERT_TRUE(get_chunk.valid());
  EXPECT_EQ(chunk_data.name(), get_chunk->name());
  EXPECT_EQ(chunk_data.data(), get_chunk->data());
}

TEST_F(BackendTest, BEH_PutChunkTwice) {
  using ::testing::_;

  const ImmutableData chunk_data{MakeChunk()};

  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(2);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(chunk_data.name())).Times(1);

  auto put_chunk1 = Network::PutChunk(network(), chunk_data, asio::use_future);
  auto put_chunk2 = Network::PutChunk(network(), chunk_data, asio::use_future);

  EXPECT_TRUE(put_chunk1.get().valid());
  EXPECT_TRUE(put_chunk2.get().valid());

  const auto get_chunk = Network::GetChunk(network(), chunk_data.name(), asio::use_future).get();
  ASSERT_TRUE(get_chunk.valid());
  EXPECT_EQ(chunk_data.name(), get_chunk->name());
  EXPECT_EQ(chunk_data.data(), get_chunk->data());
}

TEST_F(BackendTest, BEH_InterfaceThrow) {
  using ::testing::_;
  using ::testing::Throw;

  // Make sure every interface function correctly returns errors
  const ContainerInfo container_key{};
  const ContainerVersion container_version{MakeRootContainerVersion()};
  const ImmutableData chunk_data{MakeChunk()};

  const auto test_error = make_error_code(AsymmErrors::invalid_private_key);

  EXPECT_CALL(
      GetNetworkMock(),
      DoCreateSDV(container_key.GetId(), container_version, Network::GetMaxVersions(), 1))
    .Times(1).WillOnce(Throw(test_error));
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key.GetId()))
    .Times(1).WillOnce(Throw(test_error));
  EXPECT_CALL(
      GetNetworkMock(),
      DoPutSDVVersion(container_key.GetId(), container_version, container_version))
    .Times(1).WillOnce(Throw(test_error));
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(1).WillOnce(Throw(test_error));
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(chunk_data.name())).Times(1).WillOnce(Throw(test_error));

  EXPECT_THROW(
      Network::CreateSDV(network(), container_key.GetId(), container_version, asio::use_future),
      std::error_code);
  EXPECT_THROW(
      Network::PutSDVVersion(
          network(), container_key.GetId(), container_version, container_version, asio::use_future),
      std::error_code);
  EXPECT_THROW(
      Network::GetSDVVersions(network(), container_key.GetId(), asio::use_future), std::error_code);
  EXPECT_THROW(Network::PutChunk(network(), chunk_data, asio::use_future), std::error_code);
  EXPECT_THROW(Network::GetChunk(network(), chunk_data.name(), asio::use_future), std::error_code);
}

TEST_F(BackendTest, BEH_InterfaceErrors) {
  using ::testing::_;
  using ::testing::Return;

  // Make sure every interface function correctly returns errors
  const ContainerInfo container_key{};
  const ContainerVersion container_version{MakeRootContainerVersion()};
  const ImmutableData chunk_data{MakeChunk()};

  const auto test_error = make_error_code(AsymmErrors::invalid_private_key);

  const auto valid_result(std::make_shared<boost::future<std::vector<ContainerVersion>>>());
  *valid_result = boost::make_ready_future(std::vector<ContainerVersion>{container_version});

  EXPECT_CALL(
      GetNetworkMock(),
      DoCreateSDV(container_key.GetId(), container_version, Network::GetMaxVersions(), 1))
    .Times(1).WillOnce(Return(MakeFutureError<void>(test_error)));
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container_key.GetId()))
    .Times(2)
    .WillOnce(Return(MakeFutureError<std::vector<ContainerVersion>>(test_error)))
    .WillOnce(Return(valid_result));
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(container_key.GetId(), container_version))
    .Times(1).WillOnce(Return(MakeFutureError<std::vector<ContainerVersion>>(test_error)));
  EXPECT_CALL(
      GetNetworkMock(),
      DoPutSDVVersion(container_key.GetId(), container_version, container_version))
    .Times(1).WillOnce(Return(MakeFutureError<void>((test_error))));
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_))
    .Times(1).WillOnce(Return(MakeFutureError<void>(test_error)));
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(chunk_data.name()))
    .Times(1).WillOnce(Return(MakeFutureError<ImmutableData>(test_error)));

  auto void_return =
    Network::CreateSDV(network(), container_key.GetId(), container_version, asio::use_future).get();
  ASSERT_FALSE(void_return.valid());
  EXPECT_EQ(test_error, void_return.error());

  void_return = Network::PutSDVVersion(
      network(),
      container_key.GetId(), container_version, container_version, asio::use_future).get();
  ASSERT_FALSE(void_return.valid());
  EXPECT_EQ(test_error, void_return.error());

  // error on first operation
  auto versions = Network::GetSDVVersions(network(), container_key.GetId(), asio::use_future).get();
  ASSERT_FALSE(versions.valid());
  EXPECT_EQ(test_error, versions.error());

  // error on second operation
  versions = Network::GetSDVVersions(network(), container_key.GetId(), asio::use_future).get();
  ASSERT_FALSE(versions.valid());
  EXPECT_EQ(test_error, versions.error());

  void_return = Network::PutChunk(network(), chunk_data, asio::use_future).get();
  ASSERT_FALSE(void_return.valid());
  EXPECT_EQ(test_error, versions.error());

  auto chunk = Network::GetChunk(network(), chunk_data.name(), asio::use_future).get();
  ASSERT_FALSE(chunk.valid());
  EXPECT_EQ(test_error, chunk.error());
}

}  // namespace test
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
