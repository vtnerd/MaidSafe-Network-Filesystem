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
#ifndef MAIDSAFE_NFS_DETAIL_NETWORK_H_
#define MAIDSAFE_NFS_DETAIL_NETWORK_H_

#include <functional>
#include <utility>
#include <vector>

#include "boost/throw_exception.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/data_types/immutable_data.h"
#include "maidsafe/common/error.h"
#include "maidsafe/nfs/container_version.h"
#include "maidsafe/nfs/detail/async_result.h"
#include "maidsafe/nfs/detail/container_id.h"
#include "maidsafe/nfs/expected.h"


namespace maidsafe {
namespace nfs {
namespace detail {
namespace test { class MockBackend; }

class Network {
 private:
  friend class test::MockBackend;

  MAIDSAFE_CONSTEXPR_OR_CONST static std::uint32_t kMaxBranches = 1;
  MAIDSAFE_CONSTEXPR_OR_CONST static std::uint32_t kMaxVersions = 100;

 public:
  Network() = default;
  virtual ~Network();

  // Return max number of SDV versions stored on the network
  static MAIDSAFE_CONSTEXPR std::uint32_t GetMaxVersions() { return kMaxVersions; }

  template<typename Token>
  AsyncResultReturn<Token, void> CreateSDV(
      const ContainerId& container_id,
      const ContainerVersion& initial_version,
      Token token) {
    using Handler = AsyncHandler<Token, void>;

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);
    DoCreateSDV(std::move(handler), container_id, initial_version, kMaxVersions, kMaxBranches);
    return result.get();
  }

  template<typename Token>
  AsyncResultReturn<Token, void> PutSDVVersion(
      const ContainerId& container_id,
      const ContainerVersion& previous_version,
      const ContainerVersion& new_version,
      Token token) {
    using Handler = AsyncHandler<Token, void>;

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};
    DoPutSDVVersion(std::move(handler), container_id, previous_version, new_version);
    return result.get();
  }

  template<typename Token>
  static AsyncResultReturn<Token, std::vector<ContainerVersion>> GetSDVVersions(
      const std::shared_ptr<Network>& network, const ContainerId& container_id, Token token) {
    using Handler = AsyncHandler<Token, std::vector<ContainerVersion>>;

    if (network == nullptr) {
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::null_pointer));
    }

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};

    std::weak_ptr<Network> weak_network{network};
    network->DoGetBranches(
        [weak_network, container_id, handler]
        (Expected<std::vector<ContainerVersion>> branches) mutable {

          const std::shared_ptr<Network> network{weak_network.lock()};
          if (network == nullptr) {
            handler(boost::make_unexpected(make_error_code(std::errc::operation_canceled)));
            return;
          }

          if (!branches) {
            handler(boost::make_unexpected(branches.error()));
            return;
          }

          if (branches->size() == 1) {
            network->DoGetBranchVersions(
                std::move(handler), std::move(container_id), std::move(branches->front()));
          } else {
            /* A fork in the SDV. A bug in the code, or someone using rogue
               software. Do not alert via Expected, this should never
               happen currently. */
            BOOST_THROW_EXCEPTION(std::runtime_error("Unexpected fork in NFS SDV history"));
          }
        },
        container_id);

    return result.get();
  }

  template<typename Token>
  AsyncResultReturn<Token, void> PutChunk(const ImmutableData& data, Token token) {
    using Handler = AsyncHandler<Token, void>;

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};
    DoPutChunk(std::move(handler), data);
    return result.get();
  }

  template<typename Token>
  AsyncResultReturn<Token, ImmutableData> GetChunk(
      const ImmutableData::NameAndTypeId& name, Token token) {
    using Handler = AsyncHandler<Token, ImmutableData>;

    Handler handler{std::move(token)};
    asio::async_result<Handler> result{handler};
    DoGetChunk(std::move(handler), name);
    return result.get();
  }

 private:
  /* The callback function foreach virtual method is to be invoked when the
     operation completed, or failed. */
  virtual void DoCreateSDV(
      std::function<void(Expected<void>)> callback,
      const ContainerId& container_id,
      const ContainerVersion& initial_version,
      std::uint32_t max_versions,
      std::uint32_t max_branches) = 0;
  virtual void DoPutSDVVersion(
      std::function<void(Expected<void>)> callback,
      const ContainerId& container_id,
      const ContainerVersion& old_version,
      const ContainerVersion& new_version) = 0;
  virtual void DoGetBranches(
      std::function<void(Expected<std::vector<ContainerVersion>>)> callback,
      const ContainerId& container_id) = 0;
  virtual void DoGetBranchVersions(
      std::function<void(Expected<std::vector<ContainerVersion>>)> callback,
      const ContainerId& container_id,
      const ContainerVersion& tip) = 0;

  virtual void DoPutChunk(
      std::function<void(Expected<void>)> callback, const ImmutableData& data) = 0;
  virtual void DoGetChunk(
      std::function<void(Expected<ImmutableData>)> callback,
      const ImmutableData::NameAndTypeId& name) = 0;
};
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_NETWORK_H_
