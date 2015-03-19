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
#include <algorithm>
#include <functional>
#include <initializer_list>
#include <memory>
#include <utility>

#include "asio/use_future.hpp"
#include "boost/phoenix/core/value.hpp"
#include "boost/range/adaptor/filtered.hpp"
#include "boost/range/adaptor/map.hpp"
#include "boost/range/algorithm/equal.hpp"

#include "network_fixture.h"

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/nfs/detail/container.h"
#include "maidsafe/nfs/detail/container_info.h"

namespace maidsafe {
namespace nfs {
namespace detail {
namespace test {
namespace {
struct IsBlob {
  bool operator()(const ContainerInstance::Entry& entry) const {
    return static_cast<bool>(ContainerInstance::ExpectBlob(entry.second));
  }
};

struct IsContainerInfo {
  bool operator()(const ContainerInstance::Entry& entry) const {
    return static_cast<bool>(ContainerInstance::ExpectContainerInfo(entry.second));
  }
};

class ReadBlobContents {
 public:
  ReadBlobContents(std::weak_ptr<Network> network) : network_(std::move(network)) {}

  std::string operator()(const detail::Blob& blob) const {
    NetworkData data{blob.data_map(), network_};
    std::string contents{};
    contents.resize(data.encryptor().size());
    data.encryptor().Read(&contents[0], contents.size(), 0);
    return contents;
  }

  std::string operator()(const ContainerInstance::Value& blob) const {
    return (*this)(ContainerInstance::ExpectBlob(blob).value());
  }

  std::string operator()(const ContainerInstance::Entry& blob) const {
    return (*this)(blob.second);
  }

 private:
  const std::weak_ptr<Network> network_;
};

struct GetCryptoKey {
  Identity operator()(const ContainerInstance::Value& container_info) const {
    return ContainerInstance::ExpectContainerInfo(container_info).value().key();
  }

  Identity operator()(const ContainerInstance::Entry& container_info) const {
    return (*this)(container_info.second);
  }
};

class DetailContainerTest : public ::testing::Test, public NetworkFixture {
 protected:
  DetailContainerTest()
    : ::testing::Test(),
      NetworkFixture(),
      container_(std::make_shared<Container>(network(), ContainerInfo{})) {
  }

  const std::shared_ptr<Container>& container() const { return container_; }

  detail::Blob MakeBlob(std::string user_string, const std::string& contents) const {
    NetworkData data{network()};
    auto buffer = data.buffer();

    data.encryptor().Write(&contents[0], contents.size(), 0);
    EXPECT_EQ(contents.size(), data.encryptor().size());

    auto data_map = NetworkData::Store(std::move(data), network(), asio::use_future).get();
    if (!data_map) {
      BOOST_THROW_EXCEPTION(
          std::runtime_error(
              std::string("test failure - could not store blob : ") + data_map.error().message()));
    }

    UserMetaData user_meta_data{};
    user_meta_data.set_value(std::move(user_string)).value();

    EXPECT_EQ(contents.size(), data_map->size());
    return detail::Blob{
      network(),
      std::move(user_meta_data),
      std::move(*data_map),
      std::move(buffer)
    };
  }

  detail::Blob MakeBlob(const std::string& contents) const {
    return MakeBlob(std::string{}, contents);
  }

 private:
  const std::shared_ptr<Container> container_;
};
}  // namespace

TEST_F(DetailContainerTest, BEH_NullContainer) {
  EXPECT_THROW(Container::GetVersions(nullptr, asio::use_future), std::system_error);
  EXPECT_THROW(
      Container::GetInstance(nullptr, ContainerVersion{}, asio::use_future),
      std::system_error);
  EXPECT_THROW(
      Container::PutInstance(nullptr, boost::none, ContainerInstance{}, asio::use_future),
      std::system_error);
  EXPECT_THROW(
      Container::UpdateLatestInstance(
          nullptr, boost::phoenix::val(Expected<void>{boost::expect}), asio::use_future),
      std::system_error);
}

TEST_F(DetailContainerTest, BEH_EmptyHistory) {
  using ::testing::_;
  // The detail Container does not put an initial version on the network on creation

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container()->container_info().GetId())).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(0);

  const auto versions = Container::GetVersions(container(), asio::use_future).get();
  ASSERT_FALSE(versions.valid());
  EXPECT_EQ(make_error_code(VaultErrors::no_such_account), versions.error());
}

TEST_F(DetailContainerTest, BEH_EmptyContainer) {
  using ::testing::_;

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(0);

  const auto update =
    Container::PutInstance(
        container(), boost::none, ContainerInstance{}, asio::use_future).get();
  ASSERT_TRUE(update.valid());

  const auto instance = Container::GetInstance(container(), *update, asio::use_future).get().value();
  EXPECT_TRUE(instance.entries().empty());

  // even though it was cached, we never pulled from network - download time
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container()->container_info().GetId())).Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoGetBranchVersions(container()->container_info().GetId(), *update))
    .Times(1);

  const auto versions = Container::GetVersions(container(), asio::use_future).get().value();
  ASSERT_EQ(1u, versions.size());
  EXPECT_EQ(*update, versions[0]);
}

TEST_F(DetailContainerTest, BEH_OneBlobInContainer) {
  namespace adapt = boost::adaptors;
  namespace range = boost::range;
  using ::testing::_;

  const std::string blob_contents{"some blob contents"};

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(0);

  const auto update =
    Container::PutInstance(
        container(),
        boost::none,
        ContainerInstance{
          ContainerInstance::Entry{ContainerKey{network(), "key1"}, MakeBlob(blob_contents)}
        },
        asio::use_future).get().value();

  const auto instance = Container::GetInstance(container(), update, asio::use_future).get().value();
  EXPECT_TRUE(adapt::filter(instance.entries(), IsContainerInfo{}).empty());

  const auto blobs = adapt::filter(instance.entries(), IsBlob{});
  EXPECT_TRUE(range::equal(std::vector<std::string>({"key1"}), adapt::keys(blobs)));
  EXPECT_TRUE(
      range::equal(
          std::vector<std::string>{blob_contents},
          adapt::transform(blobs, ReadBlobContents{network()})));

  // even though it was cached, we never pulled from network - download time
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container()->container_info().GetId())).Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoGetBranchVersions(container()->container_info().GetId(), update))
    .Times(1);

  const auto versions = Container::GetVersions(container(), asio::use_future).get().value();
  ASSERT_EQ(1u, versions.size());
  EXPECT_EQ(update, versions[0]);
}

TEST_F(DetailContainerTest, BEH_OneContainerInContainer) {
  namespace adapt = boost::adaptors;
  namespace range = boost::range;
  using ::testing::_;

  const ContainerInfo inner_container{};

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(0);

  const auto update =
    Container::PutInstance(
        container(),
        boost::none,
        ContainerInstance{
          ContainerInstance::Entry{ContainerKey{network(), "key1"}, inner_container}
        },
        asio::use_future).get().value();

  const auto instance = Container::GetInstance(container(), update, asio::use_future).get().value();
  EXPECT_TRUE(adapt::filter(instance.entries(), IsBlob{}).empty());

  const auto containers = adapt::filter(instance.entries(), IsContainerInfo{});
  EXPECT_TRUE(range::equal(std::vector<std::string>({"key1"}), adapt::keys(containers)));
  EXPECT_TRUE(
      range::equal(
          adapt::transform(std::vector<ContainerInfo>{inner_container}, GetCryptoKey{}),
          adapt::transform(containers, GetCryptoKey{})));

  // even though it was cached, we never pulled from network - download time
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container()->container_info().GetId())).Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoGetBranchVersions(container()->container_info().GetId(), update))
    .Times(1);

  const auto versions = Container::GetVersions(container(), asio::use_future).get().value();
  ASSERT_EQ(1u, versions.size());
  EXPECT_EQ(update, versions[0]);
}

TEST_F(DetailContainerTest, BEH_VerifyStorage) {
  namespace adapt = boost::adaptors;
  namespace range = boost::range;
  using ::testing::_;

  const std::string blob_contents{"some blob contents"};
  const ContainerInfo inner_container{};

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(1);

  ContainerVersion version{};

  {
    const auto temp_container =
      std::make_shared<detail::Container>(
          network(), container()->parent_info(), container()->container_info());
    version = Container::PutInstance(
        temp_container,
        boost::none,
        ContainerInstance{
          ContainerInstance::Entry{ContainerKey{network(), "key1"}, inner_container},
          ContainerInstance::Entry{ContainerKey{network(), "key2"}, MakeBlob(blob_contents)}
        },
        asio::use_future)
      .get().value();
  }

  const auto instance =
    Container::GetInstance(container(), version, asio::use_future).get().value();

  const auto containers = adapt::filter(instance.entries(), IsContainerInfo{});
  EXPECT_TRUE(range::equal(std::vector<std::string>({"key1"}), adapt::keys(containers)));
  EXPECT_TRUE(
      range::equal(
          adapt::transform(std::vector<ContainerInfo>{inner_container}, GetCryptoKey{}),
          adapt::transform(containers, GetCryptoKey{})));

  const auto blobs = adapt::filter(instance.entries(), IsBlob{});
  EXPECT_TRUE(range::equal(std::vector<std::string>({"key2"}), adapt::keys(blobs)));
  EXPECT_TRUE(
      range::equal(
          std::vector<std::string>{blob_contents},
          adapt::transform(blobs, ReadBlobContents{network()})));

  // even though it was cached, we never pulled from network - download time
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(container()->container_info().GetId())).Times(1);
  EXPECT_CALL(
      GetNetworkMock(),
      DoGetBranchVersions(container()->container_info().GetId(), version))
    .Times(1);

  const auto versions = Container::GetVersions(container(), asio::use_future).get().value();
  ASSERT_EQ(1u, versions.size());
  EXPECT_EQ(version, versions[0]);
}

TEST_F(DetailContainerTest, BEH_DecryptionError) {
  using ::testing::_;

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(_)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times(0);

  ContainerVersion version{};

  {
    const auto temp_container =
      std::make_shared<detail::Container>(
          network(), ContainerInfo{}, container()->container_info());
    version = Container::PutInstance(
        temp_container, boost::none, ContainerInstance{}, asio::use_future).get().value();
  }

  const auto instance_result = Container::GetInstance(container(), version, asio::use_future).get();
  ASSERT_FALSE(instance_result.valid());
  EXPECT_EQ(EncryptErrors::failed_to_decrypt, instance_result.error())
    << instance_result.error().message();
}

TEST_F(DetailContainerTest, BEH_ParsingError) {
  using ::testing::_;

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(_)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times(0);

  ContainerVersion bad_version{};
  {
    encrypt::DataMap data_map{};
    data_map.content = encrypt::ByteVector(5, '\xFF');
    const ImmutableData encrypted_data{
      encrypt::EncryptDataMap(
          container()->parent_info().key(), container()->container_info().key(), data_map).data};

    bad_version = MakeContainerVersionRoot(encrypted_data.name());
    Network::PutChunk(network(), std::move(encrypted_data), asio::use_future).get().value();
    Network::CreateSDV(
        network(), container()->container_info().GetId(), bad_version, asio::use_future)
      .get().value();
  }

  const auto instance_result =
    Container::GetInstance(container(), bad_version, asio::use_future).get();
  ASSERT_FALSE(instance_result.valid());
  EXPECT_EQ(CommonErrors::parsing_error, instance_result.error())
    << instance_result.error().message();
}

TEST_F(DetailContainerTest, BEH_MaxVersions) {
  using ::testing::_;

  const auto update_error = boost::make_unexpected(make_error_code(CommonErrors::unknown));
  const auto max_versions = Network::GetMaxVersions();
  ASSERT_LE(max_versions, std::numeric_limits<unsigned>::max() / 2);

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times((max_versions * 2) - 1);
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(max_versions * 2);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(_)).Times((max_versions * 2) + 1);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times((max_versions * 2) + 1);

  std::vector<ContainerVersion> expected_versions{};
  for (unsigned i = 0;; ++i) {
    if (i != 0) {
      EXPECT_EQ(
          expected_versions,
          Container::GetVersions(container(), asio::use_future).get().value());

      expected_versions.resize(std::min<std::size_t>(expected_versions.size(), max_versions));

      EXPECT_FALSE(
          Container::UpdateLatestInstance(
              container(),
              boost::phoenix::val(Expected<ContainerVersion>{update_error}),
              asio::use_future)
          .get().valid());
    }

    if (max_versions * 2 <= i) {
      break;
    }

    boost::optional<ContainerVersion> replace_version{boost::none};
    if (!expected_versions.empty()) {
      replace_version = expected_versions.front();
    }

    expected_versions.insert(
        expected_versions.begin(),
        Container::PutInstance(
            container(), replace_version, ContainerInstance{}, asio::use_future).get().value());

    {
      namespace arg = std::placeholders;
      EXPECT_TRUE(
          std::none_of(
              expected_versions.cbegin() + 1,
              expected_versions.cend(),
              std::bind(std::equal_to<ContainerVersion>(), expected_versions.front(), arg::_1)));
    }
  }
}

TEST_F(DetailContainerTest, FUNC_LargeBlob) {
  using ::testing::_;

  const std::string blob_contents{RandomString(Bytes(MegaBytes(256)).count())};

  EXPECT_CALL(GetNetworkMock(), DoCreateSDV(_, _, _, _)).Times(1);
  EXPECT_CALL(GetNetworkMock(), DoPutSDVVersion(_, _, _)).Times(0);
  /* The number of Get/Put requests is significantly higher than expected. This
     is due to SelfEncryptor + DataBuffer. SelfEncryptor pushes chunks to the
     DataBuffer as its encrypting in the Close method. DataBuffer has hard
     limits for both memory and disk usage, and calls a method to handle a chunk
     thats being removed from the local cache. The simple routine puts it on the
     network, but does not track that it does! So it must re-download and re-put
     on the network when actually storing the Blob. A thread-safe callback needs
     to be given to DataBuffer to track the popped values, which will
     reduce the number of network calls. */
  EXPECT_CALL(GetNetworkMock(), DoPutChunk(_)).Times(399);
  EXPECT_CALL(GetNetworkMock(), DoGetChunk(_)).Times(395);
  EXPECT_CALL(GetNetworkMock(), DoGetBranches(_)).Times(0);
  EXPECT_CALL(GetNetworkMock(), DoGetBranchVersions(_, _)).Times(0);

  auto put =
    Container::PutInstance(
        container(),
        boost::none,
        ContainerInstance{
          ContainerInstance::Entry{ContainerKey{network(), "blob"}, MakeBlob(blob_contents)}
        },
        asio::use_future).get().value();

  const auto instance =
    Container::GetInstance(container(), std::move(put), asio::use_future).get().value();
  EXPECT_EQ(
      blob_contents,
      ReadBlobContents{network()}(
          instance.GetBlob(detail::ContainerKey{network(), "blob"}).value()));
}

}  // namespace test
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
