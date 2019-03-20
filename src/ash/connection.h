/// \file
/// \brief Connections are bidirectional streams.
///
/// \copyright
///   Copyright 2018 by Google Inc. All Rights Reserved.
///
/// \copyright
///   Licensed under the Apache License, Version 2.0 (the "License"); you may
///   not use this file except in compliance with the License. You may obtain a
///   copy of the License at
///
/// \copyright
///   http://www.apache.org/licenses/LICENSE-2.0
///
/// \copyright
///   Unless required by applicable law or agreed to in writing, software
///   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
///   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
///   License for the specific language governing permissions and limitations
///   under the License.

#ifndef ASH_CONNECTION_H_
#define ASH_CONNECTION_H_

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include "ash/channel.h"
#include "ash/errors.h"
#include "ash/flag.h"
#include "ash/io_adapters.h"
#include "ash/mpt.h"
#include "ash/socket.h"
#include "ash/usage_lock.h"

namespace ash {

class connection : public input_stream, public output_stream {
 public:
  /// Destructor.
  virtual ~connection();

  /// (Re)connect to the connection's target.
  virtual void connect();

  /// Disconnect if the connection was active.
  virtual void disconnect() = 0;

  /// Check whether the connection is active.
  virtual bool connected() = 0;
};

template <typename Connection>
class reconnectable_connection : public connection {
 public:
  template <typename... Args>
  explicit reconnectable_connection(Args&&... args)
      : connection_factory_(
            [args_tuple = std::make_tuple(std::forward<Args>(args)...),
             this]() {
              return std::apply(
                  [this](const auto&... args) {
                    connection_storage_.emplace(args...);
                  },
                  args_tuple);
            }) {}

  ~reconnectable_connection() override { disconnect(); }

  void connect() override {
    std::scoped_lock lock(mu_);
    auto ptr = connection_.get_or_null();
    if (!ptr || !ptr->connected()) {
      connection_.drop();
      connection_factory_();
      connection_.arm(&*connection_storage_);
    }
  }

  void disconnect() override {
    std::scoped_lock lock(mu_);
    connection_.drop();
    connection_storage_.reset();
  }

  bool connected() override {
    auto ptr = connection_.get_or_null();
    return ptr && ptr->connected();
  }

  void write(const char* data, std::size_t size) override {
    connection_.get()->write(data, size);
  }

  void putc(char c) override { connection_.get()->putc(c); }

  void flush() override { connection_.get()->flush(); }

  std::size_t read(char* data, std::size_t size) override {
    return connection_.get()->read(data, size);
  }

  char getc() override { return connection_.get()->getc(); }

 private:
  std::optional<Connection> connection_storage_;
  usage_lock<Connection, errors::io_error> connection_{"Connection is closed"};
  std::mutex mu_;
  std::function<void()> connection_factory_;
};

class packet_connection {
 public:
  /// Destructor.
  virtual ~packet_connection();

  /// (Re)connect to the connection's target.
  virtual void connect();

  /// Disconnect if the connection was active.
  virtual void disconnect() = 0;

  /// Check whether the connection is active.
  virtual bool connected() = 0;

  /// Send a packet.
  virtual void send(std::string data) = 0;

  /// Receive a packet.
  virtual std::string receive() = 0;
};

template <typename Connection, typename PacketProtocol>
class packet_connection_impl : public packet_connection {
 public:
  template <typename... Args>
  explicit packet_connection_impl(Args&&... args)
      : connection_(std::forward<Args>(args)...) {}
  ~packet_connection_impl() override { disconnect(); }

  void connect() override { connection_.connect(); }
  void disconnect() override { connection_.disconnect(); }
  bool connected() override { return connection_.connected(); }
  void send(std::string data) override {
    protocol_.send(connection_, std::move(data));
  };
  std::string receive() override { return protocol_.receive(connection_); };

 private:
  Connection connection_;
  PacketProtocol protocol_;
};

class channel_connection : public connection {
 public:
  explicit channel_connection(channel&& ch);
  ~channel_connection() override;
  bool connected() override;
  void disconnect() override;
  void write(const char* data, std::size_t size) override;
  void putc(char c) override;
  void flush() override;
  std::size_t read(char* data, std::size_t size) override;
  char getc() override;

 protected:
  usage_lock<channel_connection, errors::io_error> locked_{
      "Connection is closed"};
  std::mutex mu_;
  channel channel_;
  flag closing_;
};  // namespace ash

class char_dev_connection : public channel_connection {
 public:
  explicit char_dev_connection(const std::string& path);

 protected:
  static channel open_path(const std::string& path);
};

class client_socket_connection : public channel_connection {
 public:
  explicit client_socket_connection(endpoint name);
};

}  // namespace ash

#endif  // ASH_CONNECTION_H_
