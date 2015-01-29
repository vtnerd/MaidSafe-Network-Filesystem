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
#include <functional>
#include <system_error>

#include "boost/optional.hpp"
#include "gmock/gmock.h"

#include "maidsafe/common/make_unique.h"
#include "maidsafe/common/test.h"
#include "maidsafe/nfs/detail/action.h"
#include "maidsafe/nfs/detail/operation_handler.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4510 4610)
#endif

namespace maidsafe {
namespace nfs {
namespace detail {
namespace test {

namespace {
template<typename Handler>
struct VoidHandler {
  using ExpectedValue = void;
  void operator()() { handler(); }
  Handler handler;
};

template<typename Handler>
VoidHandler<Handler> MakeVoidHandler(Handler handler) {
  return VoidHandler<Handler>{std::move(handler)};
}
}

TEST(OperationHandler, BEH_OnSuccess) {
  bool success = false;
  std::error_code failure{};

  auto callback = operation
    .OnSuccess(action::Store(std::ref(success)))
    .OnFailure(action::Store(std::ref(failure)));

  {
    EXPECT_FALSE(success);
    EXPECT_FALSE(static_cast<bool>(failure));

    callback(true);

    EXPECT_TRUE(success);
    EXPECT_FALSE(static_cast<bool>(failure));
  }
  success = false;
  failure = std::error_code{};
  {
    EXPECT_FALSE(success);
    EXPECT_FALSE(static_cast<bool>(failure));

    callback(true, failure);

    EXPECT_TRUE(success);
    EXPECT_FALSE(static_cast<bool>(failure));
  }
}

TEST(OperationHandler, BEH_OnFailure) {
  bool success = false;
  std::error_code failure{};

  std::error_code expected_error = std::make_error_code(std::errc::operation_canceled);

  auto callback = operation
    .OnSuccess(action::Store(std::ref(success)))
    .OnFailure(action::Store(std::ref(failure)));

  {
    EXPECT_FALSE(success);
    EXPECT_FALSE(static_cast<bool>(failure));

    callback(boost::make_unexpected(expected_error));

    EXPECT_FALSE(success);
    EXPECT_TRUE(static_cast<bool>(failure));
    EXPECT_EQ(expected_error, failure);
  }
  success = false;
  failure = std::error_code{};
  {
    EXPECT_FALSE(success);
    EXPECT_FALSE(static_cast<bool>(failure));

    callback(true, expected_error);

    EXPECT_FALSE(success);
    EXPECT_TRUE(static_cast<bool>(failure));
    EXPECT_EQ(expected_error, failure);
  }
}

TEST(OperationHandler, BEH_VoidValue) {
  bool success = false;
  std::error_code failure{};

  std::error_code expected_error = std::make_error_code(std::errc::operation_canceled);

  auto callback = operation
    .OnSuccess(MakeVoidHandler(std::bind(action::Store(std::ref(success)), true)))
    .OnFailure(action::Store(std::ref(failure)));

  {
    EXPECT_FALSE(success);
    EXPECT_FALSE(static_cast<bool>(failure));

    callback();

    EXPECT_TRUE(success);
    EXPECT_FALSE(static_cast<bool>(failure));
  }
  success = false;
  failure = std::error_code{};
  {
    EXPECT_FALSE(success);
    EXPECT_FALSE(static_cast<bool>(failure));

    callback(Expected<void>{boost::expect});

    EXPECT_TRUE(success);
    EXPECT_FALSE(static_cast<bool>(failure));
  }
  success = false;
  failure = std::error_code{};
  {
    EXPECT_FALSE(success);
    EXPECT_FALSE(static_cast<bool>(failure));

    callback(boost::make_unexpected(expected_error));

    EXPECT_FALSE(success);
    EXPECT_TRUE(static_cast<bool>(failure));
    EXPECT_EQ(expected_error, failure);
  }
  success = false;
  failure = std::error_code{};
  {
    EXPECT_FALSE(success);
    EXPECT_FALSE(static_cast<bool>(failure));

    callback(expected_error);

    EXPECT_FALSE(success);
    EXPECT_TRUE(static_cast<bool>(failure));
    EXPECT_EQ(expected_error, failure);
  }
}

TEST(OperationHandler, BEH_ValueConversion) {
  boost::optional<int> success{};
  std::error_code failure{};

  auto callback = operation
    .OnSuccess(action::Store(std::ref(success)))
    .OnFailure(action::Store(std::ref(failure)));

  {
    EXPECT_FALSE(static_cast<bool>(success));
    EXPECT_FALSE(static_cast<bool>(failure));

    callback(Expected<int>(10));

    EXPECT_TRUE(static_cast<bool>(success));
    EXPECT_EQ(10, *success);
    EXPECT_FALSE(static_cast<bool>(failure));
  }
}

}  // namespace test
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe

#ifdef _MSC_VER
#pragma warning(pop)
#endif
