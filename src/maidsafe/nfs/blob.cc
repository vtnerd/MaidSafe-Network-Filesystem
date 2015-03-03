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
#include "maidsafe/nfs/blob.h"

#include "maidsafe/common/error.h"

namespace maidsafe {
namespace nfs {
Expected<std::string>  Blob::data() const {
  if (detail_blob_.data_map().chunks.empty()) {
    return std::string(
        detail_blob_.data_map().content.begin(),
        detail_blob_.data_map().content.end());
  }

  return boost::make_unexpected(make_error_code(CommonErrors::cannot_exceed_limit));
}
}  // namespace nfs
}  // namespace maidsafe
