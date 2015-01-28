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
#ifndef MAIDSAFE_NFS_DETAIL_COROUTINE_H_
#define MAIDSAFE_NFS_DETAIL_COROUTINE_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "asio/coroutine.hpp"

namespace maidsafe {
namespace nfs {
namespace detail {

template<typename Routine, typename Frame>
class Coroutine : public asio::coroutine {
 public:
  static_assert(
      std::is_same<void, typename std::result_of<Routine(Coroutine<Routine, Frame>&)>::type>::value,
      "routine should return void");

  Coroutine(Routine routine, Frame frame)
    : asio::coroutine(),
      frame_(std::make_shared<Frame>(std::move(frame))),
      routine_(std::move(routine)) {
  }

  template<typename... Args>
  Coroutine(Routine routine, Args&&... args)
    : asio::coroutine(),
      frame_(std::make_shared<Frame>(std::forward<Args>(args)...)),
      routine_(std::move(routine)) {
  }

  Coroutine(const Coroutine&) = default;
  Coroutine(Coroutine&& other)
    : asio::coroutine(std::move(other)),
      frame_(std::move(other.frame_)),
      routine_(std::move(other.routine_)) {
  }

  const Frame& frame() const { return *frame_; }
  Frame& frame() { return *frame_; }

  void Execute() {
    assert(!is_complete());
    routine_(*this);
  }

 private:
  Coroutine& operator=(const Coroutine&) = delete;
  Coroutine& operator=(Coroutine&&) = delete;

 private:
  const std::shared_ptr<Frame> frame_;
  Routine routine_;
};

template<typename Routine, typename... Args>
inline
Coroutine<Routine, typename Routine::Frame> MakeCoroutine(Args&&... args) {
  return {Routine{}, typename Routine::Frame{std::forward<Args>(args)...}};
}

}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_COROUTINE_H_
