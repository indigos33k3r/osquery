/**
 *  Copyright (c) 2014-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under both the Apache 2.0 license (found in the
 *  LICENSE file in the root directory of this source tree) and the GPLv2 (found
 *  in the COPYING file in the root directory of this source tree).
 *  You may select, at your option, one of the above-listed licenses.
 */

#pragma once

#include <osquery/utils/expected/expected.h>
#include <osquery/utils/system/linux/ebpf/program.h>

#include <boost/optional.hpp>

#include <bitset>
#include <type_traits>
#include <unordered_map>

namespace osquery {
namespace events {
namespace syscall {

enum class Type : __s32 {
  Unknown = 0,
  KillEnter = 1,
  KillExit = -KillEnter,
  SetuidEnter = 2,
  SetuidExit = -SetuidEnter,
};

static constexpr std::size_t kCommSize = 16u;

constexpr Type flipType(Type const type) noexcept {
  return static_cast<Type>(
      -static_cast<std::underlying_type<Type>::type>(type));
}

constexpr bool isTypeExit(Type const type) noexcept {
  return static_cast<std::underlying_type<Type>::type>(type) < 0;
}

constexpr bool isTypeEnter(Type const type) noexcept {
  return 0 < static_cast<std::underlying_type<Type>::type>(type);
}

struct Event {
  // Common part for all events whether Enter or Exit
  Type type;
  __s32 pid;
  __s32 tgid;

  // Body means different things for each Enter type.
  // For all Exit types Body is always the same - just return value.
  union Body {
    struct KillEnter {
      /* -44 type */
      /* -40 pid */
      /* -36 tgid */
      /* -32 */ char comm[kCommSize];
      /* -16 */ __s32 arg_pid;
      /* -12 */ __s32 arg_sig;
      /*  -8 */ __u32 uid;
      /*  -4 */ __u32 gid;
    } kill_enter;

    struct SetuidEnter {
      /* -40 type */
      /* -36 pid */
      /* -32 tgid */
      /* -28 */ char comm[kCommSize];
      /* -12 */ __s32 arg_uid;
      /*  -8 */ __u32 uid;
      /*  -4 */ __u32 gid;
    } setuid_enter;

    struct Exit {
      /* -16 type */
      /* -12 pid */
      /* -8 tgid */
      /* -4 */ __s32 ret;
    } exit;
  } body;

  // This value is used by EnterExitJoiner, final return value of the syscall
  // is placed here as a result of join().
  // Also this member is used by EnterExitJoiner to preserve the age of the
  // event.
  __s32 return_value;
};

class EnterExitJoiner {
 public:
  boost::optional<Event> join(Event event);

  bool isEmpty() const;

  using CounterType = int;
  static constexpr std::size_t KeyBitSize = 32u * 3u;
  using KeyType = std::bitset<KeyBitSize>;

 private:
  void drop_stuck_events();

 private:
  CounterType counter_ = 0;
  std::unordered_multimap<KeyType, Event> table_;
};

} // namespace syscall
} // namespace events
} // namespace osquery
