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
#ifndef MAIDSAFE_NFS_CONTAINER_VERSION_H_
#define MAIDSAFE_NFS_CONTAINER_VERSION_H_

#include "maidsafe/common/data_types/structured_data_versions.h"

namespace maidsafe {
namespace nfs {

using ContainerVersion = StructuredDataVersions::VersionName;

namespace detail {
inline ContainerVersion MakeContainerVersionRoot(ImmutableData::Name name) {
  return ContainerVersion{0, std::move(name)};
}

inline ContainerVersion MakeContainerVersionChild(
    const ContainerVersion& parent, ImmutableData::Name name) {
  return ContainerVersion{parent.index + 1, std::move(name)};
}
}  // namespace detail

}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_CONTAINER_VERSION_H_
