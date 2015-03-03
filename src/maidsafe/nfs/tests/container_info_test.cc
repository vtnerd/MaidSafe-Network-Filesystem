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
#include <utility>

#include "maidsafe/common/serialisation/serialisation.h"
#include "maidsafe/common/test.h"
#include "maidsafe/nfs/detail/container_info.h"

TEST(ContainerInfo, BEH_SameId) {
  maidsafe::nfs::detail::ContainerInfo key{};

  EXPECT_EQ(key.GetId(), key.GetId());
}

TEST(ContainerInfo, BEH_DifferentId) {
  maidsafe::nfs::detail::ContainerInfo key1{};
  maidsafe::nfs::detail::ContainerInfo key2{};

  EXPECT_NE(key1.GetId(), key2.GetId());
}

TEST(ContainerInfo, BEH_Serialize) {
  maidsafe::SerialisedData data;
  maidsafe::Identity inner_key{};
  maidsafe::nfs::detail::ContainerId id{};
  {
    maidsafe::nfs::detail::ContainerInfo key{};
    id = key.GetId();
    inner_key = key.key();
    data = maidsafe::Serialise(key);
  }

  maidsafe::nfs::detail::ContainerInfo revived_key{};
  maidsafe::Parse(data, revived_key);

  EXPECT_EQ(inner_key, revived_key.key());
  EXPECT_EQ(id, revived_key.GetId());
}

TEST(ContainerInfo, BEH_CopyConstructor) {
  maidsafe::nfs::detail::ContainerInfo key1{};
  maidsafe::nfs::detail::ContainerInfo key2{key1};

  EXPECT_EQ(key1.key(), key2.key());
  EXPECT_EQ(key1.GetId(), key2.GetId());
}

TEST(ContainerInfo, BEH_Assignment) {
  maidsafe::nfs::detail::ContainerInfo key1{};
  maidsafe::nfs::detail::ContainerInfo key2{};

  EXPECT_NE(key1.key(), key2.key());
  EXPECT_NE(key1.GetId(), key2.GetId());

  key2 = key1;

  EXPECT_EQ(key1.key(), key2.key());
  EXPECT_EQ(key1.GetId(), key2.GetId());
}

TEST(ContainerInfo, BEH_Swap) {
  maidsafe::nfs::detail::ContainerInfo key1{};
  maidsafe::nfs::detail::ContainerInfo key2{};
  const auto inner_key1 = key1.key();
  const auto inner_key2 = key2.key();
  const auto container_id1 = key1.GetId();
  const auto container_id2 = key2.GetId();

  {
    using std::swap;
    swap(key1, key2);
  }

  EXPECT_NE(inner_key1, inner_key2);
  EXPECT_NE(container_id1, container_id2);
  EXPECT_EQ(inner_key1, key2.key());
  EXPECT_EQ(inner_key2, key1.key());
  EXPECT_EQ(container_id1, key2.GetId());
  EXPECT_EQ(container_id2, key1.GetId());
}
