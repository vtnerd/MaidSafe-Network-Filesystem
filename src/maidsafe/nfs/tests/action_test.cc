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
#include "maidsafe/nfs/detail/coroutine.h"

namespace maidsafe {
namespace nfs {
namespace detail {
namespace test {

template<typename Value>
struct Tracker {
  MOCK_METHOD1_T(Invoked, void(Value));
};

template<typename Value>
struct Handler {
  template<typename Arg>
  void operator()(Arg&& arg) const {
    tracker.get().Invoked(std::forward<Arg>(arg));
  }
  std::reference_wrapper<Tracker<Value>> tracker;
};

template<typename Value>
Handler<Value> MakeHandler(Tracker<Value>& tracker) {
  return Handler<Value>{tracker};
}

template<typename Value>
struct DummyRoutine {
  struct Frame {
    Handler<Value> handler;
  };
  void operator()(Coroutine<DummyRoutine, Frame>& coro) const {
    coro.frame().handler(Value{});
  }
};

template<typename Value>
Coroutine<DummyRoutine<Value>, typename DummyRoutine<Value>::Frame> MakeDummyCoroutine(
    Tracker<Value>& tracker) {
  return MakeCoroutine<DummyRoutine<Value>>(MakeHandler(tracker));
}

namespace {
template<typename Callback, typename... Args>
void Execute(Callback callback, Args&&... args) {
  callback(std::forward<Args>(args)...);
}
}  // namespace

TEST(Action, BEH_Abort) {
  const auto error = std::make_error_code(std::errc::operation_canceled);

  auto tracker = make_unique<Tracker<Expected<int>>>();
  EXPECT_CALL(*tracker, Invoked(Expected<int>(boost::make_unexpected(error)))).Times(2);

  auto coro = MakeDummyCoroutine(*tracker);
  Execute(action::Abort(coro), error);
  Execute(action::Abort(coro), error);
}

TEST(Action, BEH_CallOnce) {
  const auto error = std::make_error_code(std::errc::operation_canceled);

  auto tracker = make_unique<Tracker<Expected<int>>>();
  EXPECT_CALL(*tracker, Invoked(Expected<int>(boost::make_unexpected(error)))).Times(1);

  auto coro = MakeDummyCoroutine(*tracker);
  std::once_flag once{};
  Execute(action::CallOnce(std::ref(once), action::Abort(coro)), error);
  Execute(action::CallOnce(std::ref(once), action::Abort(coro)), error);
}

TEST(Action, BEH_Resume) {
  using ::testing::_;

  auto tracker = make_unique<Tracker<Expected<int>>>();
  EXPECT_CALL(*tracker, Invoked(_)).Times(3);

  auto coro = MakeDummyCoroutine(*tracker);
  Execute(action::Resume(coro));
  Execute(action::Resume(coro).Then(action::Resume(coro)));
}

TEST(Action, BEH_Store) {
  {
    int value{};
    EXPECT_NE(5, value);
    Execute(action::Store(std::ref(value)), 5);
    EXPECT_EQ(5, value);
  }
  {
    const auto expected_value = "this is the string";
    boost::optional<std::string> value{};
    EXPECT_FALSE(static_cast<bool>(value));
    Execute(action::Store(std::ref(value)), expected_value);
    ASSERT_TRUE(static_cast<bool>(value));
    EXPECT_STREQ(expected_value, value->c_str());
  }
  {
    int value1{};
    int value2{};
    EXPECT_NE(10, value1);
    EXPECT_NE(20, value2);
    Execute(
        action::Store(std::ref(value1)).Then(std::bind(action::Store(std::ref(value2)), 20)), 10);
    EXPECT_EQ(10, value1);
    EXPECT_EQ(20, value2);
  }
}

}  // namespace test
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
