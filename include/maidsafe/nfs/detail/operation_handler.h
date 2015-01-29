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
#ifndef MAIDSAFE_NFS_DETAIL_OPERATION_HANDLER_H_
#define MAIDSAFE_NFS_DETAIL_OPERATION_HANDLER_H_

#include <system_error>
#include <type_traits>
#include <utility>

#include "boost/config.hpp"
#include "boost/mpl/identity.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/nfs/expected.h"

namespace maidsafe {
namespace nfs {
namespace detail {

template<typename SuccessRoutine = void*, typename FailureRoutine = void*>
class OperationHandler {
 private:
  template<typename Routine>
  using NotSet = std::is_same<void*, Routine>;

  template<typename Routine>
  struct GetExpectedType {
    using type = typename Routine::ExpectedValue;
  };

  // Tells boost::expected that the failure was handled (do not propagate).
  class FailureWrapper {
   public:
    MAIDSAFE_CONSTEXPR FailureWrapper() : failure_routine_() {}
    MAIDSAFE_CONSTEXPR explicit FailureWrapper(FailureRoutine failure_routine)
      : failure_routine_(std::move(failure_routine)) {
    }
    
    FailureWrapper(const FailureWrapper&) = default;
    FailureWrapper(FailureWrapper&& other)
      : failure_routine_(std::move(other.failure_routine_)) {
    }

    template<typename... Args>
    Expected<void> operator()(Args&&... args) {
      failure_routine_(std::forward<Args>(args)...);
      return Expected<void>{boost::expect};
    }

    const FailureRoutine& failure_routine() const { return failure_routine_; }
    FailureRoutine& failure_routine() { return failure_routine_; }

   private:
    FailureWrapper& operator=(const FailureWrapper&) = delete;
    FailureWrapper& operator=(FailureWrapper&&) = delete;

   private:
    FailureRoutine failure_routine_;
  };
 
 public:
  using ExpectedValue =
    typename std::conditional<
      NotSet<SuccessRoutine>::value,
      boost::mpl::identity<void>,
      GetExpectedType<SuccessRoutine>>::type::type;
 
  MAIDSAFE_CONSTEXPR OperationHandler() : success_routine_(), failure_wrapper_() {}
  MAIDSAFE_CONSTEXPR OperationHandler(
      SuccessRoutine success_routine,
      FailureRoutine failure_routine)
    : success_routine_(std::move(success_routine)),
      failure_wrapper_(std::move(failure_routine)) {
  }

#ifdef BOOST_NO_CXX11_REF_QUALIFIERS
  template<typename NewSuccessRoutine>
  OperationHandler<NewSuccessRoutine, FailureRoutine> OnSuccess(NewSuccessRoutine&& routine) const {
    static_assert(NotSet<SuccessRoutine>::value, "success routine already set");
    return {std::forward<NewSuccessRoutine>(routine), failure_wrapper_.failure_routine()};
  }

  template<typename NewFailureRoutine>
  OperationHandler<SuccessRoutine, NewFailureRoutine> OnFailure(NewFailureRoutine&& routine) const {
    static_assert(NotSet<FailureRoutine>::value, "failure routine already set");
    return {success_routine_, std::forward<NewFailureRoutine>(routine)};
  }
#else // CX11_REF_QUALIFIERS
  template<typename NewSuccessRoutine>
  OperationHandler<NewSuccessRoutine, FailureRoutine> OnSuccess(NewSuccessRoutine&& routine) const & {
    static_assert(NotSet<SuccessRoutine>::value, "success routine already set");
    return {std::forward<NewSuccessRoutine>(routine), failure_wrapper_.failure_routine()};
  }

  template<typename NewSuccessRoutine>
  OperationHandler<NewSuccessRoutine, FailureRoutine> OnSuccess(NewSuccessRoutine&& routine) && {
    static_assert(NotSet<SuccessRoutine>::value, "success routine already set");
    return {std::forward<NewSuccessRoutine>(routine), std::move(failure_wrapper_.failure_routine())};
  }

  template<typename NewFailureRoutine>
  OperationHandler<SuccessRoutine, NewFailureRoutine> OnFailure(NewFailureRoutine&& routine) const & {
    static_assert(NotSet<FailureRoutine>::value, "failure routine already set");
    return {success_routine_, std::forward<NewFailureRoutine>(routine)};
  }

  template<typename NewFailureRoutine>
  OperationHandler<SuccessRoutine, NewFailureRoutine> OnFailure(NewFailureRoutine&& routine) && {
    static_assert(NotSet<FailureRoutine>::value, "failure routine already set");
    return {std::move(success_routine_), std::forward<NewFailureRoutine>(routine)};
  }
#endif

  void operator()() {
    Handle(Expected<void>{boost::expect});
  }

  void operator()(const std::error_code error) {
    if (error) {
      Handle<void>(boost::make_unexpected(error));
    } else {
      Handle(Expected<void>{boost::expect});
    }
  }

  void operator()(Expected<ExpectedValue> expected_value) {
    Handle(std::move(expected_value));
  }

  template<typename Value>
  void operator()(Value&& value, const std::error_code error) {
    if (error) {
      Handle<Value>(boost::make_unexpected(error));
    } else {
      Handle<Value>(std::forward<Value>(value));
    }
  }

  template<typename Value>
  void operator()(Expected<Value> expect_value) {
    Handle(std::move(expect_value));
  }

 private:
 template<typename Value>
  void Handle(Expected<Value> expect_value) {
    static_assert(!NotSet<SuccessRoutine>::value, "success routine not set");
    static_assert(!NotSet<FailureRoutine>::value, "failure routine not set");
    static_assert(
        std::is_same<void, typename std::result_of<FailureRoutine(std::error_code)>::type>::value,
        "fail routine should not return value - or no such function call");
    static_assert(
        std::is_same<Expected<void>, decltype(expect_value.bind(success_routine_))>::value,
        "success routine should not return a value");

    expect_value.bind(success_routine_).catch_error(failure_wrapper_);
  }

 private:
  SuccessRoutine success_routine_;
  FailureWrapper failure_wrapper_;
};

MAIDSAFE_CONSTEXPR_OR_CONST OperationHandler<> operation{};

}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_OPERATION_HANDLER_H_
