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
#ifndef MAIDSAFE_NFS_DETAIL_CONTAINER_H_
#define MAIDSAFE_NFS_DETAIL_CONTAINER_H_

#include <chrono>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "boost/optional.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/error.h"
#include "maidsafe/common/unordered_map.h"
#include "maidsafe/common/unordered_set.h"
#include "maidsafe/encrypt/data_map.h"
#include "maidsafe/nfs/blob_version.h"
#include "maidsafe/nfs/container_version.h"
#include "maidsafe/nfs/detail/action/action_abort.h"
#include "maidsafe/nfs/detail/action/action_store.h"
#include "maidsafe/nfs/detail/action/action_resume.h"
#include "maidsafe/nfs/detail/async_result.h"
#include "maidsafe/nfs/detail/blob.h"
#include "maidsafe/nfs/detail/container_id.h"
#include "maidsafe/nfs/detail/container_instance.h"
#include "maidsafe/nfs/detail/container_key.h"
#include "maidsafe/nfs/detail/coroutine.h"
#include "maidsafe/nfs/detail/detail_fwd.h"
#include "maidsafe/nfs/detail/forwarding_callback.h"
#include "maidsafe/nfs/detail/operation_handler.h"
#include "maidsafe/nfs/detail/network.h"
#include "maidsafe/nfs/detail/network_data.h"
#include "maidsafe/nfs/expected.h"

namespace maidsafe {
namespace nfs {
namespace detail {

// All public methods are thread-safe
class Container {
 private:
  static MAIDSAFE_CONSTEXPR_OR_CONST std::uint16_t kRefreshInterval = 30;

 public:
  static MAIDSAFE_CONSTEXPR std::chrono::seconds GetRefreshInterval() {
    return std::chrono::seconds(kRefreshInterval);
  }

  // Create an entirely new container
  Container(std::weak_ptr<Network> network, ContainerInfo parent_info);

  // Construct existing container
  Container(std::weak_ptr<Network> network, ContainerInfo parent_info, ContainerInfo container_info);

  const std::weak_ptr<Network>& network() const { return network_; }
  const ContainerInfo& parent_info() const { return parent_info_; }
  const ContainerInfo& container_info() const { return container_info_; };

  template<typename Token>
  static AsyncResultReturn<Token, std::vector<ContainerVersion>> GetVersions(
      const std::shared_ptr<Container>& container, Token token) {
    using Handler = AsyncHandler<Token, std::vector<ContainerVersion>>;

    if (container == nullptr) {
      BOOST_THROW_EXCEPTION(MakeNullPointerException());
    }

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};

    auto cached_versions = container->GetCachedVersions();
    if (cached_versions) {
      handler(std::move(*cached_versions));
    } else {
      GetVersionsNoCache(container, std::move(handler));
    }

    return result.get();
  }

  template<typename Token>
  static AsyncResultReturn<Token, ContainerInstance> GetInstance(
      const std::shared_ptr<Container>& container,
      const ContainerVersion& get_version,
      Token token) {
    using Handler = AsyncHandler<Token, ContainerInstance>;

    if (container == nullptr) {
      BOOST_THROW_EXCEPTION(MakeNullPointerException());
    }

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};

    auto cached_instance = container->GetCachedInstance(get_version);
    if (cached_instance) {
      handler(std::move(*cached_instance));
    } else {
      auto coro = MakeCoroutine<GetInstanceRoutine<Handler>>(
          container, get_version, boost::optional<ImmutableData>{}, std::move(handler));
      coro.Execute();
    }

    return result.get();
  }

  /*
    This writes the given instance as a new version, whereas
    UpdateLatestInstance tries to fetch latest instance and update it.
   */
  template<typename Token>
  static AsyncResultReturn<Token, ContainerVersion> PutInstance(
      std::shared_ptr<Container> container,
      boost::optional<ContainerVersion> replace,
      ContainerInstance new_instance,
      Token token) {
    using Handler = AsyncHandler<Token, ContainerVersion>;

    if (container == nullptr) {
      BOOST_THROW_EXCEPTION(MakeNullPointerException());
    }

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};

    auto coro = MakeCoroutine<PutInstanceRoutine<Handler>>(
        std::move(container),
        std::move(replace),
        std::move(new_instance),
        encrypt::DataMap{},
        ImmutableData::Name{},
        ContainerVersion{},
        std::move(handler));
    coro.Execute();

    return result.get();
  }

  /*
    The signature of Update must be:
       Expected<void>(ContainerInstance, ContainerVersion)

    The function should update ContainerInstance referenced by ContainerVersion,
    and return unexpected on error. This function will retry updates if an
    unrelated change was made to the container.
  */
  template<typename Update, typename Token>
  static AsyncResultReturn<Token, ContainerVersion> UpdateLatestInstance(
      std::shared_ptr<detail::Container> container, Update update, Token token) {
    using Handler = AsyncHandler<Token, ContainerVersion>;

    if (container == nullptr) {
      BOOST_THROW_EXCEPTION(MakeNullPointerException());
    }

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};

    auto coro = MakeCoroutine<UpdateLatestInstanceRoutine<Update, Handler>>(
        std::move(container),
        std::vector<ContainerVersion>{},
        ContainerInstance{},
        Expected<ContainerVersion>{},
        std::move(update),
        std::move(handler));
    coro.Execute();

    return result.get();
  }

  template<typename Token>
  static AsyncResultReturn<Token, detail::Blob> GetBlob(
      const std::shared_ptr<Container>& container,
      const detail::ContainerKey& key,
      const BlobVersion& get_version,
      Token token) {
    using Handler = AsyncHandler<Token, detail::Blob>;

    if (container == nullptr) {
      BOOST_THROW_EXCEPTION(MakeNullPointerException());
    }

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};

    auto cached_blob = container->GetCachedBlob(key, get_version);
    if (cached_blob) {
      handler(std::move(*cached_blob));
    } else {
      auto coro = MakeCoroutine<GetBlobRoutine<Handler>>(
          container,
          key,
          get_version,
          std::vector<ContainerVersion>{},
          ContainerInstance{},
          std::move(handler));
      coro.Execute();
    }

    return result.get();
  }

 private:
  template<typename Handler>
  static void GetVersionsNoCache(std::shared_ptr<Container> container, Handler handler) {
    assert(container != nullptr);
    auto coro = MakeCoroutine<GetVersionsRoutine<Handler>>(
	std::move(container), std::vector<ContainerVersion>{}, std::move(handler));
    coro.Execute();
  }

 private:
  template<typename Handler>
  struct GetVersionsRoutine {
    struct Frame {
      std::shared_ptr<Container> container;
      std::vector<ContainerVersion> result;
      Handler handler;
    };

    void operator()(Coroutine<GetVersionsRoutine, Frame>& coro) const {
      assert(coro.frame().container != nullptr);

      ASIO_CORO_REENTER(coro) {
        ASIO_CORO_YIELD
          Network::GetSDVVersions(
              coro.frame().container->network_.lock(),
              coro.frame().container->container_info().GetId(),
              operation
                .OnSuccess(action::Store(std::ref(coro.frame().result)).Then(action::Resume(coro)))
                .OnFailure(action::Abort(coro)));

        coro.frame().container->UpdateCachedVersions(coro.frame().result);
        coro.frame().handler(std::move(coro.frame().result));
      }
    }
  };

  template<typename Handler>
  struct GetInstanceRoutine {
    struct Frame {
      std::shared_ptr<Container> container;
      ContainerVersion get_version;
      boost::optional<ImmutableData> encrypted_version;
      Handler handler;
    };

    void operator()(Coroutine<GetInstanceRoutine, Frame>& coro) const {
      assert(coro.frame().container != nullptr);

      ASIO_CORO_REENTER(coro) {
        ASIO_CORO_YIELD
          Network::GetChunk(
              coro.frame().container->network_.lock(),
              coro.frame().get_version.id,
              operation
                .OnSuccess(
                    action::Store(std::ref(coro.frame().encrypted_version))
                    .Then(action::Resume(coro)))
                .OnFailure(action::Abort(coro)));

        assert(coro.frame().encrypted_version);
        coro.frame().handler(
            coro.frame().container->DecryptAndCacheInstance(
                std::move(coro.frame().get_version),
                std::move(*(coro.frame().encrypted_version))));
      }
    }
  };

  template<typename Handler>
  struct PutInstanceRoutine {
    struct Frame {
      std::shared_ptr<Container> container;
      boost::optional<ContainerVersion> replace;
      ContainerInstance new_instance;
      encrypt::DataMap new_data_map;
      ImmutableData::Name new_version_reference;
      ContainerVersion new_version;
      Handler handler;
    };

    void operator()(Coroutine<PutInstanceRoutine, Frame>& coro) const {
      assert(coro.frame().container != nullptr);

      ASIO_CORO_REENTER(coro) {
        ASIO_CORO_YIELD {
          auto instance_serialised = coro.frame().new_instance.Serialise();
          NetworkData data{coro.frame().container->network_};
          try {
            data.encryptor().Write(
                reinterpret_cast<const char*>(instance_serialised.data()),
                instance_serialised.size(),
                0);
          } catch (const std::system_error& e) {
            coro.frame().handler(boost::make_unexpected(e.code()));
            return;
          }
          NetworkData::Store(
              std::move(data),
              coro.frame().container->network(),
              operation
                .OnSuccess(
                    action::Store(std::ref(coro.frame().new_data_map)).Then(action::Resume(coro)))
                .OnFailure(action::Abort(coro)));
        }  // yield

        ASIO_CORO_YIELD {
          auto encrypted_data_map =
            coro.frame().container->EncryptVersion(coro.frame().new_data_map);
          if (!encrypted_data_map) {
            coro.frame().handler(boost::make_unexpected(encrypted_data_map.error()));
            return;
          }
          coro.frame().new_version_reference = encrypted_data_map->name();
          Network::PutChunk(
              coro.frame().container->network().lock(),
              std::move(*encrypted_data_map),
              operation.OnSuccess(action::Resume(coro)).OnFailure(action::Abort(coro)));
        }  // yield

        if (!coro.frame().replace) {
          coro.frame().new_version =
            MakeContainerVersionRoot(std::move(coro.frame().new_version_reference));

          ASIO_CORO_YIELD
            Network::CreateSDV(
                coro.frame().container->network().lock(),
                coro.frame().container->container_info().GetId(),
                coro.frame().new_version,
                operation.OnSuccess(action::Resume(coro)).OnFailure(action::Abort(coro)));
        } else {
          coro.frame().new_version =
            MakeContainerVersionChild(
                *(coro.frame().replace), std::move(coro.frame().new_version_reference));

          ASIO_CORO_YIELD
            Network::PutSDVVersion(
                coro.frame().container->network().lock(),
                coro.frame().container->container_info().GetId(),
                *(coro.frame().replace),
                coro.frame().new_version,
                operation.OnSuccess(action::Resume(coro)).OnFailure(action::Abort(coro)));
        }

        coro.frame().container->AddNewCachedVersion(
            std::move(coro.frame().replace),
            coro.frame().new_version,
            std::move(coro.frame().new_instance));
        coro.frame().handler(std::move(coro.frame().new_version));
      }
    }
  };

  template<typename Update, typename Handler>
  struct UpdateLatestInstanceRoutine {
    struct Frame {
      std::shared_ptr<detail::Container> container;
      std::vector<ContainerVersion> history;
      ContainerInstance instance;
      Expected<ContainerVersion> put_result;
      Update update;
      Handler handler;
    };

    void operator()(Coroutine<UpdateLatestInstanceRoutine, Frame>& coro) const {
      assert(coro.frame().container != nullptr);

      ASIO_CORO_REENTER(coro) {
        // Retry if container version changes
        do {
          ASIO_CORO_YIELD
            GetVersionsNoCache(
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
            GetInstance(
                coro.frame().container,
                coro.frame().history.front(),
                operation
                  .OnSuccess(
                      action::Store(std::ref(coro.frame().instance)).Then(action::Resume(coro)))
                  .OnFailure(action::Abort(coro)));
          {
            auto update = coro.frame().update(coro.frame().instance, coro.frame().history.front());
            if (!update) {
              coro.frame().handler(boost::make_unexpected(update.error()));
              return;
            }
          }

          ASIO_CORO_YIELD
            PutInstance(
                coro.frame().container,
                std::move(coro.frame().history.front()),
                std::move(coro.frame().instance),
                action::Store(std::ref(coro.frame().put_result)).Then(action::Resume(coro)));
          // Always re-start here, success or fail

          // If version error, retry the update because it could be unrelated
          if (!coro.frame().put_result && !IsVersionError(coro.frame().put_result.error())) {
            coro.frame().handler(std::move(coro.frame().put_result));
            return;
          }
        } while (!coro.frame().put_result);

        coro.frame().handler(std::move(coro.frame().put_result));
      }
    }
  };

  template<typename Handler>
  struct GetBlobRoutine {
    struct Frame {
      std::shared_ptr<Container> container;
      detail::ContainerKey key;
      BlobVersion get_version;
      std::vector<ContainerVersion> version_history;
      ContainerInstance current_instance;
      Handler handler;
    };

    void operator()(Coroutine<GetBlobRoutine, Frame>& coro) const {
      assert(coro.frame().container != nullptr);

      ASIO_CORO_REENTER(coro) {
        ASIO_CORO_YIELD
          Container::GetVersions(
              coro.frame().container,
              operation
                .OnSuccess(
                    action::Store(std::ref(coro.frame().version_history))
                    .Then(action::Resume(coro)))
                .OnFailure(action::Abort(coro)));

        while (!coro.frame().version_history.empty()) {
          ASIO_CORO_YIELD
            Container::GetInstance(
                coro.frame().container,
                std::move(coro.frame().version_history.front()),
                operation
                  .OnSuccess(
                      action::Store(std::ref(coro.frame().current_instance))
                      .Then(action::Resume(coro)))
                  .OnFailure(action::Abort(coro)));

          auto blob = std::move(coro.frame().current_instance).GetBlob(coro.frame().key);
          if (blob && blob->version() == coro.frame().get_version) {
            coro.frame().handler(std::move(*blob));
            return;
          }

          coro.frame().version_history.erase(coro.frame().version_history.begin());
        }

        coro.frame().handler(
            boost::make_unexpected(make_error_code(CommonErrors::no_such_element)));
      }
    }
  };

 private:
  Container(const Container&) = delete;
  Container(Container&&) = delete;

  Container& operator=(const Container&) = delete;
  Container& operator=(Container&&) = delete;

 private:
  static bool IsVersionError(const std::error_code& error);
  static std::system_error MakeNullPointerException();

  boost::optional<std::vector<ContainerVersion>> GetCachedVersions() const;
  void UpdateCachedVersions(std::vector<ContainerVersion> versions);

  void AddNewCachedVersion(
      const boost::optional<ContainerVersion>& old_version,
      ContainerVersion new_version,
      ContainerInstance instance);
  void AddCachedInstance(
    ContainerVersion new_version,
    ContainerInstance instance,
    const std::lock_guard<std::mutex>& data_mutex_lock);

  boost::optional<ContainerInstance> GetCachedInstance(const ContainerVersion& version) const;
  boost::optional<detail::Blob> GetCachedBlob(
      const detail::ContainerKey& key, const BlobVersion& version) const;

  Expected<ImmutableData> EncryptVersion(const encrypt::DataMap& data_map) const;
  Expected<ContainerInstance> DecryptAndCacheInstance(
      ContainerVersion version, const ImmutableData& encrypted_version);

  maidsafe::unordered_map<ContainerVersion, const ContainerInstance>::iterator PruneCache(
      maidsafe::unordered_map<ContainerVersion, const ContainerInstance>::iterator prune_entry,
      const std::lock_guard<std::mutex>& /*data_mutex_*/);
  void PurgeVersionCache(const std::lock_guard<std::mutex>&) MAIDSAFE_NOEXCEPT;
  void PurgeInstanceCache(const std::lock_guard<std::mutex>&) MAIDSAFE_NOEXCEPT;

 private:
  const std::weak_ptr<Network> network_;

  std::vector<ContainerVersion> cached_versions_;
  maidsafe::unordered_map<ContainerVersion, const ContainerInstance> cached_instances_;
  maidsafe::unordered_map<BlobVersion, maidsafe::unordered_set<ContainerVersion>> cached_blobs_;

  const ContainerInfo parent_info_;
  const ContainerInfo container_info_;
  boost::optional<std::chrono::steady_clock::time_point> last_update_;
  mutable std::mutex data_mutex_;
};

}  // detail
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_CONTAINER_H_
