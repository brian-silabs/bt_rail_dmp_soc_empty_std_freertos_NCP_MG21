/***************************************************************************//**
 * @file
 * @brief BGAPI user command handler.
 * Demonstrates the communication between an NCP host and NCP target using
 * BGAPI user messages, responses and events. Can be used as a starting point
 * for creating custom commands or for testing purposes.
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#include "sl_ncp.h"
#include "ncp_magic_packet_cmd.h"
#include "magic_packet.h"

PACKSTRUCT(struct magic_packet_wake_event_s {
  uint8_t frameCounter;
  uint8_t status;
  uint8_t timeToLive;
});

typedef struct magic_packet_wake_event_s magic_packet_wake_event_t;

PACKSTRUCT(struct magic_packet_enable_event_s {
  uint8_t panId;
  uint8_t channel;
  uint8_t borderRouter;
});

typedef struct magic_packet_enable_event_s magic_packet_enable_event_t;

PACKSTRUCT(struct magic_packet_cmd {
  uint8_t hdr;
  // Example: union of user commands.
  union {
    magic_packet_wake_event_t magic_packet_wake_event;
    magic_packet_enable_event_t magic_packet_enable_event;
  } data;
});

typedef struct magic_packet_cmd magic_packet_cmd_t;

static MagicPacketEnablePayload_t enablePayload_g;

/***************************************************************************//**
 * User command (message_to_target) handler callback.
 *
 * Handles user defined commands received from NCP host.
 * The user commands handled here are solely meant for example purposes.
 * If not needed, this function can be removed entirely or replaced with a
 * custom one.
 * @param[in] data Data received from NCP host.
 *
 * @note This overrides the dummy weak implementation.
 ******************************************************************************/
void sl_ncp_user_cmd_message_to_target_cb(void *data)
{
  uint8array *cmd = (uint8array *)data;
  magic_packet_cmd_t *magic_packet_cmd = (magic_packet_cmd_t *)cmd->data;
  sl_status_t sc = SL_STATUS_OK;

  switch (magic_packet_cmd->hdr) {
    case MAGIC_PACKET_CMD_ENABLE_ID:
      //////////////////////////////////////////////
      // Add your user command handler code here! //
      //////////////////////////////////////////////

      // Example: Respond, then send events with the given interval until the
      // USER_CMD_PERIODIC_ASYNC_STOP_ID command is received.
      enablePayload_g.panId = magic_packet_cmd->data.magic_packet_enable_event.panId;
      enablePayload_g.channel = magic_packet_cmd->data.magic_packet_enable_event.channel;
      enablePayload_g.borderRouter = magic_packet_cmd->data.magic_packet_enable_event.borderRouter;

      enableMagicPacketFilter(&enablePayload_g);

      // Send response to user command to NCP host.
      sl_ncp_user_cmd_message_to_target_rsp(sc, 1, &magic_packet_cmd->hdr);
      break;

    case MAGIC_PACKET_CMD_DISABLE_ID:
      //////////////////////////////////////////////
      // Add your user command handler code here! //
      //////////////////////////////////////////////
      disableMagicPacketFilter();
      // Send response to user command to NCP host.
      sl_ncp_user_cmd_message_to_target_rsp(sc, 1, &magic_packet_cmd->hdr);
      break;

    /////////////////////////////////////////////////
    // Add further user command handler code here! //
    /////////////////////////////////////////////////

    // Unknown user command.
    default:
      // Send error response to NCP host.
      sl_ncp_user_cmd_message_to_target_rsp(SL_STATUS_FAIL, 0, NULL);
      break;
  }
}
