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
#include "maidsafe/nfs/detail/local_blob.h"

#include "maidsafe/common/error.h"
#include "maidsafe/common/make_unique.h"

namespace maidsafe {
namespace nfs {
namespace detail {

namespace {
boost::unexpected_type<std::error_code> MakeUnknownError() {
  return boost::make_unexpected(make_error_code(CommonErrors::unknown));
}

boost::unexpected_type<std::error_code> MakeBadVersionError() {
  return boost::make_unexpected(make_error_code(NfsErrors::bad_modify_version));
}
  
std::unique_ptr<NetworkData> MakeNetworkData(
    const std::shared_ptr<detail::Container>& container) {
  if (container == nullptr) {
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::null_pointer));
  }

  return make_unique<NetworkData>(encrypt::DataMap{}, container->network());
}
  
std::unique_ptr<NetworkData> MakeNetworkData(
    const std::shared_ptr<detail::Container>& container,
    const detail::Blob& blob) {
  if (container == nullptr) {
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::null_pointer));
  }

  return make_unique<NetworkData>(
      blob.data_map(), blob.GetBuffer(container->network()), container->network());
}
}  // namespace
  
LocalBlob::LocalBlob(
    const std::shared_ptr<detail::Container>& container,
    std::string key)
  : container_(std::move(container)),
    key_(std::move(key)),
    data_(MakeNetworkData(container_)),
    head_version_(BlobVersion::Defunct()),
    offset_(0) {
  assert(container_ != nullptr);  // checked in MakeNetworkData
}

LocalBlob::LocalBlob(
    const std::shared_ptr<detail::Container>& container,
    std::string key,
    const detail::Blob& head)
  : container_(std::move(container)),
    key_(std::move(key)),
    data_(MakeNetworkData(container_, head)),
    head_version_(head.version()),
    offset_(0) {
  assert(container_ != nullptr);  // checked in MakeNetworkData
}

Expected<std::uint64_t> LocalBlob::size() const {
  const auto data = data_.LockValue();
  if (data) {
    assert(data->value() != nullptr);
    return data->value()->encryptor().size();
  }
  return MakePendingOperationError();
}

Expected<BlobVersion> LocalBlob::head_version() const {
  const auto version = head_version_.value();
  if (version) {
    return *version;
  }
  return MakePendingOperationError();
}

Expected<std::uint64_t> LocalBlob::offset() const {
  const auto offset = offset_.value();
  if (offset) {
    return *offset;
  }
  return MakePendingOperationError();
}

Expected<void> LocalBlob::set_offset(const std::uint64_t offset) {
  if (offset_.set_value(offset)) {
    return Expected<void>{boost::expect};
  }

  return MakePendingOperationError();
}

std::system_error LocalBlob::MakeNullPointerException() {
  return MakeError(CommonErrors::null_pointer);
}

boost::unexpected_type<std::error_code> LocalBlob::MakePendingOperationError() {
  return boost::make_unexpected(make_error_code(CommonErrors::pending_result));
}

boost::unexpected_type<std::error_code> LocalBlob::MakeLimitError() {
  return boost::make_unexpected(make_error_code(CommonErrors::cannot_exceed_limit));
}

boost::unexpected_type<std::error_code> LocalBlob::MakeNoSuchElementError() {
  return boost::make_unexpected(make_error_code(CommonErrors::no_such_element));
}

std::unique_ptr<NetworkData> LocalBlob::FlushData(
    const AsyncValue<std::unique_ptr<NetworkData>>::Lock& data) const {
  auto& network_data = data.value();

  /* Broken strong-exception guarantee here!

     We need the DataMap to properly create a NetworkData object (otherwise a
     new SelfEncryptor could not be created). Unfortunately, we must close the
     SelfEncryptor to know the correct data map! If make_unique throws, we are
     left in a state where Write/Truncate will fail since the SelfEncryptor is
     closed. There doesn't appear to be way around this, without adding a proper
     Flush method to the SelfEncryptor.
   */
  network_data->encryptor().Close();
  auto new_data = make_unique<NetworkData>(
      network_data->encryptor().data_map(), network_data->buffer(), container()->network());

  network_data.swap(new_data);
  return new_data;
}

Expected<void> LocalBlob::UpdateBlob(
    ContainerInstance& instance,
    const AsyncValue<BlobVersion>::Lock& head_version,
    const detail::Blob& blob) const {
  const auto current_version = instance.GetBlob(key());
  if (!current_version) {
    if (current_version.error() != CommonErrors::no_such_element ||
        BlobVersion::Defunct() != head_version.value()) {
      return boost::make_unexpected(current_version.error());
    }
  } else if (current_version->version() != head_version.value()) {
    return MakeBadVersionError();
  }

  instance.UpdateEntry(key(), blob);
  return Expected<void>{boost::expect};
}

Expected<std::uint64_t> LocalBlob::Read(
    const asio::mutable_buffer& buffer,
    const AsyncValue<std::unique_ptr<NetworkData>>::ConstLock& data,
    const AsyncValue<std::uint64_t>::Lock& offset) {
  try {
    auto& encryptor = data.value()->encryptor();
    assert(offset.value() <= encryptor.size());

    const auto size = asio::buffer_size(buffer);
    const auto read_size = std::min<std::uint64_t>(encryptor.size() - offset.value(), size);

    if (!data.value()->encryptor().Read(
            asio::buffer_cast<char*>(buffer), read_size, offset.value())) {
      return MakeUnknownError();
    }

    offset.value() += read_size;
    return read_size;
  } catch (const std::system_error& error) {
    return boost::make_unexpected(error.code());
  }
}

Expected<void> LocalBlob::Write(
    const asio::const_buffer& buffer,
    const AsyncValue<std::unique_ptr<NetworkData>>::Lock& data,
    const AsyncValue<std::uint64_t>::Lock& offset) {
  try {
    const auto write_size = asio::buffer_size(buffer);
    if (!data.value()->encryptor().Write(
            asio::buffer_cast<const char*>(buffer), write_size, offset.value())) {
      return MakeUnknownError();
    }
    offset.value() += write_size;
    assert(offset.value() <= data.value()->encryptor().size());
  } catch (const std::system_error& error) {
    return boost::make_unexpected(error.code());
  }

  return Expected<void>{boost::expect};
}

Expected<void> LocalBlob::Truncate(
    const std::uint64_t size,
    const AsyncValue<std::unique_ptr<NetworkData>>::Lock& data,
    const AsyncValue<std::uint64_t>::Lock& offset) {
  try {
    if (!data.value()->encryptor().Truncate(size)) {
      return MakeUnknownError();
    }
    offset.value() = size;
    assert(offset.value() <= data.value()->encryptor().size());
  } catch (const std::system_error& error) {
    return boost::make_unexpected(error.code());
  }

  return Expected<void>{boost::expect};
}

}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
