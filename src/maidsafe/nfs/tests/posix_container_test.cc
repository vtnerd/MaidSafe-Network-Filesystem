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
#include <list>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "asio/use_future.hpp"
#include "boost/range/adaptor/reversed.hpp"
#include "boost/range/adaptor/transformed.hpp"
#include "boost/range/algorithm/equal.hpp"
#include "boost/range/algorithm/sort.hpp"
#include "boost/range/counting_range.hpp"
#include "boost/fusion/adapted/std_tuple.hpp"
#include "boost/spirit/include/karma_char.hpp"
#include "boost/spirit/include/karma_eol.hpp"
#include "boost/spirit/include/karma_format.hpp"
#include "boost/spirit/include/karma_list.hpp"
#include "boost/spirit/include/karma_right_alignment.hpp"
#include "boost/spirit/include/karma_string.hpp"
#include "boost/spirit/include/karma_sequence.hpp"

#include "network_fixture.h"

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/nfs/posix_container.h"
#include "maidsafe/nfs/detail/container.h"
#include "maidsafe/nfs/detail/container_info.h"
#include "maidsafe/nfs/sort_functions.h"
#include "maidsafe/nfs/transform_functions.h"

namespace maidsafe {
namespace nfs {
namespace test {
namespace {
PosixContainer MakeContainer(detail::test::NetworkFixture& fixture) {
  using ::testing::_;

  auto detail_container =
    std::make_shared<detail::Container>(fixture.network(), detail::ContainerInfo{});

  // Start with a blank Container!
  EXPECT_CALL(fixture.GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(1);
  EXPECT_CALL(fixture.GetNetworkMock(), DoPutChunk(_)).Times(1);
  detail::Container::PutInstance(
      detail_container, boost::none, detail::ContainerInstance{}, asio::use_future).get().value();

  return PosixContainer{detail_container};
}

class PosixContainerTest : public ::testing::Test, public detail::test::NetworkFixture {
 protected:
  PosixContainerTest()
    : ::testing::Test(),
      NetworkFixture(),
      container_(MakeContainer(*this)) {
  }

  const PosixContainer& container() { return container_; }

  static void WriteBlobContents(LocalBlob& blob, const std::string& data) {
    blob.Truncate(0, asio::use_future).get().value();
    blob.Write(asio::buffer(&data[0], data.size()), asio::use_future).get().value();
    EXPECT_EQ(data.size(), blob.offset());
  }

  static std::string ReadBlobContents(LocalBlob& blob) {
    std::string contents{};
    contents.resize(blob.size());
    EXPECT_EQ(
        contents.size(),
        blob.Read(asio::buffer(&contents[0], contents.size()), asio::use_future).get().value());
    EXPECT_EQ(contents.size(), blob.offset());
    return contents;
  }

 private:
  const PosixContainer container_;
};
}  // namespace

TEST_F(PosixContainerTest, BEH_WriteBlob) {
  using ::testing::_;

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(_)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(7);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(6);

  const auto random_data = RandomString(Bytes(MebiBytes(5) + KiloBytes(10)).count());
  const std::string key("\xFF KEY \xFF");

  std::vector<Blob> blobs{};
  {
    LocalBlob local_blob{container().CreateLocalBlob()};
    WriteBlobContents(local_blob, random_data);

    blobs.push_back(
        container().Write(
            local_blob, key, ModifyBlobVersion::Create(), asio::use_future).get().value());
  }
  EXPECT_EQ(key, blobs[0].key());
  EXPECT_NE(BlobVersion{}, blobs[0].version());
  EXPECT_EQ(blobs[0].creation_time(), blobs[0].modification_time());
  EXPECT_EQ(random_data.size(), blobs[0].size());
  EXPECT_TRUE(blobs[0].user_meta_data().empty());
  EXPECT_FALSE(blobs[0].data().valid());

  EXPECT_EQ(
      blobs,
      container().GetBlobs(RetrieveContainerVersion::Latest(), asio::use_future).get().value());
  {
    LocalBlob local_blob{
      container().OpenBlob(key, blobs[0].version(), asio::use_future).get().value()};
    EXPECT_EQ(random_data, ReadBlobContents(local_blob));
  }
}

TEST_F(PosixContainerTest, BEH_CopyBlob) {
  using ::testing::_;

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(_)).Times(3);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times(3);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(3);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(9);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(0);

  const auto random_data = RandomString(Bytes(MebiBytes(4) + KiloBytes(100)).count());

  std::vector<Blob> blobs{};
  {
    LocalBlob local_blob{container().CreateLocalBlob()};
    WriteBlobContents(local_blob, random_data);

    blobs.push_back(
        container().Write(
            local_blob, "key<-->", ModifyBlobVersion::Create(), asio::use_future).get().value());
  }
  {
    auto child_container =
      container().OpenContainer(
          "child container", ModifyContainerVersion::Create(), asio::use_future).get().value();
    blobs.push_back(
        child_container.Copy(
            blobs.back(),
            "second key!",
            ModifyBlobVersion::Create(),
            asio::use_future)
        .get().value());
  }
  EXPECT_NE(blobs[0], blobs[1]);
  EXPECT_NE(blobs[0].version(), blobs[1].version());
  
  EXPECT_STREQ("key<-->", blobs[0].key().c_str());
  EXPECT_STREQ("second key!", blobs[1].key().c_str());

  EXPECT_EQ(blobs[0].creation_time(), blobs[0].modification_time());
  EXPECT_EQ(blobs[1].creation_time(), blobs[1].modification_time());
  EXPECT_LT(blobs[0].creation_time(), blobs[1].creation_time());

  EXPECT_EQ(random_data.size(), blobs[0].size());
  EXPECT_EQ(blobs[0].size(), blobs[1].size());
  EXPECT_EQ(blobs[0].user_meta_data(), blobs[1].user_meta_data());
  EXPECT_EQ(
      static_cast<detail::Blob>(blobs[0]).data_map(),
      static_cast<detail::Blob>(blobs[1]).data_map());

  EXPECT_FALSE(blobs[0].data().valid());
  EXPECT_FALSE(blobs[1].data().valid());
}

TEST_F(PosixContainerTest, BEH_MultipleBlobs) {
  namespace adapt = boost::adaptors;
  namespace karma = boost::spirit::karma;
  namespace range = boost::range;
  using ::testing::_;

  const unsigned container_count = 20;
  const auto indexes = boost::counting_range(0u, container_count);
  const auto reverse_indexes = adapt::reverse(indexes);

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(_)).Times(container_count * 4);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times(container_count * 4);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(container_count * 2);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(container_count * 2);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(0);

  // Create 20 blobs
  for (unsigned i : indexes) {
    const std::string key = std::to_string(i);
    const std::string data(i, 'L');

    LocalBlob local_blob{container().CreateLocalBlob()};
    WriteBlobContents(local_blob, data);
    EXPECT_EQ(i, local_blob.size());

    const auto blob = container().Write(
        local_blob, key, ModifyBlobVersion::Create(), asio::use_future).get().value();

    EXPECT_EQ(key, blob.key());
    EXPECT_EQ(i, blob.size());
    EXPECT_EQ(blob.creation_time(), blob.modification_time());
    EXPECT_TRUE(blob.user_meta_data().empty());
    EXPECT_EQ(data, blob.data().value());

    auto create_result = container().Write(
        local_blob, key, ModifyBlobVersion::Create(), asio::use_future).get();
    ASSERT_FALSE(create_result.valid());
    EXPECT_EQ(NfsErrors::bad_modify_version, create_result.error());
  }

  // Delete 20 blobs
  for (unsigned i : reverse_indexes) {
    const std::string key = std::to_string(i);

    EXPECT_TRUE(
        container().DeleteBlob(key, RetrieveBlobVersion::Latest(), asio::use_future).get().valid());

    auto create_result = container().DeleteBlob(
        key, RetrieveBlobVersion::Latest(), asio::use_future).get();
    ASSERT_FALSE(create_result.valid());
    EXPECT_EQ(CommonErrors::no_such_element, create_result.error());
  }

  const auto history = container().GetVersions(asio::use_future).get().value();
  ASSERT_EQ((container_count * 2) + 1, history.size());

  unsigned count = 0;
  for (const auto& version : history) {
    const unsigned expected_count =
      (container_count - std::abs(container_count - static_cast<long>(count)));

    std::set<std::string> expected_keys;
    for (unsigned i : boost::counting_range(0u, expected_count)) {
      expected_keys.insert(std::to_string(i));
    }

    auto blobs = container().GetBlobs(version, asio::use_future).get().value();
    range::sort(blobs, Sort::KeyAscending{});

    for (const auto& blob : blobs) {
      const auto size = std::stoul(blob.key());
      const std::string data(size, 'L');

      EXPECT_EQ(size, blob.size());
      EXPECT_EQ(blob.creation_time(), blob.modification_time());
      EXPECT_TRUE(blob.user_meta_data().empty());
      EXPECT_EQ(data, blob.data().value());
    }

    const auto actual_keys = adapt::transform(blobs, Transform::Key{});
    const auto print_keys = (karma::right_align(2, ' ')[karma::string]) % ", ";
    EXPECT_TRUE(range::equal(expected_keys, actual_keys)) <<
      karma::format(
          ("Expected Containers:\t" << print_keys << karma::eol <<
           "Actual Containers:  \t" << print_keys),
          std::tie(expected_keys, actual_keys));
    ++count;
  }
}



TEST_F(PosixContainerTest, BEH_MultipleContainers) {
  namespace adapt = boost::adaptors;
  namespace karma = boost::spirit::karma;
  namespace range = boost::range;
  using ::testing::_;

  const unsigned container_count = 20;
  const auto container_indexes = boost::counting_range(0u, container_count);

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(container_count * 2);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(_)).Times(container_count * 4);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times(container_count * 4);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(container_count * 2);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(container_count * 4);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(0);

  // Create 20 containers
  for (unsigned i : container_indexes) {
    EXPECT_TRUE(
        container().OpenContainer(
            std::to_string(i), ModifyContainerVersion::Create(), asio::use_future).get().valid());

    auto create_result = container().OpenContainer(
        std::to_string(i), ModifyContainerVersion::Create(), asio::use_future).get();
    ASSERT_FALSE(create_result.valid());
    EXPECT_EQ(NfsErrors::bad_modify_version, create_result.error());
  }

  // Delete 20 containers
  for (unsigned i : adapt::reverse(container_indexes)) {
    EXPECT_TRUE(
        container().DeleteContainer(
            std::to_string(i), RetrieveContainerVersion::Latest(), asio::use_future).get().valid());

    auto create_result = container().DeleteContainer(
        std::to_string(i), RetrieveContainerVersion::Latest(), asio::use_future).get();
    ASSERT_FALSE(create_result.valid());
    EXPECT_EQ(CommonErrors::no_such_element, create_result.error());
  }

  const auto history = container().GetVersions(asio::use_future).get().value();
  ASSERT_EQ((container_count * 2) + 1, history.size());

  unsigned count = 0;
  for (const auto& version : history) {
    const unsigned expected_count =
      (container_count - std::abs(container_count - static_cast<long>(count)));

    std::set<std::string> expected_keys;
    for (unsigned i : boost::counting_range(0u, expected_count)) {
      expected_keys.insert(std::to_string(i));
    }

    auto containers = container().GetContainers(version, asio::use_future).get().value();
    range::sort(containers, Sort::KeyAscending{});

    const auto actual_keys = adapt::transform(containers, Transform::Key{});
    const auto print_keys = (karma::right_align(2, ' ')[karma::string]) % ", ";
    EXPECT_TRUE(range::equal(expected_keys, actual_keys)) <<
      karma::format(
          ("Expected Containers:\t" << print_keys << karma::eol <<
           "Actual Containers:  \t" << print_keys),
          std::tie(expected_keys, actual_keys));
    ++count;
  }
}

}  // namespace test
}  // namespace nfs
}  // namespace maidsafe
