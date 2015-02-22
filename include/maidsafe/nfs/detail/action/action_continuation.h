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
#ifndef MAIDSAFE_NFS_DETAIL_ACTION_ACTION_CONTINUATION_H_
#define MAIDSAFE_NFS_DETAIL_ACTION_ACTION_CONTINUATION_H_

#include <type_traits>
#include <utility>

#include "boost/config.hpp"

namespace maidsafe {
namespace nfs {
namespace detail {
namespace action {

template<typename Derived>
class ActionContinuation {
 public:
  template<typename Continuation>
  class Wrapper {
   public:
    using ExpectedValue = typename Derived::ExpectedValue;

    Wrapper(Derived derived, Continuation continuation)
      : derived_(std::move(derived)), continuation_(std::move(continuation)) {
    }

    Wrapper(const Wrapper&) = default;
    Wrapper(Wrapper&& other)
      : derived_(std::move(other.derived_)),
        continuation_(std::move(other.continuation_)) {
    }

    template<typename... Args>
    typename std::result_of<Continuation()>::type operator()(Args&&... args) {
      static_assert(
          std::is_same<void, typename std::result_of<Derived(Args...)>::type>::value,
          "derived returns value, and the result is unused. consider "
          "overload that sends to continuation");
      derived_(std::forward<Args>(args)...);
      return continuation_();
    }

   private:
    Wrapper& operator=(const Wrapper&) = delete;
    Wrapper& operator=(Wrapper&&) = delete;

   private:
    Derived derived_;
    Continuation continuation_;
  };

#ifdef BOOST_NO_CXX11_REF_QUALIFIERS
  template<typename Continuation>
  Wrapper<Continuation> Then(Continuation continuation) const {
    return {*self(), std::move(continuation)};
  }
#else // CX11_REF_QUALIFIERS
  template<typename Continuation>
  Wrapper<Continuation> Then(Continuation continuation) const & {
    return {*self(), std::move(continuation)};
  }

  template<typename Continuation>
  Wrapper<Continuation> Then(Continuation continuation) && {
    return {std::move(*self()), std::move(continuation)};
  }
#endif

 private:
  Derived* self() { return static_cast<Derived*>(this); }
  const Derived* self() const { return static_cast<const Derived*>(this); }
};

}  // action
}  // detail
}  // nfs
}  // maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_ACTION_ACTION_CONTINUATION_H_
