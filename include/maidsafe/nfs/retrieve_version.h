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
#ifndef MAIDSAFE_NFS_RETRIEVE_VERSION_H_
#define MAIDSAFE_NFS_RETRIEVE_VERSION_H_

#include "boost/optional.hpp"
#include "boost/throw_exception.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/error.h"

namespace maidsafe {
namespace nfs {

template<typename Version>
class RetrieveVersion {
 public:
  static RetrieveVersion Latest() { return RetrieveVersion(); }

  RetrieveVersion(Version version)
    : version_(std::move(version)) {
  }

  RetrieveVersion(const RetrieveVersion&) = default;
  RetrieveVersion(RetrieveVersion&& other)
    : version_(std::move(other.version_)) {
  }

  RetrieveVersion& operator=(RetrieveVersion other) MAIDSAFE_NOEXCEPT {
    swap(other);
    return *this;
  }

  void swap(RetrieveVersion& other) MAIDSAFE_NOEXCEPT {
    using std::swap;
    swap(version_, other.version_);
  }

  bool Equal(const RetrieveVersion& other) const {
    return version_ == other.version_;
  }

  bool Equal(const Version& other) const {
    if (version_) {
      return other == *version_;
    }
    return false;
  }

  // Throws if *this == Create() or *this == Latest()
  explicit operator Version&&() && { return std::move(version()); }

 private:
  RetrieveVersion() : version_() {}

  Version& version() {
    if (!version_) {
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::uninitialised));
    }
    return *version_;
  }

 private:
  boost::optional<Version> version_;
};

template<typename Version>
void swap(RetrieveVersion<Version>& lhs, RetrieveVersion<Version>& rhs) MAIDSAFE_NOEXCEPT {
  lhs.swap(rhs);
}

template<typename Version>
inline bool operator==(const RetrieveVersion<Version>& lhs, const RetrieveVersion<Version>& rhs) {
  return lhs.Equal(rhs);
}

template<typename Version>
inline bool operator==(const RetrieveVersion<Version>& lhs, const Version& rhs) {
  return lhs.Equal(rhs);
}

template<typename Version>
inline bool operator==(const Version& lhs, const RetrieveVersion<Version>& rhs) {
  return rhs.Equal(lhs);
}

template<typename Version>
inline bool operator!=(const RetrieveVersion<Version>& lhs, const RetrieveVersion<Version>& rhs) {
  return !lhs.Equal(rhs);
}

template<typename Version>
inline bool operator!=(const RetrieveVersion<Version>& lhs, const Version& rhs) {
  return !lhs.Equal(rhs);
}

template<typename Version>
inline bool operator!=(const Version& lhs, const RetrieveVersion<Version>& rhs) {
  return !rhs.Equal(lhs);
}

}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_RETRIEVE_VERSION_H_
