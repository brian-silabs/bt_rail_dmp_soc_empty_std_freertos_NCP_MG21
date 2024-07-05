/***************************************************************************//**
 * @file
 * @brief Core proprietary application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "rail.h"
#include "em_common.h"
#include "app_assert.h"
#include "app_proprietary.h"
#include "sl_component_catalog.h"
#ifdef SL_CATALOG_FLEX_IEEE802154_SUPPORT_PRESENT
#include "sl_flex_util_802154_protocol_types.h"
#include "sl_flex_util_802154_init_config.h"
#include "sl_flex_rail_ieee802154_config.h"
#include "sl_flex_ieee802154_support.h"
#include "sl_flex_util_802154_init.h"
#elif defined SL_CATALOG_FLEX_BLE_SUPPORT_PRESENT
#include "sl_flex_util_ble_protocol_config.h"
#include "sl_flex_util_ble_init_config.h"
#include "sl_flex_util_ble_init.h"
#else
#endif

#include "magic_packet.h"
#include "ncp_magic_packet_cmd.h"

// -----------------------------------------------------------------------------
// Constant definitions and macros
#ifdef SL_CATALOG_FLEX_BLE_SUPPORT_PRESENT
/// BLE channel number
#define BLE_CHANNEL ((uint8_t) 0)
#endif

#define TX_OPTIONS                        RAIL_TX_OPTIONS_DEFAULT
#define CCA_ENABLE                        0

static volatile bool app_rail_busy = true;

/// Proprietary Task Parameters
#define APP_PROPRIETARY_TASK_NAME         "app_proprietary"
#define APP_PROPRIETARY_TASK_STACK_SIZE   200
#define APP_PROPRIETARY_TASK_PRIO         5u

StackType_t app_proprietary_task_stack[APP_PROPRIETARY_TASK_STACK_SIZE] = { 0 };
StaticTask_t app_proprietary_task_buffer;
TaskHandle_t app_proprietary_task_handle;

/// OS event group to prevent cyclic execution of the task main loop.
EventGroupHandle_t app_proprietary_event_group_handle;
StaticEventGroup_t app_proprietary_event_group_buffer;

static uint8_t rxData[SL_FLEX_RAIL_FRAME_MAX_SIZE];
static uint8_t txData[SL_FLEX_RAIL_FRAME_MAX_SIZE];
#if CCA_ENABLE
RAIL_CsmaConfig_t csmaConfig = RAIL_CSMA_CONFIG_802_15_4_2003_2p4_GHz_OQPSK_CSMA;
#endif

static uint8_t *eventData;

/**************************************************************************//**
 * Proprietary application task.
 *
 * @param[in] p_arg Unused parameter required by the OS API.
 *****************************************************************************/
static void app_proprietary_task(void *p_arg);

/******************************************************************************
 * RAIL callback, called after the RAIL's initialization finished.
 *****************************************************************************/
void sl_rail_util_on_rf_ready(RAIL_Handle_t rail_handle)
{
  (void) rail_handle;
  app_rail_busy = false;
}

/**************************************************************************//**
 * Proprietary application init.
 *****************************************************************************/
void app_proprietary_init()
{
  while (app_rail_busy) {
  }

  // Create the Proprietary Application task.
  app_proprietary_task_handle = xTaskCreateStatic(app_proprietary_task,
                                                  APP_PROPRIETARY_TASK_NAME,
                                                  APP_PROPRIETARY_TASK_STACK_SIZE,
                                                  NULL,
                                                  APP_PROPRIETARY_TASK_PRIO,
                                                  app_proprietary_task_stack,
                                                  &app_proprietary_task_buffer);

  app_assert(NULL != app_proprietary_task_handle,
             "Task creation failed" APP_LOG_NEW_LINE);

  // Initialize the flag group for the proprietary task.
  app_proprietary_event_group_handle =
    xEventGroupCreateStatic(&app_proprietary_event_group_buffer);

  app_assert(NULL != app_proprietary_event_group_handle,
             "Event group creation failed" APP_LOG_NEW_LINE);
}

static void app_proprietary_task(void *p_arg)
{
  (void)p_arg;
  EventBits_t event_bits;

  /////////////////////////////////////////////////////////////////////////////
  //                                                                         //
  // The following code snippet shows how to start simple receiving, as an   //
  // example, within a DMP application. However, it is commented out to      //
  // demonstrate the lowest possible power consumption as well.              //
  //                                                                         //
  // PLEASE NOTE: ENABLING CONSTANT RECIEVING HAS HEAVY IMPACT ON POWER      //
  // CONSUMPTION OF YOUR PRODUCT.                                            //
  //                                                                         //
  // For further examples on sending / recieving in a DMP application, and   //
  // also on reducing the overall power demand, the following Flex projects  //
  // could serve as a good starting point:                                   //
  //                                                                         //
  //  - Flex (Connect) - Soc Empty Example DMP                               //
  //  - Flex (RAIL) - Range Test DMP                                         //
  //  - Flex (RAIL) - Energy Mode                                            //
  //                                                                         //
  // See also: AN1134: Dynamic Multiprotocol Development with Bluetooth and  //
  //                   Proprietary Protocols on RAIL in GSDK v2.x            //
  /////////////////////////////////////////////////////////////////////////////


   RAIL_Handle_t rail_handle;
   RAIL_Status_t status;
   RAIL_SchedulerInfo_t rxSchedulerInfo;
   RAIL_SchedulerInfo_t txSchedulerInfo;

  // Start task main loop.
  while (1) {
    // Wait for the event bit to be set.
    event_bits = xEventGroupWaitBits(app_proprietary_event_group_handle,
                                     (  APP_PROPRIETARY_EVENT_FLAG \
                                      | APP_PROPRIETARY_EVENT_MAGIC_INIT_FLAG \
                                      | APP_PROPRIETARY_EVENT_MAGIC_DEINIT_FLAG \
                                      | APP_PROPRIETARY_EVENT_MAGIC_WAKE_FLAG),
                                     pdTRUE,
                                     pdFALSE,
                                     portMAX_DELAY);

//    app_assert(event_bits & APP_PROPRIETARY_EVENT_FLAG,
//               "Wrong event bit is set!" APP_LOG_NEW_LINE);

    ///////////////////////////////////////////////////////////////////////////
    // Put your additional application code here!                            //
    // This is called when the event flag APP_PROPRIETARY_EVENT_FLAG is set  //
    ///////////////////////////////////////////////////////////////////////////

    if(event_bits & APP_PROPRIETARY_EVENT_FLAG)
    {
      MagicPacketError_t magicStatus = decodeMagicPacket(&rxData[1]);
      app_log("decodeMagicPacket returned : %d \n", magicStatus);

    }
    if (event_bits & APP_PROPRIETARY_EVENT_MAGIC_INIT_FLAG )
    {
      MagicPacketEnablePayload_t *enable = (MagicPacketEnablePayload_t *)eventData;
      app_log("Enabling 15.4 RX packet with :\n");
      app_log("PanID : 0x%x\n", enable->panId);
      app_log("Channel : 0x%x\n", enable->channel);
      app_log("BR : 0x%x\n", enable->borderRouter);
      // Only set priority because transactionTime is meaningless for infinite
      // operations and slipTime has a reasonable default for relative operations.
      rxSchedulerInfo = (RAIL_SchedulerInfo_t){ .priority = 200 };

      rail_handle = sl_flex_util_get_handle();
#ifdef SL_CATALOG_FLEX_IEEE802154_SUPPORT_PRESENT
      // init the selected protocol for IEEE, first
      sl_flex_ieee802154_protocol_init(rail_handle, SL_FLEX_UTIL_INIT_PROTOCOL_INSTANCE_DEFAULT);


      status = RAIL_IEEE802154_SetPanId(rail_handle, enable->panId, 0);
      if (status != RAIL_STATUS_NO_ERROR) {
        app_log_error("RAIL_IEEE802154_SetPanId() status: %d failed", status);
      }

      sl_flex_ieee802154_set_channel(enable->channel);
      // Start reception.
      status = RAIL_StartRx(rail_handle, sl_flex_ieee802154_get_channel(), (const RAIL_SchedulerInfo_t *)&rxSchedulerInfo);//Flex deserves a channel setter
#elif defined SL_CATALOG_FLEX_BLE_SUPPORT_PRESENT
      status = RAIL_StartRx(rail_handle, BLE_CHANNEL, NULL);
#else
#endif
      app_assert(status == RAIL_STATUS_NO_ERROR,
                "[E: 0x%04x] Failed to start RAIL reception" APP_LOG_NEW_LINE,
                (int)status);

    }
    if (event_bits & APP_PROPRIETARY_EVENT_MAGIC_DEINIT_FLAG )
    {
      app_log("Disabling 15.4 RX packet\n");
      rail_handle = sl_flex_util_get_handle();
      RAIL_Idle(rail_handle, RAIL_IDLE_ABORT, true);

    }
    if (event_bits & APP_PROPRIETARY_EVENT_MAGIC_WAKE_FLAG )
    {
      app_log("Send Wake event back to host\n");
      ncp_sendMagicWakeUpPayloadToHost((MagicPacketPayload_t *)eventData);

    }
    if (event_bits & APP_PROPRIETARY_EVENT_MAGIC_WAKE_FLAG )
    {
       app_log("Transmit requested\n");
       txSchedulerInfo.priority = 200; //Keep it lower than BLE at the moment
       txSchedulerInfo.slipTime = 4300; // BLE ATT max size is 512, @1Mbps will be 4096us over the air . We allow our packet to slip after one
       txSchedulerInfo.transactionTime = 1000; // our packet currently is 12 bytes at 250kbps, whic is ~400us.

       rail_handle =  sl_flex_util_get_handle();
       uint32_t bytesWritten = RAIL_WriteTxFifo(rail_handle, txData, (txData[0] + 1), true);
       if(bytesWritten != 0)
       {
#if CCA_ENABLE
         status = RAIL_StartCcaCsmaTx(rail_handle, sl_flex_ieee802154_get_channel(), TX_OPTIONS, &csmaConfig, (const RAIL_SchedulerInfo_t *)&txSchedulerInfo);
#else
         status = RAIL_StartTx(rail_handle, sl_flex_ieee802154_get_channel(), TX_OPTIONS, (const RAIL_SchedulerInfo_t *)&txSchedulerInfo);
#endif //#if CCA_ENABLE
       }

    }
  }
}

MagicPacketError_t magicPacketCallback(MagicPacketCallbackEvent_t event, void *data)
{
  BaseType_t xHigherPriorityTaskWoken, xResult;
  EventBits_t flag = 0;

  switch (event) {
    case MAGIC_PACKET_EVENT_ENABLED:
      flag = APP_PROPRIETARY_EVENT_MAGIC_INIT_FLAG;
      if(NULL != data)
      {
        eventData = (uint8_t*)data;
      }
      break;
    case MAGIC_PACKET_EVENT_DISABLED:
      flag = APP_PROPRIETARY_EVENT_MAGIC_DEINIT_FLAG;
      break;
    case MAGIC_PACKET_EVENT_WAKE_RX:
      flag = APP_PROPRIETARY_EVENT_MAGIC_WAKE_FLAG;
      if(NULL != data)
      {
        eventData = (uint8_t*)data;
      }
      break;
    case MAGIC_PACKET_EVENT_TX:
      flag = APP_PROPRIETARY_EVENT_MAGIC_TX_FLAG;
      if(NULL != data)
      {
        memcpy(txData, (uint8_t*)data, ((uint8_t*)data)[0]); // We memcpy it immediately in tx buffer, still possible for the
        eventData = (uint8_t*)data;//There is a problem here as data may be overwritten before another TX
      }
      break;
    default:
      break;
  }

  if(0 != flag)
  {
    /* xHigherPriorityTaskWoken must be initialised to pdFALSE. */
    xHigherPriorityTaskWoken = pdFALSE;

    /* Set bit 0 and bit 4 in xEventGroup. */
    xResult = xEventGroupSetBitsFromISR(
                                app_proprietary_event_group_handle,   /* The event group being updated. */
                                (const EventBits_t)flag, /* The bits being set. */
                                &xHigherPriorityTaskWoken );

    /* Was the message posted successfully? */
    if( xResult != pdFAIL )
    {
        /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context
        switch should be requested.  The macro used is port specific and will
        be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() - refer to
        the documentation page for the port being used. */
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
  }

  return MAGIC_PACKET_SUCCESS;
}

/**************************************************************************//**
 * This callback is called on registered RAIL events.
 * Overrides dummy weak implementation.
 *****************************************************************************/
void sl_rail_util_on_event(RAIL_Handle_t rail_handle,
                           RAIL_Events_t events)
{
  //(void)rail_handle;
  (void)events;

  RAIL_RxPacketDetails_t rxDetails;
  RAIL_RxPacketHandle_t packetHandle;
  RAIL_RxPacketInfo_t packetInfo;
  RAIL_Status_t rxStatus;
  BaseType_t xHigherPriorityTaskWoken, xResult;

  /////////////////////////////////////////////////////////////////////////////
  // Add event handlers here as your application requires!                   //
  //                                                                         //
  // Flex (RAIL) - Simple TRX Standards might serve as a good example on how //
  // to implement this event handler properly.                               //
  /////////////////////////////////////////////////////////////////////////////
  ///

  if(events & RAIL_EVENT_RX_PACKET_RECEIVED)
  {
    // Parse 802.15.4 packets
    //app_log("15.4 RX packet received\n");
    packetHandle = RAIL_GetRxPacketInfo(rail_handle, RAIL_RX_PACKET_HANDLE_NEWEST, &packetInfo);
    if ( packetHandle != RAIL_RX_PACKET_HANDLE_INVALID ){
      RAIL_CopyRxPacket(rxData, &packetInfo);
      rxStatus = RAIL_GetRxPacketDetails(rail_handle, packetHandle, &rxDetails);
      RAIL_ReleaseRxPacket(rail_handle, packetHandle);

        /* xHigherPriorityTaskWoken must be initialised to pdFALSE. */
        xHigherPriorityTaskWoken = pdFALSE;

        /* Set bit 0 and bit 4 in xEventGroup. */
        xResult = xEventGroupSetBitsFromISR(
                                    app_proprietary_event_group_handle,   /* The event group being updated. */
                                    APP_PROPRIETARY_EVENT_FLAG, /* The bits being set. */
                                    &xHigherPriorityTaskWoken );

        /* Was the message posted successfully? */
        if( xResult != pdFAIL )
        {
            /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context
            switch should be requested.  The macro used is port specific and will
            be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() - refer to
            the documentation page for the port being used. */
            portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
        }

    }
  }

  if (events & (RAIL_EVENT_TX_PACKET_SENT
                | RAIL_EVENT_TX_ABORTED
                | RAIL_EVENT_TX_UNDERFLOW
                | RAIL_EVENT_SCHEDULER_STATUS)) {
    RAIL_YieldRadio(rail_handle);// Unnecessary to yield upon RX, according to RAIL MultiProtocol docs. We do it only here
  }
}
