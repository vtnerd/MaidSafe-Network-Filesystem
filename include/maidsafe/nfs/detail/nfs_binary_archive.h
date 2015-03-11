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
#ifndef MAIDSAFE_NFS_DETAIL_NFS_BINARY_ARCHIVE_H_
#define MAIDSAFE_NFS_DETAIL_NFS_BINARY_ARCHIVE_H_

#include "maidsafe/common/serialisation/binary_archive.h"

namespace maidsafe {
namespace nfs {
namespace detail {
class NfsOutputArchive : public cereal::OutputArchive<NfsOutputArchive> {
 public:
  template<typename... Args>
  NfsOutputArchive(Args&&... args)
    : cereal::OutputArchive<NfsOutputArchive>(this),
      real_archive_(std::forward<Args>(args)...) {
  }

  template<typename... Args>
  void saveBinary(Args&&... args) {
    real_archive_.saveBinary(std::forward<Args>(args)...);
  }

 private:
  BinaryOutputArchive real_archive_;
};

class NfsInputArchive : public cereal::InputArchive<NfsInputArchive> {
 public:
  template<typename... Args>
  NfsInputArchive(std::shared_ptr<Network> network, Args&&... args)
    : cereal::InputArchive<NfsInputArchive>(this),
      network_(std::move(network)),
      real_archive_(std::forward<Args>(args)...) {
  }

  template<typename... Args>
  void loadBinary(Args&&... args) {
    real_archive_.loadBinary(std::forward<Args>(args)...);
  }

  const std::shared_ptr<Network>& network() const { return network_; }

 private:
  const std::shared_ptr<Network> network_;
  BinaryInputArchive real_archive_;
};

 // Saving for POD types to binary
template <class T>
inline typename std::enable_if<std::is_arithmetic<T>::value, void>::type CEREAL_SAVE_FUNCTION_NAME(
    NfsOutputArchive& ar, T const& t) {
  ar.saveBinary(std::addressof(t), sizeof(t));
}

// Loading for POD types from binary
template <class T>
inline typename std::enable_if<std::is_arithmetic<T>::value, void>::type CEREAL_LOAD_FUNCTION_NAME(
    NfsInputArchive& ar, T& t) {
  ar.loadBinary(std::addressof(t), sizeof(t));
}

// Serializing NVP types to binary
template <class Archive, class T>
inline CEREAL_ARCHIVE_RESTRICT(NfsInputArchive, NfsOutputArchive)
    CEREAL_SERIALIZE_FUNCTION_NAME(Archive& ar, cereal::NameValuePair<T>& t) {
  ar(t.value);
}

// Serializing SizeTags to binary
template <class Archive, class T>
inline CEREAL_ARCHIVE_RESTRICT(NfsInputArchive, NfsOutputArchive)
    CEREAL_SERIALIZE_FUNCTION_NAME(Archive& ar, cereal::SizeTag<T>& t) {
  ar(t.size);
}

// Saving binary data
template <class T>
inline void CEREAL_SAVE_FUNCTION_NAME(NfsOutputArchive& ar, cereal::BinaryData<T> const& bd) {
  ar.saveBinary(bd.data, static_cast<std::size_t>(bd.size));
}

// Loading binary data
template <class T>
inline void CEREAL_LOAD_FUNCTION_NAME(NfsInputArchive& ar, cereal::BinaryData<T>& bd) {
  ar.loadBinary(bd.data, static_cast<std::size_t>(bd.size));
}
}  // detail
}  // nfs
}  // maidsafe

CEREAL_REGISTER_ARCHIVE(maidsafe::nfs::detail::NfsInputArchive)
CEREAL_REGISTER_ARCHIVE(maidsafe::nfs::detail::NfsOutputArchive)
CEREAL_SETUP_ARCHIVE_TRAITS(maidsafe::nfs::detail::NfsInputArchive, maidsafe::nfs::detail::NfsOutputArchive)

#endif  // MAIDSAFE_NFS_DETAIL_NFS_BINARY_ARCHIVE_H_
