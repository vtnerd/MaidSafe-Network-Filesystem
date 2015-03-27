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
#ifndef MAIDSAFE_NFS_TESTS_NETWORK_FIXTURE_H_
#define MAIDSAFE_NFS_TESTS_NETWORK_FIXTURE_H_

#include <functional>
#include <memory>

#include "maidsafe/common/test.h"
#include "maidsafe/nfs/detail/network.h"
#include "maidsafe/nfs/detail/tests/mock_backend.h"

namespace maidsafe {
namespace nfs {
namespace detail {
namespace test {

class NetworkFixture {
 public:
  NetworkFixture();
  virtual ~NetworkFixture();

  // Create a NetworkInterface instance, as selected by command args
  static std::shared_ptr<Network> Create();

  const MockBackend& GetNetworkMock() const { return *network_; }
  MockBackend& GetNetworkMock() { return *network_; }
  std::shared_ptr<Network> network() const { return network_; }

 private:
  const std::shared_ptr<MockBackend> network_;
};

}  // namespace test
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_TESTS_NETWORK_FIXTURE_H_
