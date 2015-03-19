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
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "boost/optional.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/error.h"
#include "maidsafe/common/unordered_map.h"
#include "maidsafe/encrypt/data_map.h"
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

  template<typename Update>
  using UpdatedResult =
    typename std::decay<
      typename std::result_of<Update(ContainerInstance&, ContainerVersion)>::type>::type;

 public:
  static MAIDSAFE_CONSTEXPR std::chrono::seconds GetRefreshInterval() {
    return std::chrono::seconds(kRefreshInterval);
  }

  // Create an entirely new container
  Container(std::weak_ptr<Network> network, ContainerInfo parent_info);

  // Construct existing container
  Container(
      std::weak_ptr<Network> network, ContainerInfo parent_info, ContainerInfo container_info);

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
       Expected<unspecified>(ContainerInstance, ContainerVersion)

    The function should update ContainerInstance referenced by ContainerVersion,
    and return unexpected on error. This function will retry updates if an
    unrelated change was made to the container. The value returned by the Update
    function is forwarded to the Token on success.
  */
  template<typename Update, typename Token>
  static AsyncResultReturn<Token, typename UpdatedResult<Update>::value_type> UpdateLatestInstance(
        std::shared_ptr<detail::Container> container, Update update, Token token) {
    using Handler = AsyncHandler<Token, typename UpdatedResult<Update>::value_type>;

    if (container == nullptr) {
      BOOST_THROW_EXCEPTION(MakeNullPointerException());
    }

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};

    auto coro = MakeCoroutine<UpdateInstanceRoutine<Update, Handler>>(
        std::move(container),
        std::vector<ContainerVersion>{},
        ContainerInstance{},
        UpdatedResult<Update>{},
        Expected<ContainerVersion>{},
        std::move(update),
        std::move(handler));
    coro.Execute();

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
              coro.frame().container->network().lock(),
              coro.frame().get_version.id,
              operation
                .OnSuccess(
                    action::Store(std::ref(coro.frame().encrypted_version))
                    .Then(action::Resume(coro)))
                .OnFailure(action::Abort(coro)));

        assert(coro.frame().encrypted_version);
        coro.frame().handler(
            coro.frame().container->DecryptAndCacheInstance(
                coro.frame().container->network().lock(),
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
  struct UpdateInstanceRoutine {    
    struct Frame {
      std::shared_ptr<detail::Container> container;
      std::vector<ContainerVersion> history;
      ContainerInstance instance;
      UpdatedResult<Update> update_result;
      Expected<ContainerVersion> put_result;
      Update update;
      Handler handler;
    };

    void operator()(Coroutine<UpdateInstanceRoutine, Frame>& coro) const {
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
          
          coro.frame().update_result =
            coro.frame().update(coro.frame().instance, coro.frame().history.front());
          if (!coro.frame().update_result) {
            coro.frame().handler(std::move(coro.frame().update_result));
            return;
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
            coro.frame().handler(boost::make_unexpected(coro.frame().put_result.error()));
            return;
          }
        } while (!coro.frame().put_result);

        coro.frame().handler(std::move(coro.frame().update_result));
      }
    }
  };

 private:
  Container(const Container&) = delete;
  Container(Container&&) = delete;

  Container& operator=(const Container&) = delete;
  Container& operator=(Container&&) = delete;

 private:
  static std::system_error MakeNullPointerException();
  static bool IsVersionError(const std::error_code& error);

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

  Expected<ImmutableData> EncryptVersion(const encrypt::DataMap& data_map) const;
  Expected<ContainerInstance> DecryptAndCacheInstance(
      std::shared_ptr<Network> network,
      ContainerVersion version,
      const ImmutableData& encrypted_version);

  maidsafe::unordered_map<ContainerVersion, const ContainerInstance>::iterator PruneCache(
      maidsafe::unordered_map<ContainerVersion, const ContainerInstance>::iterator prune_entry,
      const std::lock_guard<std::mutex>& /*data_mutex_*/);
  void PurgeVersionCache(const std::lock_guard<std::mutex>&) MAIDSAFE_NOEXCEPT;
  void PurgeInstanceCache(const std::lock_guard<std::mutex>&) MAIDSAFE_NOEXCEPT;

 private:
  const std::weak_ptr<Network> network_;

  std::vector<ContainerVersion> cached_versions_;
  maidsafe::unordered_map<ContainerVersion, const ContainerInstance> cached_instances_;

  const ContainerInfo parent_info_;
  const ContainerInfo container_info_;
  boost::optional<std::chrono::steady_clock::time_point> last_update_;
  mutable std::mutex data_mutex_;
};

}  // detail
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_CONTAINER_H_
