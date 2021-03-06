#pragma once
//------------------------------------------------------------------------------
//
//   Copyright 2018 Fetch.AI Limited
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
//------------------------------------------------------------------------------

#include "core/byte_array/encoders.hpp"
#include "core/mutex.hpp"
#include "network/service/protocol.hpp"
#include "storage/document_store.hpp"
#include "storage/revertible_document_store.hpp"

#include <map>

namespace fetch {
namespace storage {

class RevertibleDocumentStoreProtocol : public fetch::service::Protocol
{
public:
  using connection_handle_type = network::AbstractConnection::connection_handle_type;
  using lane_type              = uint32_t;  // TODO(issue 12): Fetch from some other palce

  enum
  {
    GET = 0,
    GET_OR_CREATE,
    LAZY_GET,
    SET,
    COMMIT,
    REVERT,
    HASH,

    LOCK = 20,
    UNLOCK,
    HAS_LOCK
  };

  explicit RevertibleDocumentStoreProtocol(RevertibleDocumentStore *doc_store)
    : fetch::service::Protocol(), doc_store_(doc_store)
  {
    this->Expose(GET, (RevertibleDocumentStore::super_type *)doc_store,
                 &RevertibleDocumentStore::super_type::Get);
    this->Expose(GET_OR_CREATE, (RevertibleDocumentStore::super_type *)doc_store,
                 &RevertibleDocumentStore::super_type::GetOrCreate);
    this->Expose(SET, (RevertibleDocumentStore::super_type *)doc_store,
                 &RevertibleDocumentStore::super_type::Set);
    this->Expose(COMMIT, doc_store, &RevertibleDocumentStore::Commit);
    this->Expose(REVERT, doc_store, &RevertibleDocumentStore::Revert);
    this->Expose(HASH, doc_store, &RevertibleDocumentStore::Hash);

    this->ExposeWithClientArg(LOCK, this, &RevertibleDocumentStoreProtocol::LockResource);
    this->ExposeWithClientArg(UNLOCK, this, &RevertibleDocumentStoreProtocol::UnlockResource);
    this->ExposeWithClientArg(HAS_LOCK, this, &RevertibleDocumentStoreProtocol::HasLock);
  }

  RevertibleDocumentStoreProtocol(RevertibleDocumentStore *doc_store, lane_type const &lane,
                                  lane_type const &maxlanes)
    : fetch::service::Protocol(), doc_store_(doc_store), lane_assignment_(lane)
  {

    SetLaneLog2(maxlanes);
    assert(maxlanes == (1u << log2_lanes_));
    logger.Info("Spinning up lane ", lane_assignment_);

    this->Expose(GET, this, &RevertibleDocumentStoreProtocol::GetLaneChecked);
    this->Expose(GET_OR_CREATE, this, &RevertibleDocumentStoreProtocol::GetOrCreateLaneChecked);
    this->ExposeWithClientArg(SET, this, &RevertibleDocumentStoreProtocol::SetLaneChecked);

    this->Expose(COMMIT, doc_store, &RevertibleDocumentStore::Commit);
    this->Expose(REVERT, doc_store, &RevertibleDocumentStore::Revert);
    this->Expose(HASH, doc_store, &RevertibleDocumentStore::Hash);

    this->ExposeWithClientArg(LOCK, this, &RevertibleDocumentStoreProtocol::LockResource);
    this->ExposeWithClientArg(UNLOCK, this, &RevertibleDocumentStoreProtocol::UnlockResource);
    this->ExposeWithClientArg(HAS_LOCK, this, &RevertibleDocumentStoreProtocol::HasLock);
  }

  bool HasLock(connection_handle_type const &client_id, ResourceID const &rid)
  {
    std::lock_guard<mutex::Mutex> lock(lock_mutex_);
    auto                          it = locks_.find(rid.id());
    if (it == locks_.end()) return false;

    return (it->second == client_id);
  }

  bool LockResource(connection_handle_type const &client_id, ResourceID const &rid)
  {
    std::lock_guard<mutex::Mutex> lock(lock_mutex_);
    auto                          it = locks_.find(rid.id());
    if (it == locks_.end())
    {
      locks_[rid.id()] = client_id;
      return true;
    }

    return (it->second == client_id);
  }

  bool UnlockResource(connection_handle_type const &client_id, ResourceID const &rid)
  {

    std::lock_guard<mutex::Mutex> lock(lock_mutex_);
    auto                          it = locks_.find(rid.id());
    if (it == locks_.end())
    {
      return false;
    }

    if (it->second == client_id)
    {
      locks_.erase(it);
      return true;
    }

    return false;
  }

private:
  Document GetLaneChecked(ResourceID const &rid)
  {
    if (lane_assignment_ != rid.lane(log2_lanes_))
    {
      logger.Warn("Lane assignment is ", lane_assignment_, " vs ", rid.lane(log2_lanes_));
      logger.Debug("Address:", byte_array::ToHex(rid.id()));

      throw serializers::SerializableException(  // TODO(issue 11): set exception number
          0, byte_array_type("Get: Resource located on other lane. TODO, set error number"));
    }

    return doc_store_->Get(rid);
  }

  Document GetOrCreateLaneChecked(ResourceID const &rid)
  {
    if (lane_assignment_ != rid.lane(log2_lanes_))
    {
      logger.Warn("Lane assignment is ", lane_assignment_, " vs ", rid.lane(log2_lanes_));
      logger.Debug("Address:", byte_array::ToHex(rid.id()));

      throw serializers::SerializableException(  // TODO(issue 11): set exception number
          0, byte_array_type("GetOrCreate: Resource located on other lane. "
                             "TODO, set error number"));
    }

    return doc_store_->GetOrCreate(rid);
  }

  void SetLaneChecked(connection_handle_type const &client_id, ResourceID const &rid,
                      byte_array::ConstByteArray const &value)
  {
    if (lane_assignment_ != rid.lane(log2_lanes_))
    {
      throw serializers::SerializableException(  // TODO(issue 11): set exception number
          0, byte_array_type("Set: Resource located on other lane. TODO: Set error number."));
    }
    {
      std::lock_guard<mutex::Mutex> lock(lock_mutex_);
      auto                          it = locks_.find(rid.id());
      if ((it == locks_.end()) || (it->second != client_id))
      {
        throw serializers::SerializableException(  // TODO(issue 11): set exception number
            0, byte_array_type("Client does not have a lock for the resource"));
      }
    }

    doc_store_->Set(rid, value);
  }

  void SetLaneLog2(lane_type const &count)
  {
    log2_lanes_ = uint32_t((sizeof(uint32_t) << 3) - uint32_t(__builtin_clz(uint32_t(count)) + 1));
  }

  RevertibleDocumentStore *doc_store_;

  uint32_t log2_lanes_ = 0;

  uint32_t lane_assignment_ = 0;

  mutex::Mutex                                                 lock_mutex_;
  std::map<byte_array::ConstByteArray, connection_handle_type> locks_;
};

}  // namespace storage
}  // namespace fetch
