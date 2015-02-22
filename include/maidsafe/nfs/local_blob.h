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
#ifndef MAIDSAFE_NFS_LOCAL_BLOB_H_
#define MAIDSAFE_NFS_LOCAL_BLOB_H_

#include <cstdint>
#include <limits>
#include <string>
#include <system_error>

#include "asio/buffer.hpp"
#include "boost/optional.hpp"

#include "maidsafe/nfs/blob_version.h"
#include "maidsafe/nfs/detail/async_result.h"
#include "maidsafe/nfs/detail/coroutine.h"
#include "maidsafe/nfs/detail/detail_fwd.h"
#include "maidsafe/nfs/detail/forwarding_callback.h"
#include "maidsafe/nfs/detail/local_blob.h"
#include "maidsafe/nfs/detail/meta_data.h"
#include "maidsafe/nfs/expected.h"

namespace maidsafe {
namespace nfs {

class LocalBlob {
 public:
  using Clock = detail::MetaData::Clock;
  
  LocalBlob(const std::shared_ptr<detail::Container>& container, std::string key);
  LocalBlob(
      const std::shared_ptr<detail::Container>& container,
      std::string key,
      const detail::Blob& head);

  LocalBlob(LocalBlob&& other)
    : working_blob_(std::move(other.working_blob_)),
      meta_data_(std::move(other.meta_data_)) {
  }

  LocalBlob& operator=(LocalBlob&& other) {
    working_blob_ = std::move(other.working_blob_);
    meta_data_ = std::move(other.meta_data_);
    return *this;
  }

  const std::string& key() const { return working_blob_->key(); }
  Expected<std::uint64_t> size() const { return working_blob_->size(); }
  Clock::time_point creation_time() const { return meta_data_.creation_time(); }
  Clock::time_point last_modification_time() const { return meta_data_.last_modification_time(); }

  const std::string& user_meta_data() { return meta_data_.user(); }
  Expected<void> set_user_meta_data(std::string user) {
    return meta_data_.set_user(std::move(user));
  }

  Expected<BlobVersion> head_version() const { return working_blob_->head_version(); }

  template<typename Token>
  detail::AsyncResultReturn<Token, std::vector<BlobVersion>> GetVersions(Token token) {
    using Handler = detail::AsyncHandler<Token, std::vector<BlobVersion>>;

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};
    detail::LocalBlob::GetVersions(
        working_blob_, detail::MakeForwardingCallback(std::move(handler)));
    return result.get();
  }

  Expected<std::uint64_t> offset() const { return working_blob_->offset(); }
  Expected<void> set_offset(const std::uint64_t offset) {
    return working_blob_->set_offset(offset);
  }

  template<typename Buffer, typename Token>
  detail::AsyncResultReturn<Token, std::uint64_t> Read(Buffer buffer, Token token) {
    using Handler = detail::AsyncHandler<Token, std::uint64_t>;

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};
    working_blob_->Read(std::move(buffer), detail::MakeForwardingCallback(std::move(handler)));
    return result.get();
  }

  template<typename Buffer, typename Token>
  detail::AsyncResultReturn<Token, void> Write(Buffer buffer, Token token) {
    using Handler = detail::AsyncHandler<Token, void>;

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};
    meta_data_.UpdateLastModifiedTime();
    working_blob_->Write(std::move(buffer), detail::MakeForwardingCallback(std::move(handler)));
    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, void> Truncate(const std::uint64_t size, Token token) {
    using Handler = detail::AsyncHandler<Token, void>;

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};
    meta_data_.UpdateLastModifiedTime();
    working_blob_->Truncate(size, detail::MakeForwardingCallback(std::move(handler)));
    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, BlobVersion> Commit(Token token) {
    using Handler = detail::AsyncHandler<Token, BlobVersion>;

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};
    detail::LocalBlob::Commit(
        working_blob_, meta_data_, detail::MakeForwardingCallback(std::move(handler)));
    return result.get();
  }

 private:
  LocalBlob(const LocalBlob&) = delete;
  LocalBlob& operator=(const LocalBlob&) = delete;

 private:
  std::shared_ptr<detail::LocalBlob> working_blob_;
  detail::MetaData meta_data_;
};
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_LOCAL_BLOB_H_
