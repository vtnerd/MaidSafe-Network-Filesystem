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
#ifndef MAIDSAFE_NFS_CONTAINER_H_
#define MAIDSAFE_NFS_CONTAINER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "maidsafe/common/config.h"
#include "maidsafe/common/error.h"
#include "maidsafe/nfs/container_version.h"
#include "maidsafe/nfs/detail/action/action_abort.h"
#include "maidsafe/nfs/detail/action/action_ignore.h"
#include "maidsafe/nfs/detail/action/action_store.h"
#include "maidsafe/nfs/detail/action/action_resume.h"
#include "maidsafe/nfs/detail/async_result.h"
#include "maidsafe/nfs/detail/blob.h"
#include "maidsafe/nfs/detail/container.h"
#include "maidsafe/nfs/detail/container_instance.h"
#include "maidsafe/nfs/detail/container_key.h"
#include "maidsafe/nfs/detail/coroutine.h"
#include "maidsafe/nfs/detail/forwarding_callback.h"
#include "maidsafe/nfs/detail/operation_handler.h"
#include "maidsafe/nfs/blob.h"
#include "maidsafe/nfs/container_info.h"
#include "maidsafe/nfs/local_blob.h"

namespace maidsafe {
namespace nfs {

class PosixContainer {
 public:
  explicit PosixContainer(std::shared_ptr<detail::Container> container);

  // do not allow moves, it can leave a null shared_ptr
  PosixContainer(const PosixContainer&) = default;
  PosixContainer& operator=(const PosixContainer&) = default;

  void swap(PosixContainer& other) MAIDSAFE_NOEXCEPT {
    using std::swap;
    swap(container_, other.container_);
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, std::vector<ContainerInfo>> ListChildContainers(
      Token token, std::string prefix = std::string()) const {
    namespace arg = std::placeholders;
    using Handler = detail::AsyncHandler<Token, std::vector<ContainerInfo>>;

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);
    GetLatestEntries(
        std::bind(GetContainers{}, arg::_1, std::move(prefix)), std::move(handler));
    return result.get();
  }

    template<typename Token>
  detail::AsyncResultReturn<Token, ContainerInfo> GetChildContainerInfo(
      const std::string& key, Token token) const {
    namespace arg = std::placeholders;
    using Handler = detail::AsyncHandler<Token, ContainerInfo>;

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);
    GetLatestEntries(
        std::bind(
            GetContainerInfo{},
            arg::_1,
            detail::ContainerKey{container_->network().lock(), key}),
        std::move(handler));
    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, PosixContainer> CreateChildContainer(
      const std::string& key, Token token) const {
    using Handler = detail::AsyncHandler<Token, PosixContainer>;
    assert(container_ != nullptr);

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    auto coro = detail::MakeCoroutine<CreateContainerRoutine<Handler>>(
        container_,
        detail::ContainerKey{container_->network().lock(), key},
        std::make_shared<detail::Container>(container_->network(), container_->container_info()),
        std::move(handler));
    coro.Execute();

    return result.get();
  }

  PosixContainer OpenChildContainer(const ContainerInfo& child_info) const;

  template<typename Token>
  detail::AsyncResultReturn<Token, PosixContainer> OpenChildContainer(
      const std::string& key, Token token) const {
    namespace arg = std::placeholders;
    using Handler = detail::AsyncHandler<Token, PosixContainer>;
    assert(container_ != nullptr);

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    GetLatestEntries(
        std::bind(
            GetPosixContainer{},
            container_,
            arg::_1,
            detail::ContainerKey{container_->network().lock(), key}),
        std::move(handler));

    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, void> DeleteChildContainer(
      const ContainerInfo& child_info, Token token) const {
    namespace arg = std::placeholders;
    using Handler = detail::AsyncHandler<Token, void>;
    assert(container_ != nullptr);

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);
    detail::Container::UpdateLatestInstance(
        container_,
        std::bind(RemoveContainer{}, arg::_1, child_info),
        detail::MakeForwardingCallback(std::move(handler)));
    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, std::vector<Blob>> ListBlobs(
      Token token, std::string prefix = std::string()) const {
    namespace arg = std::placeholders;
    using Handler = detail::AsyncHandler<Token, std::vector<Blob>>;

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);
    GetLatestEntries(std::bind(GetBlobs{}, arg::_1, std::move(prefix)), std::move(handler));
    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, std::vector<Blob>> GetBlobHistory(
      const std::string& key, Token token) const {
    using Handler = detail::AsyncHandler<Token, std::vector<Blob>>;
    assert(container_ != nullptr);

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    auto coro = detail::MakeCoroutine<BlobHistoryRoutine<Handler>>(
        container_,
        detail::ContainerKey{container_->network().lock(), key},
        std::vector<detail::ContainerVersion>{},
        detail::ContainerInstance{},
        std::vector<Blob>{},
        std::move(handler));

    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, Blob> GetBlob(const std::string& key, Token token) const {
    namespace arg = std::placeholders;
    using Handler = detail::AsyncHandler<Token, Blob>;
    assert(container_ != nullptr);

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    GetLatestEntries(
        std::bind(
            GetBlobInternal{},
            arg::_1,
            detail::ContainerKey{container_->network().lock(), key}),
        std::move(handler));

    return result.get();
  }

  LocalBlob CreateLocalBlob() const;
  LocalBlob OpenLocalBlob(const Blob& blob) const;

  template<typename Token>
  detail::AsyncResultReturn<Token, LocalBlob> OpenLocalBlob(
      const std::string& key, Token token) const {
    namespace arg = std::placeholders;
    using Handler = detail::AsyncHandler<Token, LocalBlob>;
    assert(container_ != nullptr);

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    GetLatestEntries(
        std::bind(
            GetLocalBlob{},
            container_,
            arg::_1,
            detail::ContainerKey{container_->network().lock(), key}),
        std::move(handler));

    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, Blob> CopyBlob(
      const Blob& from, const std::string& to, Token token) const {
    namespace arg = std::placeholders;
    using Handler = detail::AsyncHandler<Token, Blob>;
    assert(container_ != nullptr);

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    detail::Container::UpdateLatestInstance(
        container_,
        std::bind(
            AddBlob{},
            container_,
            arg::_1,
            from,
            detail::ContainerKey{container_->network().lock(), to}),
        detail::MakeForwardingCallback(std::move(handler)));

    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, Blob> WriteBlob(
      LocalBlob& from, const std::string& to, Token token) const {
    using Handler = detail::AsyncHandler<Token, Blob>;
    assert(container_ != nullptr);

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    from.Commit(
        container_,
        detail::ContainerKey{container_->network().lock(), to},
        boost::none,
        detail::MakeForwardingCallback(std::move(handler)));

    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, Blob> UpdateBlob(
      LocalBlob& from, const Blob& to, Token token) const {
    using Handler = detail::AsyncHandler<Token, Blob>;

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    const auto& key = Blob::Detail::key(to);
    const auto& replace = Blob::Detail::blob(to);
    from.Commit(
        container_, key, replace, detail::MakeForwardingCallback(std::move(handler)));

    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, void> DeleteBlob(
      const Blob& blob, Token token) const {
    namespace arg = std::placeholders;
    using Handler = detail::AsyncHandler<Token, void>;
    assert(container_ != nullptr);

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);
    detail::Container::UpdateLatestInstance(
        container_,
        std::bind(RemoveBlob{}, arg::_1, blob),
        detail::MakeForwardingCallback(std::move(handler)));
    return result.get();
  }

 private:
  template<typename Convert, typename Handler>
  void GetLatestEntries(Convert convert, Handler handler) const {
    auto coro = detail::MakeCoroutine<GetLatestEntriesRoutine<Convert, Handler>>(
        container_,
        std::vector<detail::ContainerVersion>{},
        detail::ContainerInstance{},
        std::move(convert),
        std::move(handler));
    coro.Execute();
  }

  template<typename Handler>
  struct CreateContainerRoutine {
    struct Frame {
      std::shared_ptr<detail::Container> container;
      detail::ContainerKey new_key;
      std::shared_ptr<detail::Container> new_container;
      Handler handler;
    };

    void operator()(detail::Coroutine<CreateContainerRoutine, Frame>& coro) const {
      using detail::operation;
      namespace arg = std::placeholders;
      namespace action = detail::action;
      assert(coro.frame().container != nullptr);
      assert(coro.frame().new_container != nullptr);

      ASIO_CORO_REENTER(coro) {
        // First entry is always an empty container
        ASIO_CORO_YIELD
          detail::Container::PutInstance(
              coro.frame().new_container,
              boost::none,
              detail::ContainerInstance{},
              operation
                .OnSuccess(action::Ignore<detail::ContainerVersion>().Then(action::Resume(coro)))
                .OnFailure(action::Abort(coro)));

        detail::Container::UpdateLatestInstance(
            coro.frame().container,
            std::bind(
                AddContainer{},
                arg::_1,
                std::move(coro.frame().new_key),
                std::move(coro.frame().new_container)),
            detail::MakeForwardingCallback(std::move(coro.frame().handler)));
      }
    }
  };

  template<typename Handler>
  struct BlobHistoryRoutine {
    struct Frame {
      std::shared_ptr<detail::Container> container;
      detail::ContainerKey key;
      std::vector<detail::ContainerVersion> history;
      detail::ContainerInstance instance;
      std::vector<Blob> result;
      Handler handler;
    };

    void operator()(detail::Coroutine<BlobHistoryRoutine, Frame>& coro) const {
      using detail::operation;
      namespace action = detail::action;
      assert(coro.frame().local_blob != nullptr);

      ASIO_CORO_REENTER(coro) {
        ASIO_CORO_YIELD
          detail::Container::GetVersions(
              coro.frame().container,
              operation
                .OnSuccess(
                    action::Store(std::ref(coro.frame().history)).Then(action::Resume(coro)))
                .OnFailure(action::Abort(coro)));

        // quicker to continually pop_back(), so swap direction
        std::reverse(coro.frame().history.begin(), coro.frame().history.end());

        while (!coro.frame().history.empty()) {
          ASIO_CORO_YIELD
            detail::Container::GetInstance(
                coro.frame().container,
                std::move(coro.frame().history.back()),
                operation
                  .OnSuccess(
                      action::Store(std::ref(coro.frame().instance)).Then(action::Resume(coro)))
                  .OnFailure(action::Abort(coro)));

          auto blob = coro.frame().instance.GetBlob(coro.frame().key);
          if (blob) {
            if (coro.frame().result.empty() ||
                Blob::Detail::blob(coro.frame().result.back()) == *blob) {
              coro.frame().result.emplace_back(coro.frame().key, std::move(*blob));
            }
            coro.frame().history.pop_back();
          } else {
            coro.frame().history.clear();
          }
        }

        coro.frame().handler(std::move(coro.frame().result));
      }
    }
  };

  template<typename ConversionFunction, typename Handler>
  struct GetLatestEntriesRoutine {
    struct Frame {
      std::shared_ptr<detail::Container> container;
      std::vector<detail::ContainerVersion> history;
      detail::ContainerInstance instance;
      ConversionFunction convert;
      Handler handler;
    };

    void operator()(detail::Coroutine<GetLatestEntriesRoutine, Frame>& coro) const {
      using detail::operation;
      namespace action = detail::action;
      assert(coro.frame().container != nullptr);

      ASIO_CORO_REENTER(coro) {
        ASIO_CORO_YIELD
          detail::Container::GetVersions(
              coro.frame().container,
              operation
                .OnSuccess(
                    action::Store(std::ref(coro.frame().history)).Then(action::Resume(coro)))
                .OnFailure(action::Abort(coro)));
        if (coro.frame().history.empty()) {
          coro.frame().handler(
              boost::make_unexpected(make_error_code(CommonErrors::no_such_element)));
          return;
        }

        ASIO_CORO_YIELD
          detail::Container::GetInstance(
              coro.frame().container,
              std::move(coro.frame().history.front()),
              operation
                .OnSuccess(
                    action::Store(std::ref(coro.frame().instance)).Then(action::Resume(coro)))
                .OnFailure(action::Abort(coro)));

        coro.frame().handler(coro.frame().convert(std::move(coro.frame().instance)));
      }
    }
  };

 private:
  struct GetContainers {
    std::vector<ContainerInfo> operator()(
        const detail::ContainerInstance& instance, const std::string& prefix) const;
  };
  struct GetPosixContainer {
    Expected<PosixContainer> operator()(
        const std::shared_ptr<detail::Container>& container,
        const detail::ContainerInstance& instance,
        const detail::ContainerKey& key) const;
  };
  struct GetContainerInfo {
    Expected<ContainerInfo> operator()(
        const detail::ContainerInstance& instance, const detail::ContainerKey& key) const;
  };
  struct AddContainer {
    Expected<PosixContainer> operator()(
        detail::ContainerInstance& instance,
        detail::ContainerKey new_key,
        const std::shared_ptr<detail::Container>& new_container) const;
  };
  struct RemoveContainer {
    Expected<void> operator()(
        detail::ContainerInstance& instance, const ContainerInfo& child_info) const;
  };

  struct GetBlobs {
    std::vector<Blob> operator()(
        const detail::ContainerInstance& instance, const std::string& prefix) const;
  };
  struct GetBlobInternal {
    Expected<Blob> operator()(
        const detail::ContainerInstance& instance, const detail::ContainerKey& key) const;
  };
  struct GetLocalBlob {
    Expected<LocalBlob> operator()(
        const std::shared_ptr<detail::Container>& container,
        const detail::ContainerInstance& instance,
        const detail::ContainerKey& key) const;
  };
  struct AddBlob {
    Expected<Blob> operator()(
        const std::shared_ptr<detail::Container>& container,
        detail::ContainerInstance& instance,
        const Blob& from,
        detail::ContainerKey to) const;
  };
  struct RemoveBlob {
    Expected<void> operator()(detail::ContainerInstance& instance, const Blob& remove) const;
  };

 private:
  std::shared_ptr<detail::Container> container_;
};

inline void swap(PosixContainer& lhs, PosixContainer& rhs) MAIDSAFE_NOEXCEPT {
  lhs.swap(rhs);
}
}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_CONTAINER_H_
