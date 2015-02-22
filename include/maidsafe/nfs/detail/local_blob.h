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
#ifndef MAIDSAFE_NFS_DETAIL_LOCAL_BLOB_H_
#define MAIDSAFE_NFS_DETAIL_LOCAL_BLOB_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "boost/optional.hpp"

#include "maidsafe/nfs/blob_version.h"
#include "maidsafe/nfs/container_version.h"
#include "maidsafe/nfs/detail/action/action_abort.h"
#include "maidsafe/nfs/detail/action/action_ignore.h"
#include "maidsafe/nfs/detail/action/action_store.h"
#include "maidsafe/nfs/detail/action/action_resume.h"
#include "maidsafe/nfs/detail/async_result.h"
#include "maidsafe/nfs/detail/async_value.h"
#include "maidsafe/nfs/detail/container.h"
#include "maidsafe/nfs/detail/container_instance.h"
#include "maidsafe/nfs/detail/coroutine.h"
#include "maidsafe/nfs/detail/detail_fwd.h"
#include "maidsafe/nfs/detail/operation_handler.h"
#include "maidsafe/nfs/detail/network_data.h"
#include "maidsafe/nfs/expected.h"

namespace maidsafe {
namespace nfs {
namespace detail {

// All public methods are thread-safe
class LocalBlob {
 public:
  LocalBlob();
  LocalBlob(const detail::Blob& head);

  Expected<std::uint64_t> size() const;

  template<typename Token>
  static detail::AsyncResultReturn<Token, std::vector<BlobVersion>> GetVersions(
      std::shared_ptr<LocalBlob> local_blob, Token token) {
    using Handler = detail::AsyncHandler<Token, std::vector<BlobVersion>>;

    if (local_blob == nullptr) {
      BOOST_THROW_EXCEPTION(MakeNullPointerException());
    }

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};

    auto coro = MakeCoroutine<GetVersionsRoutine<Handler>>(
        std::move(local_blob),
        std::vector<ContainerVersion>{},
        detail::ContainerInstance{},
        std::vector<BlobVersion>{},
        std::move(handler));
    coro.Execute();

    return result.get();
  }

  Expected<std::uint64_t> offset() const;
  Expected<void> set_offset(const std::uint64_t value);

  template<typename Buffer, typename Token>
  detail::AsyncResultReturn<Token, std::uint64_t> Read(Buffer buffer, Token token) {
    using Handler = detail::AsyncHandler<Token, std::uint64_t>;

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};

    auto data = data_.LockCValue();
    auto offset = offset_.LockValue();
    if (!data || !offset) {
      handler(MakePendingOperationError());
    } else {
      auto result = Read(std::move(buffer), *data, *offset);
      // release locks on values before calling handler
      data = boost::none;
      offset = boost::none;
      handler(std::move(result));
    }

    return result.get();
  }

  template<typename Buffer, typename Token>
  detail::AsyncResultReturn<Token, void> Write(Buffer buffer, Token token) {
    using Handler = detail::AsyncHandler<Token, void>;

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};

    auto data = data_.LockValue();
    auto offset = offset_.LockValue();
    if (!data || !offset) {
      handler(MakePendingOperationError());
    } else {
      auto result = Write(std::move(buffer), *data, *offset);
      // release locks on values before calling handler
      data = boost::none;
      offset = boost::none;
      handler(std::move(result));
    }

    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, void> Truncate(const std::uint64_t size, Token token) {
    using Handler = detail::AsyncHandler<Token, void>;

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};

    auto data = data_.LockValue();
    auto offset = offset_.LockValue();
    if (!data || !offset) {
      handler(MakePendingOperationError());
    } else {
      auto result = Truncate(size, *data, *offset);
      // release locks on values before calling handler
      data = boost::none;
      offset = boost::none;
      handler(std::move(result));
    }

    return result.get();
  }

  //
  // Expected to only be invoked internally. Use Container::Copy instead.
  //
  template<typename Token>
  static detail::AsyncResultReturn<Token, BlobVersion> Commit(
      std::shared_ptr<LocalBlob> local_blob, MetaData meta_data, Token token) {
    using Handler = detail::AsyncHandler<Token, BlobVersion>;

    if (local_blob == nullptr) {
      BOOST_THROW_EXCEPTION(MakeNullPointerException());
    }

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};

    auto data = local_blob->data_.LockValue();
    auto head_version = local_blob->head_version_.LockValue();
    if (!data || !head_version) {
      handler(MakePendingOperationError());
    } else {
      auto original_data = local_blob->FlushData(*data);
      assert(original_data != nullptr);

      // Release lock before start coroutine
      data = boost::none;
      detail::Blob new_blob{
        std::move(meta_data), original_data->encryptor().data_map(), original_data->buffer()};

      auto coro = MakeCoroutine<CommitRoutine<Handler>>(
          std::move(local_blob),
          std::move(original_data),
          std::move(new_blob),
          std::move(head_version),
          encrypt::DataMap{},
          std::vector<ContainerVersion>{},
          ContainerInstance{},
          Expected<ContainerVersion>{},
          std::move(handler));
      coro.Execute();
    }

    return result.get();
  }

 private:
  template<typename Handler>
  struct GetVersionsRoutine {
    struct Frame {
      std::shared_ptr<detail::LocalBlob> local_blob;
      std::vector<ContainerVersion> container_versions;
      detail::ContainerInstance instance;
      std::vector<BlobVersion> result;
      Handler handler;
    };

    void operator()(detail::Coroutine<GetVersionsRoutine, Frame>& coro) const {
      assert(coro.frame().local_blob != nullptr);

      ASIO_CORO_REENTER(coro) {
        ASIO_CORO_YIELD
          Container::GetVersions(
              coro.frame().local_blob->container(),
              operation
                .OnSuccess(
                    action::Store(std::ref(coro.frame().container_versions))
                    .Then(action::Resume(coro)))
                .OnFailure(action::Abort(coro)));

        while (!coro.frame().container_versions.empty()) {
          ASIO_CORO_YIELD
            Container::GetInstance(
                coro.frame().local_blob->container(),
                std::move(coro.frame().container_versions.front()),
                operation
                  .OnSuccess(
                      action::Store(std::ref(coro.frame().instance))
                      .Then(action::Resume(coro)))
                  .OnFailure(action::Abort(coro)));

          auto blob_version = BlobVersion::Defunct();
          const auto blob =
            std::move(coro.frame().instance).GetBlob(coro.frame().local_blob->key());
          if (blob) {
            blob_version = blob->version();
          }

          if (coro.frame().result.empty() || coro.frame().result.back() != blob_version) {
            coro.frame().result.push_back(blob_version);
          }

          coro.frame().container_versions.erase(coro.frame().container_versions.begin());
        }

        coro.frame().handler(std::move(coro.frame().result));
      }
    }
  };

  template<typename Handler>
  struct CommitRoutine {
    struct Frame {
      std::shared_ptr<detail::LocalBlob> local_blob;
      std::unique_ptr<NetworkData> new_data;
      detail::Blob new_blob;
      boost::optional<AsyncValue<BlobVersion>::Lock> head_version;
      encrypt::DataMap new_data_map;
      std::vector<ContainerVersion> container_versions;
      ContainerInstance instance;
      Expected<ContainerVersion> put_result;
      Handler handler;
    };

    class SafeAbort {
     public:
      explicit SafeAbort(Coroutine<CommitRoutine, Frame> coro) : coro_(std::move(coro)) {}

      void operator()(std::error_code error) {
        assert(error);
        (*this)(boost::make_unexpected(std::move(error)));
      }

      void operator()(boost::unexpected_type<std::error_code> error) {
        // Release locks before calling handler
        coro_.frame().head_version = boost::none;
        coro_.frame().handler(std::move(error));
      }

     private:
      Coroutine<CommitRoutine, Frame> coro_;
    };

    void operator()(Coroutine<CommitRoutine, Frame>& coro) const {
      assert(coro.frame().local_blob != nullptr);
      assert(coro.frame().head_version);

      ASIO_CORO_REENTER(coro) {
        assert(coro.frame().new_data != nullptr);
        ASIO_CORO_YIELD
          NetworkData::Store(
              std::move(*(coro.frame().new_data)),
              coro.frame().local_blob->container()->network(),
              operation
                .OnSuccess(
                    action::Store(std::ref(coro.frame().new_data_map))
                    .Then(action::Resume(coro)))
                .OnFailure(SafeAbort{coro}));

        assert(coro.frame().new_data_map == coro.frame().new_blob.data_map());
        coro.frame().new_data.release();

        ASIO_CORO_YIELD
          detail::Container::UpdateLatestInstance(
              coro.frame().local_blob->container(),
              std::bind(
                  &LocalBlob::UpdateBlob,
                  coro.frame().local_blob,
                  std::placeholders::_1,
                  std::cref(*(coro.frame().head_version)),
                  std::cref(coro.frame().new_blob)),
              operation
                .OnSuccess(action::Ignore<ContainerVersion>().Then(action::Resume(coro)))
                .OnFailure(SafeAbort{coro}));

        coro.frame().head_version->value() = coro.frame().new_blob.version();

        // release locks on value before calling handler
        coro.frame().head_version = boost::none;
        coro.frame().handler(std::move(coro.frame().new_blob).version());
      }
    }
  };

 private:
  LocalBlob(const LocalBlob&) = delete;
  LocalBlob(LocalBlob&&) = delete;

  LocalBlob& operator=(const LocalBlob&) = delete;
  LocalBlob& operator=(LocalBlob&&) = delete;

 private:
  static std::system_error MakeNullPointerException();
  static boost::unexpected_type<std::error_code> MakePendingOperationError();
  static boost::unexpected_type<std::error_code> MakeLimitError();
  static boost::unexpected_type<std::error_code> MakeNoSuchElementError();

  std::unique_ptr<NetworkData> FlushData(
      const AsyncValue<std::unique_ptr<NetworkData>>::Lock& data) const;

  Expected<void> UpdateBlob(
      ContainerInstance& instance,
      const AsyncValue<BlobVersion>::Lock& head_version,
      const detail::Blob& blob) const;

  static Expected<std::uint64_t> Read(
      const asio::mutable_buffer& buffer,
      const AsyncValue<std::unique_ptr<NetworkData>>::ConstLock& data,
      const AsyncValue<std::uint64_t>::Lock& offset);

  template<typename MutableBufferSequence>
  static Expected<std::uint64_t> Read(
      const MutableBufferSequence& sequence,
      const AsyncValue<std::unique_ptr<NetworkData>>::ConstLock& data,
      const AsyncValue<std::uint64_t>::Lock& offset) {
    std::uint64_t total_read = 0;
    for (auto& buffer : sequence) {
      const auto read_length = Read(buffer, data, offset);
      if (!read_length) {
        return read_length;
      }

      if (std::numeric_limits<uint64_t>::max() - total_read <= *read_length) {
        return MakeLimitError();
      }

      total_read += *read_length;
    }

    return total_read;
  }

  static Expected<void> Write(
      const asio::const_buffer& buffer,
      const AsyncValue<std::unique_ptr<NetworkData>>::Lock& data,
      const AsyncValue<std::uint64_t>::Lock& offset);

  template<typename ConstBufferSequence>
  static Expected<void> Write(
      const ConstBufferSequence& sequence,
      const AsyncValue<std::unique_ptr<NetworkData>>::Lock& data,
      const AsyncValue<std::uint64_t>::Lock& offset) {
    for(const auto& buffer : sequence) {
      const auto write_result = Write(buffer, data, offset);
      if (!write_result) {
        return write_result;
      }
    }
    return Expected<void>{boost::expect};
  }

  static Expected<void> Truncate(
      const std::uint64_t size,
      const AsyncValue<std::unique_ptr<NetworkData>>::Lock& data,
      const AsyncValue<std::uint64_t>::Lock& offset);

 private:

  /* unique_ptr is used because NetworkData is not copyable or movable
     (SelfEncryptor). boost::optional was preferred, but swap was needed for
     exception guarantee, but swap is unavailable in NetworkData
     (SelfEncryptor). */
  AsyncValue<std::unique_ptr<NetworkData>> data_;
  AsyncValue<std::uint64_t> offset_;
};

}  // detail
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_LOCAL_BLOB_H_
