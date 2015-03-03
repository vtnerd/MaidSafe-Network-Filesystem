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
#include "maidsafe/nfs/detail/container_key.h"

#include "maidsafe/common/crypto.h"
#include "maidsafe/common/serialisation/serialisation.h"

namespace maidsafe {
namespace nfs {
namespace detail {

/* Keep constructor, destructor, and load methods in cc file. These
   instantiate a templated singleton object for flyweight, and the easiest
   way to keep these in maidsafe DSOs is to keep them in maidsafe TU. Otherwise,
   multiple flyweight registries will exist.*/

ContainerKey::ContainerKey() : value_() {}
ContainerKey::ContainerKey(std::string key) : value_(std::move(key)) {}
ContainerKey::~ContainerKey() {}

template<typename Archive>
Archive& ContainerKey::load(Archive& archive) {
  std::string value;
  archive(value);
  value_ = std::move(value);
  return archive;
}

template BinaryInputArchive& ContainerKey::load<BinaryInputArchive>(BinaryInputArchive&);

}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
