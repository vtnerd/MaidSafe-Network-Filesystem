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
#ifndef MAIDSAFE_NFS_DETAIL_NETWORK_IMPLEMENTATION_H_
#define MAIDSAFE_NFS_DETAIL_NETWORK_IMPLEMENTATION_H_

#include <utility>
#include <vector>

#include "boost/thread/future.hpp"

#include "maidsafe/common/data_types/immutable_data.h"
#include "maidsafe/nfs/container_version.h"
#include "maidsafe/nfs/detail/container_id.h"
#include "maidsafe/nfs/detail/network_interface.h"

namespace maidsafe {
namespace nfs {
namespace detail {

// For legacy reasons, the network and disk versions are not using virtual dispatch
template<typename Backend>
class NetworkImplementation : public Network::Interface {
 public:
  template<typename... Args>
  explicit NetworkImplementation(Args... args)
    : Network::Interface(),
      backend_(std::forward<Args>(args)...) {
  }

  NetworkImplementation(NetworkImplementation&&) = default;
  NetworkImplementation& operator=(NetworkImplementation&&) = default;

  virtual ~NetworkImplementation() {}

 private:
  virtual boost::future<void> DoCreateSDV(
      const ContainerId& container_id,
      const ContainerVersion& initial_version,
      std::uint32_t max_versions,
      std::uint32_t max_branches) override final {
    return backend_.CreateVersionTree(container_id.data, initial_version, max_versions, max_branches);
  }
  virtual boost::future<void> DoPutSDVVersion(
      const ContainerId& container_id,
      const ContainerVersion& old_version,
      const ContainerVersion& new_version) override final {
    return backend_.PutVersion(container_id.data, old_version, new_version);
  }
  virtual boost::future<std::vector<ContainerVersion>> DoGetBranches(
      const ContainerId& container_id) override final {
    return backend_.GetVersions(container_id.data);
  }
  virtual boost::future<std::vector<ContainerVersion>> DoGetBranchVersions(
      const ContainerId& container_id, const ContainerVersion& tip) override final {
    return backend_.GetBranch(container_id.data, tip);
  }

  virtual boost::future<void> DoPutChunk(const ImmutableData& data) override final {
    return backend_.Put(data);
  }
  virtual boost::future<ImmutableData> DoGetChunk(const ImmutableData::Name& name) override final {
    return backend_.Get(name);
  }

 private:
  Backend backend_;
};

}  // detail
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_NETWORK_IMPLEMENTATION_H_
