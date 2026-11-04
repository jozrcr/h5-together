// Quick resume: sending the host's resume save to a player who lacks it, over the
// virtual LAN (see resume_transfer.cpp; the pure part is resume_transfer_logic.h).
#pragma once

#include <stdint.h>

namespace vlan {
class Transport;
}

namespace mods {

// From the virtual LAN core, with its lock held, at each of its polls (every hooked
// socket call): the requests and answers between this PC and the hosts it knows.
void ResumeTransferPoll(vlan::Transport *transport, uint64_t self, uint32_t now_ms);

// For the LAN screen's lines: 0 nothing, 1 receiving (percent), 2 received, 3 failed.
int ResumeTransferState(unsigned *percent);

}  // namespace mods
