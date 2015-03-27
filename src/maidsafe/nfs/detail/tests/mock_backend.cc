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
#include "maidsafe/nfs/detail/tests/mock_backend.h"

#include "maidsafe/common/error.h"

namespace maidsafe {
namespace nfs {
namespace detail {
namespace test {
MockBackend::MockBackend(std::shared_ptr<Network> real)
  : Network(),
    real_(std::move(real)) {
  if (real_ == nullptr) {
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::null_pointer));
  }
}

void MockBackend::SetDefaultDoCreateSDV() {
  using ::testing::_;
  using ::testing::Invoke;
  namespace arg = std::placeholders;
  ON_CALL(*this, DoCreateSDV(_, _, _, _, _))
    .WillByDefault(Invoke(real_.get(), &Network::DoCreateSDV));
}

void MockBackend::SetDefaultDoPutSDVVersion() {
  using ::testing::_;
  using ::testing::Invoke;
  namespace arg = std::placeholders;
  ON_CALL(*this, DoPutSDVVersion(_, _, _, _))
    .WillByDefault(Invoke(real_.get(), &Network::DoPutSDVVersion));
}

void MockBackend::SetDefaultDoGetBranches() {
  using ::testing::_;
  using ::testing::Invoke;
  ON_CALL(*this, DoGetBranches(_, _)).WillByDefault(Invoke(real_.get(), &Network::DoGetBranches));
}

void MockBackend::SetDefaultDoGetBranchVersions() {
  using ::testing::_;
  using ::testing::Invoke;
  ON_CALL(*this, DoGetBranchVersions(_, _, _))
    .WillByDefault(Invoke(real_.get(), &Network::DoGetBranchVersions));
}

void MockBackend::SetDefaultDoPutChunk() {
  using ::testing::_;
  using ::testing::Invoke;
  ON_CALL(*this, DoPutChunk(_, _)).WillByDefault(Invoke(real_.get(), &Network::DoPutChunk));
}

void MockBackend::SetDefaultDoGetChunk() {
  using ::testing::_;
  using ::testing::Invoke;
  ON_CALL(*this, DoGetChunk(_, _)).WillByDefault(Invoke(real_.get(), &Network::DoGetChunk));
}

void MockBackend::SetDefaults() {
  SetDefaultDoCreateSDV();
  SetDefaultDoPutSDVVersion();
  SetDefaultDoGetBranches();
  SetDefaultDoGetBranchVersions();
  SetDefaultDoPutChunk();
  SetDefaultDoGetChunk();
}

MockBackend::~MockBackend() {}

}  // namespace test
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
