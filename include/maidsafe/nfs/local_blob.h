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
#ifndef MAIDSAFE_NFS_LOCAL_BLOB_H_
#define MAIDSAFE_NFS_LOCAL_BLOB_H_

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <system_error>

#include "asio/buffer.hpp"
#include "boost/optional.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/error.h"
#include "maidsafe/nfs/blob_version.h"
#include "maidsafe/nfs/detail/action/action_abort.h"
#include "maidsafe/nfs/detail/action/action_ignore.h"
#include "maidsafe/nfs/detail/action/action_resume.h"
#include "maidsafe/nfs/detail/action/action_store.h"
#include "maidsafe/nfs/detail/async_result.h"
#include "maidsafe/nfs/detail/container.h"
#include "maidsafe/nfs/detail/container_instance.h"
#include "maidsafe/nfs/detail/coroutine.h"
#include "maidsafe/nfs/detail/detail_fwd.h"
#include "maidsafe/nfs/detail/forwarding_callback.h"
#include "maidsafe/nfs/detail/network_data.h"
#include "maidsafe/nfs/detail/operation_handler.h"
#include "maidsafe/nfs/detail/user_meta_data.h"
#include "maidsafe/nfs/blob.h"
#include "maidsafe/nfs/expected.h"

namespace maidsafe {
namespace nfs {
class LocalBlob {
 public:
  explicit LocalBlob(std::weak_ptr<detail::Network> network);
  LocalBlob(const std::weak_ptr<detail::Network>& network, const detail::Blob& head);

  LocalBlob(LocalBlob&& other) MAIDSAFE_NOEXCEPT
    : data_(std::move(other.data_)),
      offset_(std::move(other.offset_)),
      user_meta_data_(std::move(other.user_meta_data_)) {
  }

  LocalBlob& operator=(LocalBlob&& other) MAIDSAFE_NOEXCEPT {
    data_ = std::move(other.data_);
    offset_ = std::move(other.offset_);
    user_meta_data_ = std::move(other.user_meta_data_);
    return *this;
  }

  const std::string& user_meta_data() const MAIDSAFE_NOEXCEPT { return user_meta_data_.value(); }
  Expected<void> set_user_meta_data(std::string user) {
    return user_meta_data_.set_value(std::move(user));
  }

  std::uint64_t size() const;
  std::uint64_t offset() const { return offset_; }
  void set_offset(const std::uint64_t value) { offset_ = value; }

  template<typename Buffer, typename Token>
  detail::AsyncResultReturn<Token, std::uint64_t> Read(Buffer buffer, Token token) {
    using Handler = detail::AsyncHandler<Token, std::uint64_t>;

    if (data_ == nullptr) {
      BOOST_THROW_EXCEPTION(MakeNullPointerException());
    }

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};
    handler(Read(std::move(buffer)));

    return result.get();
  }

  template<typename Buffer, typename Token>
  detail::AsyncResultReturn<Token, void> Write(Buffer buffer, Token token) {
    using Handler = detail::AsyncHandler<Token, void>;

    if (data_ == nullptr) {
      BOOST_THROW_EXCEPTION(MakeNullPointerException());
    }

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};
    handler(Write(std::move(buffer)));

    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, void> Truncate(const std::uint64_t size, Token token) {
    using Handler = detail::AsyncHandler<Token, void>;

    if (data_ == nullptr) {
      BOOST_THROW_EXCEPTION(MakeNullPointerException());
    }

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};
    handler(Truncate(size));

    return result.get();
  }

  //
  // To be used internally, use Container::Write instead.
  //
  template<typename Token>
  detail::AsyncResultReturn<Token, Blob> Commit(
      std::shared_ptr<detail::Container> container,
      detail::ContainerKey update_key,
      boost::optional<detail::Blob> replace,
      Token token) {
    using Handler = detail::AsyncHandler<Token, Blob>;

    if (container == nullptr || data_ == nullptr) {
      BOOST_THROW_EXCEPTION(MakeNullPointerException());
    }

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};

    auto original_data = FlushData(container->network());
    assert(original_data != nullptr);
    
    auto coro = detail::MakeCoroutine<CommitRoutine<Handler>>(
        std::move(container),
        std::move(update_key),
        std::move(replace),
        std::move(original_data),
        user_meta_data_,
        original_data->encryptor().data_map(),
        original_data->buffer(),
        std::move(handler));
    coro.Execute();

    return result.get();
  }

 private:
  template<typename Handler>
  struct CommitRoutine {
    struct Frame {
      std::shared_ptr<detail::Container> container;
      const detail::ContainerKey update_key;
      boost::optional<detail::Blob> replace;
      std::unique_ptr<detail::NetworkData> store_data;
      detail::UserMetaData new_user_meta_;
      encrypt::DataMap new_data_map_;
      std::shared_ptr<detail::NetworkData::Buffer> buffer_;
      Handler handler;
    };

    void operator()(detail::Coroutine<CommitRoutine, Frame>& coro) const {
      namespace action = detail::action;
      using detail::operation;
      assert(coro.frame().container != nullptr);

      ASIO_CORO_REENTER(coro) {
        assert(coro.frame().store_data != nullptr);
        ASIO_CORO_YIELD
          detail::NetworkData::Store(
              std::move(*(coro.frame().store_data)),
              coro.frame().container->network(),
              operation
                .OnSuccess(action::Ignore<encrypt::DataMap>().Then(action::Resume(coro)))
                .OnFailure(action::Abort(coro)));

        coro.frame().store_data.reset();

        detail::Container::UpdateLatestInstance(
            coro.frame().container,
            std::bind(
                UpdateBlob{},
                std::placeholders::_1,
                coro.frame().container->network(),
                std::move(coro.frame().update_key),
                std::move(coro.frame().replace),
                std::move(coro.frame().new_user_meta_),
                std::move(coro.frame().new_data_map_),
                std::move(coro.frame().buffer_)),
            detail::MakeForwardingCallback(std::move(coro.frame().handler)));
      }
    }
  };

 private:
  LocalBlob(const LocalBlob&) = delete;
  LocalBlob& operator=(const LocalBlob&) = delete;

 private:
  struct UpdateBlob {
    Expected<Blob> operator()(
        detail::ContainerInstance& instance,
        const std::weak_ptr<detail::Network>& network,
        const detail::ContainerKey& key,
        const boost::optional<detail::Blob>& replace,
        const detail::UserMetaData& new_user_meta,
        const encrypt::DataMap& new_data_map,
        const std::shared_ptr<detail::NetworkData::Buffer>& buffer) const;
  };

  static std::system_error MakeNullPointerException();
  std::unique_ptr<detail::NetworkData> FlushData(const std::weak_ptr<detail::Network>& network);

  Expected<std::uint64_t> Read(const asio::mutable_buffer& buffer);

  template<typename MutableBufferSequence>
  Expected<std::uint64_t> Read(const MutableBufferSequence& sequence) {
    std::uint64_t total_read = 0;
    for (auto& buffer : sequence) {
      const auto read_length = Read(buffer);
      if (!read_length) {
        return read_length;
      }

      if (std::numeric_limits<uint64_t>::max() - total_read <= *read_length) {
        return boost::make_unexpected(make_error_code(CommonErrors::cannot_exceed_limit));
      }

      total_read += *read_length;
    }

    return total_read;
  }

  Expected<void> Write(const asio::const_buffer& buffer);

  template<typename ConstBufferSequence>
  Expected<void> Write(const ConstBufferSequence& sequence) {
    for(const auto& buffer : sequence) {
      const auto write_result = Write(buffer);
      if (!write_result) {
        return write_result;
      }
    }
    return Expected<void>{boost::expect};
  }

  Expected<void> Truncate(const std::uint64_t size);

 private:
  /* unique_ptr is used because NetworkData is not copyable or movable
     (SelfEncryptor). boost::optional was preferred, but swap was needed for
     exception guarantee, but swap is unavailable in NetworkData
     (SelfEncryptor). */
  std::unique_ptr<detail::NetworkData> data_;
  std::uint64_t offset_;
  detail::UserMetaData user_meta_data_;
};
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_LOCAL_BLOB_H_
