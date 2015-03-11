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
#include "maidsafe/nfs/modify_blob_version.h"
#include "maidsafe/nfs/modify_container_version.h"
#include "maidsafe/nfs/retrieve_blob_version.h"
#include "maidsafe/nfs/retrieve_container_version.h"

namespace maidsafe {
namespace nfs {

class PosixContainer {
 public:
  explicit PosixContainer(std::shared_ptr<detail::Container> container);

  // do not allow moves, it can leave a null shared_ptr
  PosixContainer(const PosixContainer&) = default;
  PosixContainer& operator=(const PosixContainer&) = default;

  template<typename Token>
  detail::AsyncResultReturn<Token, std::vector<ContainerVersion>> GetVersions(Token token) const {
    using Handler = detail::AsyncHandler<Token, std::vector<ContainerVersion>>;

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);
    detail::Container::GetVersions(container_, detail::MakeForwardingCallback(std::move(handler)));
    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, std::vector<ContainerInfo>> GetContainers(
      RetrieveContainerVersion version, Token token) const {
    namespace arg = std::placeholders;
    using Handler = detail::AsyncHandler<Token, std::vector<ContainerInfo>>;

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);
    GetEntries(std::move(version), std::bind(ExtractContainers{}, arg::_1), std::move(handler));
    return result.get();
  }

  PosixContainer OpenContainer(ContainerInfo child_container_info) const;

  template<typename Token>
  detail::AsyncResultReturn<Token, PosixContainer> OpenContainer(
      const std::string& key, ModifyContainerVersion version, Token token) const {
    namespace arg = std::placeholders;
    using Handler = detail::AsyncHandler<Token, PosixContainer>;
    assert(container_ != nullptr);

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    detail::ContainerKey container_key{container_->network().lock(), key};

    if (version == ModifyContainerVersion::Create()) {
      auto coro = detail::MakeCoroutine<CreateContainerRoutine<Handler>>(
          container_,
          std::move(container_key),
          std::make_shared<detail::Container>(container_->network(), container_->container_info()),
          std::move(handler));
      coro.Execute();
    } else {
      GetEntries(
          version == ModifyContainerVersion::Latest() ?
            RetrieveContainerVersion::Latest() :
            RetrieveContainerVersion{static_cast<ContainerVersion>(std::move(version))},
          std::bind(GetContainer{}, container_, arg::_1, std::move(container_key)),
          std::move(handler));
    }

    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, ContainerVersion> DeleteContainer(
      const std::string& key, RetrieveContainerVersion version, Token token) const {
    namespace arg = std::placeholders;
    using Handler = detail::AsyncHandler<Token, ContainerVersion>;
    assert(container_ != nullptr);

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);
    detail::Container::UpdateLatestInstance(
        container_,
        std::bind(
            RemoveContainer{},
            arg::_1,
            detail::ContainerKey{container_->network().lock(), key},
            arg::_2,
            std::move(version)),
        detail::MakeForwardingCallback(std::move(handler)));
    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, std::vector<Blob>> GetBlobs(
      RetrieveContainerVersion version, Token token) const {
    namespace arg = std::placeholders;
    using Handler = detail::AsyncHandler<Token, std::vector<Blob>>;

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);
    GetEntries(std::move(version), std::bind(ExtractBlobs{}, arg::_1), std::move(handler));
    return result.get();
  }

  LocalBlob CreateLocalBlob() const;
  LocalBlob OpenBlob(const Blob& blob) const;

  template<typename Token>
  detail::AsyncResultReturn<Token, LocalBlob> OpenBlob(
      const std::string& key, RetrieveBlobVersion version, Token token) const {
    using Handler = detail::AsyncHandler<Token, LocalBlob>;
    assert(container_ != nullptr);

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    auto coro = detail::MakeCoroutine<OpenBlobRoutine<Handler>>(
        container_,
        detail::ContainerKey{container_->network().lock(), key},
        std::move(version),
        std::vector<ContainerVersion>{},
        detail::ContainerInstance{},
        detail::Blob{},
        std::move(handler));
    coro.Execute();

    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, Blob> Copy(
      const Blob& blob, std::string key, ModifyBlobVersion replace, Token token) const {
    using Handler = detail::AsyncHandler<Token, Blob>;

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    LocalBlob local_blob = OpenBlob(blob);
    Write(
        local_blob,
        std::move(key),
        std::move(replace),
        detail::MakeForwardingCallback(std::move(handler)));

    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, Blob> Write(
      LocalBlob& local_blob, std::string key, ModifyBlobVersion replace, Token token) const {
    using Handler = detail::AsyncHandler<Token, Blob>;

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    local_blob.Commit(
        container_,
        std::move(key),
        std::move(replace),
        detail::MakeForwardingCallback(std::move(handler)));

    return result.get();
  }

  template<typename Token>
  detail::AsyncResultReturn<Token, ContainerVersion> DeleteBlob(
      const std::string& key, RetrieveBlobVersion version, Token token) const {
    namespace arg = std::placeholders;
    using Handler = detail::AsyncHandler<Token, ContainerVersion>;
    assert(container_ != nullptr);

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);
    detail::Container::UpdateLatestInstance(
        container_,
        std::bind(
            RemoveBlob{},
            arg::_1,
            detail::ContainerKey{container_->network().lock(), key},
            std::move(version)),
        detail::MakeForwardingCallback(std::move(handler)));
    return result.get();
  }

 private:
  template<typename Convert, typename Handler>
  void GetEntries(RetrieveContainerVersion&& version, Convert convert, Handler handler) const {
    auto coro = detail::MakeCoroutine<GetEntriesRoutine<Convert, Handler>>(
        container_,
        std::move(version),
        std::vector<ContainerVersion>{},
        detail::ContainerInstance{},
        std::move(convert),
        std::move(handler));
    coro.Execute();
  }

  template<typename Handler>
  struct CreateContainerRoutine {
    struct Frame {
      std::shared_ptr<detail::Container> container;
      detail::ContainerKey key;
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
                .OnSuccess(action::Ignore<ContainerVersion>().Then(action::Resume(coro)))
                .OnFailure(action::Abort(coro)));

        ASIO_CORO_YIELD
          detail::Container::UpdateLatestInstance(
              coro.frame().container,
              std::bind(
                  AddContainer{},
                  arg::_1,
                  std::cref(coro.frame().key),
                  coro.frame().new_container->container_info()),
              operation
                .OnSuccess(action::Ignore<ContainerVersion>().Then(action::Resume(coro)))
                .OnFailure(action::Abort(coro)));

        coro.frame().handler(PosixContainer{std::move(coro.frame().new_container)});
      }
    }
  };

  template<typename Handler>
  struct OpenBlobRoutine {
    struct Frame {
      std::shared_ptr<detail::Container> container;
      detail::ContainerKey key;
      RetrieveBlobVersion version;
      std::vector<ContainerVersion> history;
      detail::ContainerInstance instance;
      detail::Blob head;
      Handler handler;
    };

    void operator()(detail::Coroutine<OpenBlobRoutine, Frame>& coro) const {
      using detail::operation;
      namespace action = detail::action;
      assert(coro.frame().container != nullptr);

      ASIO_CORO_REENTER(coro) {
        if (coro.frame().version == RetrieveBlobVersion::Latest()) {
          ASIO_CORO_YIELD
            detail::Container::GetVersions(
                coro.frame().container,
                operation
                  .OnSuccess(
                      action::Store(std::ref(coro.frame().history)).Then(action::Resume(coro)))
                  .OnFailure(action::Abort(coro)));
          if (coro.frame().history.empty()) {
            coro.frame().handler(
                Expected<LocalBlob>{
                  boost::make_unexpected(make_error_code(CommonErrors::no_such_element))});
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

          auto latest_version = coro.frame().instance.GetBlob(coro.frame().key);
          if (!latest_version) {
            coro.frame().handler(boost::make_unexpected(latest_version.error()));
            return;
          }

          coro.frame().version = RetrieveBlobVersion{std::move(latest_version)->version()};
        }

        assert(coro.frame().version != RetrieveBlobVersion::Latest());

        ASIO_CORO_YIELD
          detail::Container::GetBlob(
              coro.frame().container,
              std::move(coro.frame().key),
              static_cast<BlobVersion>(std::move(coro.frame().version)),
              operation
                .OnSuccess(action::Store(std::ref(coro.frame().head)).Then(action::Resume(coro)))
                .OnFailure(action::Abort(coro)));

        coro.frame().handler(
            LocalBlob{coro.frame().container->network(), std::move(coro.frame().head)});
      }
    }
  };

  template<typename ConversionFunction, typename Handler>
  struct GetEntriesRoutine {
    struct Frame {
      std::shared_ptr<detail::Container> container;
      RetrieveContainerVersion version;
      std::vector<ContainerVersion> history;
      detail::ContainerInstance instance;
      ConversionFunction convert;
      Handler handler;
    };

    void operator()(detail::Coroutine<GetEntriesRoutine, Frame>& coro) const {
      using detail::operation;
      namespace action = detail::action;
      assert(coro.frame().container != nullptr);

      ASIO_CORO_REENTER(coro) {
        if (coro.frame().version == RetrieveContainerVersion::Latest()) {
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

          coro.frame().version =
            RetrieveContainerVersion{std::move(coro.frame().history.front())};
        }

        assert(coro.frame().version != RetrieveContainerVersion::Latest());
        ASIO_CORO_YIELD
          detail::Container::GetInstance(
              coro.frame().container,
              static_cast<ContainerVersion>(std::move(coro.frame().version)),
              operation
                .OnSuccess(
                    action::Store(std::ref(coro.frame().instance)).Then(action::Resume(coro)))
                .OnFailure(action::Abort(coro)));

        coro.frame().handler(coro.frame().convert(std::move(coro.frame().instance)));
      }
    }
  };

 private:
  struct ExtractContainers {
    std::vector<ContainerInfo> operator()(const detail::ContainerInstance& instance) const;
  };
  struct GetContainer {
    Expected<PosixContainer> operator()(
        const std::shared_ptr<detail::Container>& container,
        const detail::ContainerInstance& instance,
        const detail::ContainerKey& key) const;
  };
  struct AddContainer {
    Expected<void> operator()(
        detail::ContainerInstance& instance,
        const detail::ContainerKey& key,
        const detail::ContainerInfo& new_container) const;
  };
  struct RemoveContainer {
    Expected<void> operator()(
        detail::ContainerInstance& instance,
        const detail::ContainerKey& key,
        const ContainerVersion& current_version,
        const RetrieveContainerVersion& replace) const;
  };
  struct ExtractBlobs {
    std::vector<Blob> operator()(const detail::ContainerInstance& instance) const;
  };
  struct RemoveBlob {
    Expected<void> operator()(
        detail::ContainerInstance& instance,
        const detail::ContainerKey& key,
        const RetrieveBlobVersion& replace) const;
  };

 private:
  const std::shared_ptr<detail::Container> container_;
};

}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_CONTAINER_H_
