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
#include <list>
#include <memory>
#include <type_traits>
#include <utility>

#include "asio/use_future.hpp"
#include "boost/range/algorithm/equal.hpp"

#include "network_fixture.h"

#include "maidsafe/common/error.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/nfs/detail/container_info.h"
#include "maidsafe/nfs/detail/network_data.h"
#include "maidsafe/nfs/local_blob.h"

namespace maidsafe {
namespace nfs {
namespace test {
namespace {
class LocalBlobTest : public detail::test::NetworkFixture, public ::testing::Test {
 protected:
  LocalBlobTest()
    : NetworkFixture(),
      ::testing::Test(),
      container_(std::make_shared<detail::Container>(network(), detail::ContainerInfo{})) {
    using ::testing::_;

    // Start with a blank Container!
    EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(1);
    EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(1);
    detail::Container::PutInstance(
        container(), boost::none, detail::ContainerInstance{}, asio::use_future).get().value();
  }

  std::shared_ptr<detail::Container> MakeTempContainer() const {
    return std::make_shared<detail::Container>(
        container()->network(), container()->parent_info(), container()->container_info());
  }

  const std::shared_ptr<detail::Container>& container() const { return container_; }

  std::string ReadBlobContents(const detail::Blob& blob) const {
    detail::NetworkData data{blob.data_map(), network()};
    std::string contents{};
    contents.resize(data.encryptor().size());
    data.encryptor().Read(&contents[0], contents.size(), 0);
    return contents;
  }

  static std::string ReadLocalBlobContents(LocalBlob& local_blob) {
    local_blob.set_offset(0);
    EXPECT_EQ(0, local_blob.offset());

    std::string data{};
    data.resize(local_blob.size());

    EXPECT_EQ(
        data.size(),
        local_blob.Read(
            asio::buffer(&data[0], data.size()), asio::use_future).get().value());
    EXPECT_EQ(data.size(), local_blob.offset());
    return data;
  }

  static void OverWriteLocalBlob(LocalBlob& local_blob, const std::string& data) {
    local_blob.Truncate(0, asio::use_future).get().value();
    EXPECT_EQ(0, local_blob.offset());

    local_blob.Write(asio::buffer(&data[0], data.size()), asio::use_future).get().value();
    EXPECT_EQ(data.size(), local_blob.offset());
  }

  std::future<Expected<Blob>> Commit(
      std::shared_ptr<detail::Container> container,
      LocalBlob& local_blob,
      const std::string& key,
      const boost::optional<const Blob>& replace) const {
    boost::optional<detail::Blob> actual_replace{};
    if (replace) {
      actual_replace = Blob::Detail::blob(*replace);
    }
    return local_blob.Commit(
        container,
        detail::ContainerKey{container->network().lock(), key},
        std::move(actual_replace),
        asio::use_future);
  }

  std::future<Expected<Blob>> Commit(
      LocalBlob& local_blob,
      const std::string& key,
      const boost::optional<const Blob>& replace) const {
    return Commit(container_, local_blob, key, replace);
  }

  void VerifyBlobContents(const Blob& blob, const std::string& expected_contents) const {
    LocalBlob local_blob{container()->network(), Blob::Detail::blob(blob)};
    EXPECT_EQ(expected_contents, ReadLocalBlobContents(local_blob));
  }

 private:
  const std::shared_ptr<detail::Container> container_;
};
}  // namespace

TEST_F(LocalBlobTest, BEH_ReadWrite) {
  using ::testing::_;
  const auto blob_contents = "the contents of the test blob";

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(_)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(0);

  LocalBlob local_blob{container()->network()};
  OverWriteLocalBlob(local_blob, blob_contents);
  EXPECT_STREQ(blob_contents, ReadLocalBlobContents(local_blob).c_str());

  OverWriteLocalBlob(local_blob, "");
  EXPECT_STREQ("", ReadLocalBlobContents(local_blob).c_str());
}

TEST_F(LocalBlobTest, Commit) {
  using ::testing::_;

  const auto blob_contents = "the contents of the test blob";

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(_)).Times(2);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times(2);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(1);

  LocalBlob local_blob{container()->network()};
  OverWriteLocalBlob(local_blob, blob_contents);
  const Blob blob = Commit(local_blob, "my blob", boost::none).get().value();
  /*  VerifyBlobContents(blob, blob_contents);

  EXPECT_STREQ("my blob", blob.key().c_str());
  EXPECT_EQ(blob.creation_time(), blob.modification_time());
  EXPECT_TRUE(blob.user_meta_data().empty());
  EXPECT_EQ(std::string(blob_contents).size(), blob.size());*/
}

TEST_F(LocalBlobTest, CommitTwoBlobs) {
  using ::testing::_;
  using ::testing::Invoke;

  const auto blob1_key = "\xFF BLOB1\xFF";
  const auto blob2_key = "\xFF BLOB2\xFF";
  const auto blob1_contents = "\xFF BLOB1\xFF DATA";
  const auto blob2_contents = "\xFF BLOB2\xFF DATA";
  std::vector<Blob> blobs{};
  std::atomic<bool> waiting{true};
  boost::promise<void> put_promise{};

  {
    auto shared_future = std::make_shared<boost::future<void>>();
    *shared_future = put_promise.get_future();

    EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(0);
    EXPECT_CALL(GetNetworkMock(), DoGetBranches(_)).Times(5);
    EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times(5);
    EXPECT_CALL(
        GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(3).WillOnce(
            Invoke(
                [&waiting, shared_future]
                (const detail::ContainerId&,
                 const detail::ContainerVersion&,
                 const detail::ContainerVersion&) {
                  waiting = false;
                  return shared_future;
                }));
    EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(3);
    EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(2);
  }
  {
    LocalBlob local_blob1{container()->network()};
    LocalBlob local_blob2{container()->network()};

    OverWriteLocalBlob(local_blob1, blob1_contents);
    OverWriteLocalBlob(local_blob2, blob2_contents);

    // Start commit 1, then pause it, and do commit 2. Commit 1 should retry and succeed
    auto commit_future = Commit(local_blob1, blob1_key, boost::none);
    while (waiting);
    blobs.push_back(Commit(local_blob2, blob2_key, boost::none).get().value());
    put_promise.set_exception(MakeError(CommonErrors::cannot_exceed_limit));
    blobs.push_back(commit_future.get().value());
  }
  VerifyBlobContents(blobs[1], blob1_contents);
  VerifyBlobContents(blobs[0], blob2_contents);

  EXPECT_STREQ(blob1_key, blobs[1].key().c_str());
  EXPECT_STREQ(blob2_key, blobs[0].key().c_str());

  EXPECT_EQ(blobs[0].creation_time(), blobs[0].modification_time());
  EXPECT_EQ(blobs[1].creation_time(), blobs[1].modification_time());
  EXPECT_LT(blobs[0].creation_time(), blobs[1].creation_time());

  EXPECT_TRUE(blobs[0].user_meta_data().empty());
  EXPECT_TRUE(blobs[1].user_meta_data().empty());

  EXPECT_EQ(std::string(blob1_contents).size(), blobs[1].size());
  EXPECT_EQ(std::string(blob2_contents).size(), blobs[0].size());
}

TEST_F(LocalBlobTest, BadBlobVersion) {
  using ::testing::_;

  const auto blob_contents = "the contents of the test blob";
  std::vector<Blob> blobs{};

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(_)).Times(5);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times(5);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(2);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(2);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(2);

  {
    LocalBlob local_blob{container()->network()};
    OverWriteLocalBlob(local_blob, blob_contents);
    blobs.push_back(Commit(local_blob, "my blob", boost::none).get().value());

    OverWriteLocalBlob(local_blob, "");
    blobs.push_back(Commit(local_blob, "my blob", blobs.back()).get().value());

    EXPECT_STREQ("my blob", blobs[1].key().c_str());
    EXPECT_STREQ("my blob", blobs[0].key().c_str());

    EXPECT_EQ(blobs[0].creation_time(), blobs[0].modification_time());
    EXPECT_EQ(blobs[0].creation_time(), blobs[1].creation_time());
    EXPECT_LT(blobs[1].creation_time(), blobs[1].modification_time());

    EXPECT_TRUE(blobs[0].user_meta_data().empty());
    EXPECT_TRUE(blobs[1].user_meta_data().empty());

    EXPECT_EQ(std::string(blob_contents).size(), blobs[0].size());
    EXPECT_EQ(0u, blobs[1].size());
  }
  {
    LocalBlob local_blob{container()->network()};

    OverWriteLocalBlob(local_blob, blob_contents);
    const auto commit_result = Commit(local_blob, "my blob", blobs.front()).get();
    ASSERT_FALSE(commit_result);
    EXPECT_EQ(NfsErrors::bad_modify_version, commit_result.error())
      << commit_result.error().message();
  }
  {
    LocalBlob local_blob{container()->network()};

    OverWriteLocalBlob(local_blob, blob_contents);
    const auto commit_result =
      Commit(MakeTempContainer(), local_blob, "my blob", blobs.front()).get();
    ASSERT_FALSE(commit_result);
    EXPECT_EQ(NfsErrors::bad_modify_version, commit_result.error())
      << commit_result.error().message();
  }
  {
    LocalBlob local_blob{container()->network()};

    OverWriteLocalBlob(local_blob, blob_contents);
    const auto commit_result =
      Commit(MakeTempContainer(), local_blob, "no blob", blobs.front()).get();
    ASSERT_FALSE(commit_result);
    EXPECT_EQ(CommonErrors::no_such_element, commit_result.error())
      << commit_result.error().message();
  }
}

TEST_F(LocalBlobTest, ExistingBlob) {
  using ::testing::_;

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(_)).Times(3);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times(3);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(1);

  {
    LocalBlob local_blob{container()->network()};
    Commit(local_blob, "my blob", boost::none).get().value();
  }
  {
    LocalBlob local_blob{container()->network()};
    const auto commit_result =
      Commit(local_blob, "my blob", boost::none).get();

    ASSERT_FALSE(commit_result.valid());
    EXPECT_EQ(NfsErrors::bad_modify_version, commit_result.error())
      << commit_result.error().message();
  }
  {
    LocalBlob local_blob{container()->network()};
    const auto commit_result =
      Commit(MakeTempContainer(), local_blob, "my blob", boost::none).get();

    ASSERT_FALSE(commit_result.valid());
    EXPECT_EQ(NfsErrors::bad_modify_version, commit_result.error())
      << commit_result.error().message();
  }
}

TEST_F(LocalBlobTest, ExistingContainer) {
  using ::testing::_;

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(_)).Times(3);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times(3);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(1);

  detail::Container::PutInstance(
      container(),
      detail::Container::GetVersions(container(), asio::use_future).get().value().front(),
      detail::ContainerInstance{
        detail::ContainerInstance::Entry{
          detail::ContainerKey{network(), "KEY!"}, detail::ContainerInfo{}
        }
      },
      asio::use_future).get().value();
  {
    LocalBlob local_blob{container()->network()};
    const auto commit_result =
      Commit(local_blob, "KEY!", boost::none).get();
    ASSERT_FALSE(commit_result.valid());
    EXPECT_EQ(CommonErrors::invalid_conversion, commit_result.error())
      << commit_result.error().message();
  }
  {
    LocalBlob local_blob{container()->network()};
    const auto commit_result =
      Commit(MakeTempContainer(), local_blob, "KEY!", boost::none).get();
    ASSERT_FALSE(commit_result.valid());
    EXPECT_EQ(CommonErrors::invalid_conversion, commit_result.error())
      << commit_result.error().message();
  }
}

TEST_F(LocalBlobTest, SetMetadata) {
  using ::testing::_;

  const auto meta_data_contents = "my meta data";

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(_)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(0);

  std::vector<Blob> blobs{};

  {
    LocalBlob local_blob{container()->network()};

    EXPECT_TRUE(local_blob.user_meta_data().empty());
    local_blob.set_user_meta_data(meta_data_contents).value();
    EXPECT_STREQ(meta_data_contents, local_blob.user_meta_data().c_str());
    blobs.push_back(Commit(local_blob, "the test blob", boost::none).get().value());
  }
  EXPECT_STREQ(meta_data_contents, blobs.back().user_meta_data().c_str());
  {
    LocalBlob local_blob{container()->network(), Blob::Detail::blob(blobs.back())};
    EXPECT_STREQ(meta_data_contents, local_blob.user_meta_data().c_str());
    EXPECT_TRUE(ReadLocalBlobContents(local_blob).empty());
  }
}

TEST_F(LocalBlobTest, MetaDataFailure) {
  using ::testing::_;

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(_)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(0);

  std::string user{};
  user.resize(Bytes(KiloBytes(64)).count());

  LocalBlob local_blob{container()->network()};

  EXPECT_TRUE(local_blob.set_user_meta_data(user).valid());
  user.resize(user.size() + 1);
  {
    const auto set_result = local_blob.set_user_meta_data(user);
    ASSERT_FALSE(set_result.valid());
    EXPECT_EQ(
        CommonErrors::cannot_exceed_limit, set_result.error()) << set_result.error().message();
  }
  user.resize(user.size() - 1);
  EXPECT_EQ(user, local_blob.user_meta_data());
}

}  // namespace test
}  // namespace nfs
}  // namespace maidsafe
