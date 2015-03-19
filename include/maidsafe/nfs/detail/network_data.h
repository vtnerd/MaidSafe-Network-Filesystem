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
#ifndef MAIDSAFE_NFS_DETAIL_NETWORK_DATA_H_
#define MAIDSAFE_NFS_DETAIL_NETWORK_DATA_H_

#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>

#include "maidsafe/common/data_buffer.h"
#include "maidsafe/common/hash/hash_vector.h"
#include "maidsafe/common/types.h"
#include "maidsafe/encrypt/data_map.h"
#include "maidsafe/encrypt/data_map_encryptor.h"
#include "maidsafe/encrypt/self_encryptor.h"
#include "maidsafe/nfs/detail/action/action_abort.h"
#include "maidsafe/nfs/detail/action/action_call_once.h"
#include "maidsafe/nfs/detail/action/action_resume.h"
#include "maidsafe/nfs/detail/async_result.h"
#include "maidsafe/nfs/detail/coroutine.h"
#include "maidsafe/nfs/detail/operation_handler.h"
#include "maidsafe/nfs/detail/network.h"
#include "maidsafe/nfs/expected.h"

namespace maidsafe {
namespace nfs {
namespace detail {

/* SelfEncryptor requires live references for DataBuffer and DataMap.
   This ensures valid references. This will store the data for a Blob OR
   serialised Container. */
class NetworkData {
 public:
  using Buffer = DataBuffer<std::string>;  // can reduce network traffic (local ram/disk cache)

  static std::shared_ptr<Buffer> MakeBuffer(const std::weak_ptr<Network>& network);
  static Expected<ImmutableData> GetChunk(
      Buffer& buffer, const std::weak_ptr<Network>& network, std::string raw_key);

  // New/empty network data
  explicit NetworkData(std::weak_ptr<Network> network);
  NetworkData(std::shared_ptr<Buffer> buffer, std::weak_ptr<Network> network);

  // Existing network data
  NetworkData(encrypt::DataMap existing_data, std::weak_ptr<Network> network);
  NetworkData(
      encrypt::DataMap existing_data,
      std::shared_ptr<Buffer> buffer,
      std::weak_ptr<Network> network);

  ~NetworkData();

  const encrypt::SelfEncryptor& encryptor() const { return encryptor_; }
  encrypt::SelfEncryptor& encryptor() { return encryptor_; }
  const std::shared_ptr<Buffer>& buffer() const { return buffer_; }

  template<typename Token>
  static AsyncResultReturn<Token, encrypt::DataMap> Store(
      NetworkData&& network_data,
      std::weak_ptr<Network> network,
      Token token) {
    assert(network_data.buffer_ != nullptr);
    using Handler = AsyncHandler<Token, encrypt::DataMap>;

    auto new_chunks = network_data.PrepareNewChunks();

    Handler handler(std::move(token));
    asio::async_result<Handler> result(handler);

    if (new_chunks.empty()) {
      handler(std::move(network_data.map_));
    } else {
      Coroutine<StoreRoutine<Handler>, typename StoreRoutine<Handler>::Frame> coro{
          StoreRoutine<Handler>{},
          std::move(network),
          std::move(new_chunks),
          std::move(network_data.buffer_),
          std::move(network_data.map_),
          std::move(handler)
      };

      coro.Execute();
    }

    return result.get();
  }

 private:
  // SelfEncryptor is non-movable and non-copyable
  NetworkData(const NetworkData&) = delete;
  NetworkData(NetworkData&& other) = delete;

  NetworkData& operator=(const NetworkData&) = delete;
  NetworkData& operator=(NetworkData&& other) = delete;

  std::vector<encrypt::ByteVector> PrepareNewChunks();

  template<typename Handler>
  struct StoreRoutine {
    struct Frame {
      // constructor needed; once_flag is not moveable or copyable
      Frame(
          std::weak_ptr<Network> network_in,
          std::vector<encrypt::ByteVector> new_chunks_in,
          std::shared_ptr<Buffer> buffer_in,
          encrypt::DataMap map_in,
          Handler handler_in)
        : network(std::move(network_in)),
          new_chunks(std::move(new_chunks_in)),
          buffer(std::move(buffer_in)),
          map(std::move(map_in)),
          count(),
          once(),
          handler(std::move(handler_in)) {
      }
      std::weak_ptr<Network> network;
      std::vector<encrypt::ByteVector> new_chunks;
      std::shared_ptr<Buffer> buffer;
      encrypt::DataMap map;
      std::atomic<std::size_t> count;
      std::once_flag once;
      Handler handler;

     private:
      Frame(const Frame&) = delete;
      Frame(Frame&&) = delete;

      Frame& operator=(const Frame&) = delete;
      Frame& operator=(Frame&&) = delete;
    };

    void operator()(Coroutine<StoreRoutine<Handler>, Frame>& coro) const {
      ASIO_CORO_REENTER(coro) {
        assert(coro.frame().buffer != nullptr);

        static_assert(
            std::is_same<std::size_t, decltype(coro.frame().new_chunks.size())>::value,
            "bad size type");
        coro.frame().count = coro.frame().new_chunks.size();
        assert(coro.frame().count != 0);

        // queue all put requests, then yield
        ASIO_CORO_YIELD {
          const auto network = coro.frame().network.lock();
          auto fail_handler = action::CallOnce(std::ref(coro.frame().once), action::Abort(coro));

          for (auto& chunk_name : coro.frame().new_chunks) {
            /* next line _usually_ retrieves chunk from buffer
               (local disk/memory). In rare situations if a large file was
               processed, the call will be retrieving the data from the
               network!!, but due to the design of self-encryptor, its
               difficult to know what chunks this has been done with. */
            auto chunk = GetChunk(
                *(coro.frame().buffer),
                coro.frame().network,
                std::string(chunk_name.begin(), chunk_name.end()));
            if (!chunk) {
              fail_handler(chunk.error());
              return;
            }

            Network::PutChunk(
                network,
                std::move(*chunk),
                operation.OnSuccess(action::Resume(coro)).OnFailure(fail_handler));
          }
        }  // yield

        if (--(coro.frame().count) == 0) {
          coro.frame().handler(std::move(coro.frame().map));
        }
      }
    }
  };

 private:
  std::shared_ptr<Buffer> buffer_;
  encrypt::DataMap map_;
  encrypt::SelfEncryptor encryptor_;
};

}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_NETWORK_DATA_H_
