/// \file
/// \brief select-friendly flag objects.
///
/// \copyright
///   Copyright 2019 by Google LLC.
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

#ifndef ARPC_FLAG_H_
#define ARPC_FLAG_H_

#include <mutex>
#include "arpc/awaitable.h"
#include "arpc/channel.h"

namespace arpc {

class flag {
 public:
  flag();

  void set();
  void reset();
  bool is_set() const;
  explicit operator bool() const;
  awaitable<void> wait_set();

 private:
  mutable std::mutex mu_;
  bool set_ = false;
  channel pipe_[2];
};

}  // namespace arpc

#endif  // ARPC_FLAG_H_