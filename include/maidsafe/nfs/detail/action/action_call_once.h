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
#ifndef MAIDSAFE_NFS_DETAIL_ACTION_ACTION_CALL_ONCE_H_
#define MAIDSAFE_NFS_DETAIL_ACTION_ACTION_CALL_ONCE_H_

#include <memory>
#include <mutex>
#include <utility>

namespace maidsafe {
namespace nfs {
namespace detail {
namespace action {

template<typename Once, typename Callback>
class ActionCallOnce {
 public:
  using ExpectedValue = typename Callback::ExpectedValue;

  ActionCallOnce(Once once, Callback callback)
    : once_(std::move(once)),
      callback_(std::move(callback)) {
  }

  ActionCallOnce(const ActionCallOnce&) = default;
  ActionCallOnce(ActionCallOnce&& other)
    : once_(std::move(other.once_)),
      callback_(std::move(other.callback_)) {
  }

  template<typename... Values>
  void operator()(Values&&... values) {
    std::call_once(once_, callback_, std::forward<Values>(values)...);
  }

 private:
  ActionCallOnce& operator=(const ActionCallOnce&) = delete;
  ActionCallOnce& operator=(ActionCallOnce&&) = delete;

 private:
  Once once_;
  Callback callback_;
};

template<typename Once, typename Callback>
inline
ActionCallOnce<Once, Callback> CallOnce(Once once, Callback callback) {
  return {std::move(once), std::move(callback)};
}

}  // action
}  // detail
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_ACTION_ACTION_CALL_ONCE_H_
