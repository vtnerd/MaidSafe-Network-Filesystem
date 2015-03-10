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
#include "maidsafe/nfs/local_blob.h"

#include "maidsafe/common/make_unique.h"

namespace maidsafe {
namespace nfs {
namespace {
boost::unexpected_type<std::error_code> MakeUnknownError() {
  return boost::make_unexpected(make_error_code(CommonErrors::unknown));
}
}  // namespace

LocalBlob::LocalBlob(std::weak_ptr<detail::Network> network)
  : data_(make_unique<detail::NetworkData>(std::move(network))),
    offset_(0),
    user_meta_data_() {
}

LocalBlob::LocalBlob(const std::weak_ptr<detail::Network>& network, const detail::Blob& head)
  : data_(make_unique<detail::NetworkData>(head.data_map(), head.GetBuffer(network), network)),
    offset_(0),
    user_meta_data_(head.meta_data().user_meta_data()) {
}

std::uint64_t LocalBlob::size() const {
  if (data_ == nullptr) {
    BOOST_THROW_EXCEPTION(MakeNullPointerException());
  }
  return data_->encryptor().size();
}

Expected<void> LocalBlob::UpdateBlob::operator()(
    detail::ContainerInstance& instance,
    const std::shared_ptr<detail::Network>& network,
    const detail::ContainerKey& key,
    const ModifyBlobVersion& replace,
    const detail::PendingBlob& pending_blob,
    detail::Blob& new_blob) const {
  return instance.UpdateEntries(
      [&] (detail::ContainerInstance::Entries& entries) {

        return detail::ContainerInstance::Get(entries, key).bind(
            [&] (detail::ContainerInstance::Entries::iterator entry) {

              return detail::ContainerInstance::ExpectBlob(entry->second).bind(
                  [&] (detail::Blob current_blob) -> Expected<void> {

                    if (replace == current_blob.version() ||
                        replace == ModifyBlobVersion::Latest()) {
                      new_blob =
                        detail::Blob{
                          network, pending_blob, current_blob.meta_data().creation_time()};
                      entry->second = new_blob;
                      return Expected<void>{boost::expect};
                    } else {
                      return boost::make_unexpected(make_error_code(NfsErrors::bad_modify_version));
                    }
                  });

            }).catch_error(
                [&] (const std::error_code error) -> Expected<void> {

                  if (error == CommonErrors::no_such_element &&
                      replace == ModifyBlobVersion::Create()) {
                    new_blob = detail::Blob{network, pending_blob};
                    entries[key] = new_blob;
                    return Expected<void>{boost::expect};
                  } else {
                    return boost::make_unexpected(error);
                  }
                });
      });
}

std::system_error LocalBlob::MakeNullPointerException() {
  return MakeError(CommonErrors::null_pointer);
}

std::unique_ptr<detail::NetworkData> LocalBlob::FlushData(
    const std::weak_ptr<detail::Network>& network) {
  assert(data_ != nullptr);
  /* Broken strong-exception guarantee here!

     We need the DataMap to properly create a NetworkData object (otherwise a
     new SelfEncryptor could not be created). Unfortunately, we must close the
     SelfEncryptor to know the correct data map! If make_unique throws, we are
     left in a state where Write/Truncate will fail since the SelfEncryptor is
     closed. There doesn't appear to be way around this, without adding a proper
     Flush method to the SelfEncryptor.
   */
  data_->encryptor().Close();
  auto new_data = make_unique<detail::NetworkData>(
      data_->encryptor().data_map(), data_->buffer(), network);

  data_.swap(new_data);
  return new_data;
}

Expected<std::uint64_t> LocalBlob::Read(const asio::mutable_buffer& buffer) {
  assert(data_ != nullptr);
  try {
    auto& encryptor = data_->encryptor();
    assert(offset_ <= encryptor.size());

    const auto size = asio::buffer_size(buffer);
    const auto read_size = std::min<std::uint64_t>(encryptor.size() - offset_, size);

    if (!encryptor.Read(asio::buffer_cast<char*>(buffer), read_size, offset_)) {
      return MakeUnknownError();
    }

    offset_ += read_size;
    return read_size;
  } catch (const std::system_error& error) {
    return boost::make_unexpected(error.code());
  }
}

Expected<void> LocalBlob::Write(const asio::const_buffer& buffer) {
  assert(data_ != nullptr);
  try {
    const auto write_size = asio::buffer_size(buffer);
    if (!data_->encryptor().Write(asio::buffer_cast<const char*>(buffer), write_size, offset_)) {
      return MakeUnknownError();
    }
    offset_ += write_size;
    assert(offset_ <= data_->encryptor().size());
  } catch (const std::system_error& error) {
    return boost::make_unexpected(error.code());
  }

  return Expected<void>{boost::expect};
}

Expected<void> LocalBlob::Truncate(const std::uint64_t size) {
  assert(data_ != nullptr);
  try {
    if (!data_->encryptor().Truncate(size)) {
      return MakeUnknownError();
    }
    offset_ = size;
    assert(offset_ <= data_->encryptor().size());
  } catch (const std::system_error& error) {
    return boost::make_unexpected(error.code());
  }

  return Expected<void>{boost::expect};
}
}  // namespace nfs
}  // namespace maidsafe
