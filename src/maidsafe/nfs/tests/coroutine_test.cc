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
#include <memory>

#include "boost/optional.hpp"
#include "gmock/gmock.h"

#include "maidsafe/common/make_unique.h"
#include "maidsafe/common/test.h"
#include "maidsafe/nfs/detail/coroutine.h"

namespace maidsafe {
namespace nfs {
namespace detail {
namespace test {

namespace {
struct AsyncOperation {
  MOCK_METHOD0(Run, void());
};

template<typename Routine, typename Frame>
unsigned RunCoroutine(Coroutine<Routine, Frame>&& coro) {
  unsigned count{};
  while (!coro.is_complete()) {
    ++count;
    coro.Execute();
  }
  
  return count;
}
  
template<typename Routine, typename... Args>
unsigned RunCoroutine(Args&&... args) {
  return RunCoroutine(MakeCoroutine<Routine>(std::forward<Args>(args)...));
}
}  // namespace

TEST(Coroutine, BEH_Basic) {
  /// VS2013 cannot properly move here, so use shared_ptr
  auto async_operation = std::make_shared<AsyncOperation>();
  EXPECT_CALL(*async_operation, Run()).Times(1);
  
  struct TestRoutine {
    struct Frame {
      std::shared_ptr<AsyncOperation> async_operation;
    };

    void operator()(Coroutine<TestRoutine, Frame>& coro) const {
      ASIO_CORO_REENTER(coro) {
        ASIO_CORO_YIELD coro.frame().async_operation->Run();
      }
    }
  };

  EXPECT_EQ(2u, RunCoroutine<TestRoutine>(std::move(async_operation)));
}

TEST(Coroutine, BEH_Multiple) {
  // VS2013 cannot properly move here, so use shared_ptr
  auto async_operation = std::make_shared<AsyncOperation>();
  EXPECT_CALL(*async_operation, Run()).Times(2);
  
  struct TestRoutine {
    struct Frame {
      std::shared_ptr<AsyncOperation> async_operation;
      unsigned count;
    };

    void operator()(Coroutine<TestRoutine, Frame>& coro) const {
      ASIO_CORO_REENTER(coro) {
        EXPECT_EQ(0, (coro.frame().count)++);
        ASIO_CORO_YIELD coro.frame().async_operation->Run();

        EXPECT_EQ(1, (coro.frame().count)++);
        ASIO_CORO_YIELD coro.frame().async_operation->Run();

        EXPECT_EQ(2, (coro.frame().count)++);
      }
    }
  };

  EXPECT_EQ(3u, RunCoroutine<TestRoutine>(std::move(async_operation), 0u));
}

TEST(Coroutine, BEH_AlternateConstructor) {
  auto async_operation = make_unique<AsyncOperation>();
  EXPECT_CALL(*async_operation, Run()).Times(2);
  
  struct TestRoutine {
    struct Frame {
      explicit Frame(std::unique_ptr<AsyncOperation> async_operation_in)
        : async_operation(std::move(async_operation_in)),
          count(10) {
      }
      
      std::unique_ptr<AsyncOperation> async_operation;
      unsigned count;
    };

    void operator()(Coroutine<TestRoutine, Frame>& coro) const {
      ASIO_CORO_REENTER(coro) {
        EXPECT_EQ(10, (coro.frame().count)++);
        ASIO_CORO_YIELD coro.frame().async_operation->Run();

        EXPECT_EQ(11, (coro.frame().count)++);
        ASIO_CORO_YIELD coro.frame().async_operation->Run();

        EXPECT_EQ(12, coro.frame().count);
      }
    }
  };

  EXPECT_EQ(
      3u,
      RunCoroutine<TestRoutine>(
          Coroutine<TestRoutine, TestRoutine::Frame>{TestRoutine{}, std::move(async_operation)}));
}

TEST(Coroutine, BEH_FrameIsNotCopied) {
  // VS2013 cannot properly move here, so use shared_ptr
  auto async_operation = std::make_shared<AsyncOperation>();
  EXPECT_CALL(*async_operation, Run()).Times(1);
  
  struct TestRoutine {
    struct Frame {    
      std::shared_ptr<AsyncOperation> async_operation;
      unsigned count;
    };

    void operator()(Coroutine<TestRoutine, Frame>& coro) const {
      ASIO_CORO_REENTER(coro) {
        EXPECT_EQ(0, (coro.frame().count)++);
        ASIO_CORO_YIELD coro.frame().async_operation->Run();

        EXPECT_EQ(1, (coro.frame().count)++);
      }
    }
  };

  auto coro(MakeCoroutine<TestRoutine>(std::move(async_operation), 0u));
  coro.Execute();
  EXPECT_FALSE(coro.is_complete());

  auto coro2 = coro;
  EXPECT_NE(std::addressof(coro), std::addressof(coro2));
  
  coro2.Execute();
  EXPECT_FALSE(coro.is_complete());
  EXPECT_TRUE(coro2.is_complete());

  EXPECT_EQ(2u, coro.frame().count);
  EXPECT_EQ(coro.frame().count, coro2.frame().count);
}

}  // namespace test
}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
