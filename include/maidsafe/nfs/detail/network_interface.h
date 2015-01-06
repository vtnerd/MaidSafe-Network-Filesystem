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
#ifndef MAIDSAFE_NFS_DETAIL_NETWORK_INTERFACE_H_
#define MAIDSAFE_NFS_DETAIL_NETWORK_INTERFACE_H_

#include <utility>
#include <vector>

#include "boost/thread/future.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/data_types/immutable_data.h"
#include "maidsafe/nfs/container_version.h"
#include "maidsafe/nfs/detail/async_result.h"
#include "maidsafe/nfs/detail/container_id.h"
#include "maidsafe/nfs/expected.h"

namespace maidsafe {
namespace nfs {
namespace detail {

class NetworkInterface {
 private:
  MAIDSAFE_CONSTEXPR_OR_CONST static std::uint32_t kMaxBranches = 1;
  MAIDSAFE_CONSTEXPR_OR_CONST static std::uint32_t kMaxVersions = 100;

 public:
  virtual ~NetworkInterface() = 0;

  template<typename Token>
  AsyncResultReturn<Token, void> CreateSDV(
      const ContainerId& container_id,
      const ContainerVersion& initial_version,
      Token token) {
    using Handler = AsyncHandler<Token, void>;

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    DoCreateSDV(container_id, initial_version, kMaxVersions, kMaxBranches)
      .then(Bridge<Handler>{std::move(handler)});

    return result.get();
  }

  template<typename Token>
  AsyncResultReturn<Token, void> PutSDVVersion(
      const ContainerId& container_id,
      const ContainerVersion& previous_version,
      const ContainerVersion& new_version,
      Token token) {
    using Handler = AsyncHandler<Token, void>;

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    DoPutSDVVersion(container_id, previous_version, new_version)
      .then(Bridge<Handler>{std::move(handler)});

    return result.get();
  }

  template<typename Token>
  static AsyncResultReturn<Token, std::vector<ContainerVersion>> GetSDVVersions(
      const std::shared_ptr<NetworkInterface>& self,
      const ContainerId& container_id,
      Token token) {
    using Handler = AsyncHandler<Token, std::vector<ContainerVersion>>;

    if (self == nullptr) {
      BOOST_THROW_EXCEPTION(std::runtime_error("Unexpected nullptr"));
    }

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);
    
    self->DoGetBranches(container_id).then(
        [self, container_id, handler] (boost::future<std::vector<ContainerVersion>> future) mutable {
          const auto branches = ConvertToExpected(std::move(future));

          if (!branches) {
            handler(branches);
          } else {
            if (branches->size() == 1) {
              self->DoGetBranchVersions(container_id, branches->front())
                .then(Bridge<Handler>{std::move(handler)});
            } else {
              /* A fork in the SDV. A bug in the code, or someone using rogue
                 software. Do not alert via Expected, this should never
               happen currently. */
              BOOST_THROW_EXCEPTION(std::runtime_error("Unexpected fork in NFS SDV history"));
            }
          }
        });

    return result.get();
  }

  template<typename Token>
  AsyncResultReturn<Token, void> PutChunk(const ImmutableData& data, Token token) {
    using Handler = AsyncHandler<Token, void>;

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    DoPutChunk(data).then(Bridge<Handler>{std::move(handler)});

    return result.get();
  }

  template<typename Token>
  AsyncResultReturn<Token, ImmutableData> GetChunk(const ImmutableData::Name& name, Token token) {
    using Handler = AsyncHandler<Token, ImmutableData>;

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    DoGetChunk(name).then(Bridge<Handler>{std::move(handler)});

    return result.get();
  }

 private:
  template<typename Result>
  static Expected<Result> ConvertToExpected(boost::future<Result> result) {
    try {
      return result.get();
    } catch (const std::system_error& error) {
      return boost::make_unexpected(error.code());
    } catch (...) {
      throw;
    }
  }

  static Expected<void> ConvertToExpected(boost::future<void> result) {
    try {
      result.get();
      return Expected<void>(boost::expect);
    } catch (const std::system_error& error) {
      return boost::make_unexpected(error.code());
    } catch (...) {
      throw;
    }
  }

  /* After routing_v2 settles, the SAFE and disk backends can be
     re-written to support async_result. */
  template<typename Handler>
  class Bridge {
   public:
    Bridge(Handler handler) : handler_(std::move(handler)) {}

    Bridge(const Bridge&) = default;
    Bridge(Bridge&&) = default;

    template<typename Result>
    void operator()(boost::future<Result> result) {
      handler_(ConvertToExpected(std::move(result)));
    }

   private:
    Handler handler_;
  };

 private:
  virtual boost::future<void> DoCreateSDV(
      const ContainerId& container_id,
      const ContainerVersion& initial_version,
      std::uint32_t max_versions,
      std::uint32_t max_branches) = 0;
  virtual boost::future<void> DoPutSDVVersion(
      const ContainerId& container_id,
      const ContainerVersion& old_version,
      const ContainerVersion& new_version) = 0;
  virtual boost::future<std::vector<ContainerVersion>> DoGetBranches(
      const ContainerId& container_id) = 0;
  virtual boost::future<std::vector<ContainerVersion>> DoGetBranchVersions(
      const ContainerId& container_id, const ContainerVersion& tip) = 0;

  virtual boost::future<void> DoPutChunk(const ImmutableData& data) = 0;
  virtual boost::future<ImmutableData> DoGetChunk(const ImmutableData::Name& name) = 0;
};

}  // detail
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_NETWORK_INTERFACE_H_
