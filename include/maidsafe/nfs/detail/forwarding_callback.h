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
#ifndef MAIDSAFE_NFS_DETAIL_FORWARDING_CALLBACK_H_
#define MAIDSAFE_NFS_DETAIL_FORWARDING_CALLBACK_H_

#include <type_traits>
#include <utility>

namespace maidsafe {
namespace nfs {
namespace detail {

/* The result of asio::handler_type cannot be passed directly to another
   function leveraging asio::async_result. Otherwise, asio::async_result will
   try to get the future from a promise multiple times, etc. */
template<typename Callback>
class ForwardingCallback {
 public:
  explicit ForwardingCallback(Callback callback)
    : callback_(std::move(callback)) {
  }

  template<typename... Args>
  typename std::result_of<Callback(Args...)>::type operator()(Args&&... args) {
    return callback_(std::forward<Args>(args)...);
  }

 private:
  Callback callback_;
};

template<typename Callback>
inline ForwardingCallback<Callback> MakeForwardingCallback(Callback callback) {
  return ForwardingCallback<Callback>{std::move(callback)};
}

}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_FORWARDING_CALLBACK_H_
