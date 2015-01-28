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
#ifndef MAIDSAFE_NFS_DETAIL_ACTION_ACTION_STORE_H_
#define MAIDSAFE_NFS_DETAIL_ACTION_ACTION_STORE_H_

#include <memory>
#include <utility>

#include "maidsafe/nfs/detail/action/action_continuation.h"

namespace maidsafe {
namespace nfs {
namespace detail {
namespace action {

template<typename Value>
class ActionStore : public ActionContinuation<ActionStore<Value>> {
 public:
  using ExpectedValue = Value;

  explicit ActionStore(std::reference_wrapper<Value> value)
    : ActionContinuation<ActionStore<Value>>(),
      value_(std::move(value)) {
  }

  ActionStore(const ActionStore&) = default;
  ActionStore(ActionStore&& other)
    : ActionContinuation<ActionStore<Value>>(std::move(other)),
      value_(std::move(other.value_)) {
  }

  template<typename InValue>
  void operator()(InValue&& in_value) const {
    value_.get() = std::forward<InValue>(in_value);
  }

 private:
  ActionStore& operator=(const ActionStore&) = delete;
  ActionStore& operator=(ActionStore&&) = delete;

 private:
  std::reference_wrapper<Value> value_;
};

template<typename Value>
inline
ActionStore<Value> Store(std::reference_wrapper<Value> value) {
  return ActionStore<Value>{std::move(value)};
}

}  // action
}  // detail
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_ACTION_ACTION_STORE_H_
