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
#include "maidsafe/nfs/detail/network_data.h"

#include <system_error>

#include "asio/use_future.hpp"

#include "maidsafe/common/unordered_set.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/nfs/detail/network.h"

namespace maidsafe {
namespace nfs {
namespace detail {

namespace {
std::shared_ptr<NetworkData::Buffer> MakeValidLocalBuffer(
    std::shared_ptr<NetworkData::Buffer> local_buffer, const std::weak_ptr<Network>& network) {
  if (local_buffer != nullptr) {
    return local_buffer;
  }

  return NetworkData::MakeBuffer(network);
}

struct GetChunkCallback {
  GetChunkCallback(std::shared_ptr<NetworkData::Buffer> buffer, std::weak_ptr<Network> network)
    : buffer_(std::move(buffer)),
      network_(std::move(network)) {
  }

  NonEmptyString operator()(std::string key) const {
    return {NetworkData::GetChunk(*buffer_, network_, std::move(key)).value().data()};
  }

private:
  std::shared_ptr<NetworkData::Buffer> buffer_;
  std::weak_ptr<Network> network_;
};
}  // namespace

std::shared_ptr<NetworkData::Buffer> NetworkData::MakeBuffer(const std::weak_ptr<Network>& network) {
  return std::make_shared<Buffer>(
      MemoryUsage(Bytes(MebiBytes(5)).count()),
      DiskUsage(Bytes(MebiBytes(100)).count()),
      [network](const std::string&, const NonEmptyString& data) {
        Network::PutChunk(network.lock(), ImmutableData{data}, asio::use_future).get().value();
      });
}

Expected<ImmutableData> NetworkData::GetChunk(
    Buffer& buffer, const std::weak_ptr<Network>& network, std::string raw_key) {
  ImmutableData::Name key{Identity{std::move(raw_key)}};
  try {
    return ImmutableData{buffer.Get(key.value.string())};
  } catch (const std::system_error& e) {
    if (e.code() != make_error_code(CommonErrors::no_such_element)) {
      return boost::make_unexpected(e.code());
    }
  }

  return Network::GetChunk(network.lock(), std::move(key), asio::use_future).get();
}

NetworkData::NetworkData(std::weak_ptr<Network> network)
  : NetworkData(encrypt::DataMap{}, {}, std::move(network)) {
}

NetworkData::NetworkData(std::shared_ptr<Buffer> buffer, std::weak_ptr<Network> network)
  : NetworkData(encrypt::DataMap{}, std::move(buffer), std::move(network)) {
}

NetworkData::NetworkData(encrypt::DataMap existing_data, std::weak_ptr<Network> network)
  : NetworkData(std::move(existing_data), {}, std::move(network)) {
}

NetworkData::NetworkData(
    encrypt::DataMap existing_data,
    std::shared_ptr<Buffer> buffer,
    std::weak_ptr<Network> network)
  : buffer_(MakeValidLocalBuffer(std::move(buffer), network)),
    map_(std::move(existing_data)),
    encryptor_(
        map_,
        *buffer_,
        GetChunkCallback{buffer_, std::move(network)}) {
  assert(buffer_ != nullptr);
}

NetworkData::~NetworkData() {
  try {
    encryptor_.Close();
  } catch (...) {
  }
}

std::vector<encrypt::ByteVector> NetworkData::PrepareNewChunks() {
  encryptor().Close();

  maidsafe::unordered_set<encrypt::ByteVector> original_chunks;
  for (auto& chunk : encryptor().original_data_map().chunks) {
    original_chunks.insert(chunk.hash);
  }

  std::vector<encrypt::ByteVector> new_chunks;
  for (auto& chunk : map_.chunks) {
    if (original_chunks.find(chunk.hash) == original_chunks.end()) {
      new_chunks.push_back(chunk.hash);
    }
  }

  return new_chunks;
}

}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
