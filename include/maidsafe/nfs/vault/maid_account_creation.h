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

#ifndef MAIDSAFE_NFS_VAULT_MAID_ACCOUNT_CREATION_H_
#define MAIDSAFE_NFS_VAULT_MAID_ACCOUNT_CREATION_H_

#include <memory>
#include <string>

#include "maidsafe/common/config.h"
#include "maidsafe/common/types.h"

#include "maidsafe/passport/types.h"

namespace maidsafe {

namespace nfs_vault {

struct MaidAccountCreation {
  MaidAccountCreation();
  MaidAccountCreation(const passport::PublicMaid& public_maid,
                      const passport::PublicAnmaid& public_anmaid);
  explicit MaidAccountCreation(const std::string& serialised_copy);
  MaidAccountCreation(const MaidAccountCreation& other);
  MaidAccountCreation(MaidAccountCreation&& other);

  passport::PublicMaid public_maid() const { return *public_maid_ptr; }
  passport::PublicAnmaid public_anmaid() const { return *public_anmaid_ptr; }

  MaidAccountCreation& operator=(MaidAccountCreation other);
  std::string Serialise() const;

  friend bool operator==(const MaidAccountCreation& lhs, const MaidAccountCreation& rhs);
  friend void swap(MaidAccountCreation& lhs, MaidAccountCreation& rhs) MAIDSAFE_NOEXCEPT;

 private:
  std::unique_ptr<passport::PublicMaid> public_maid_ptr;
  std::unique_ptr<passport::PublicAnmaid> public_anmaid_ptr;
};

}  // namespace nfs_vault

}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_VAULT_MAID_ACCOUNT_CREATION_H_
