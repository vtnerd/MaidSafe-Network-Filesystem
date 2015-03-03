/*  Copyright 2013 MaidSafe.net limited

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
#include "maidsafe/nfs/blob_version.h"

#include <type_traits>

namespace maidsafe {
namespace nfs {
static_assert(
    std::is_nothrow_copy_assignable<maidsafe::SHA512::Digest>::value,
    "No strong-exception guarantee for BlobVersion copy assignment operator");

BlobVersion::BlobVersion() : version_() {}
BlobVersion::BlobVersion(const maidsafe::SHA512::Digest& version) : version_(version) {}
bool BlobVersion::Equal(const BlobVersion& other) const { return other.version_ == version_; }

}  // namespace nfs
}  // namespace maidsafe
