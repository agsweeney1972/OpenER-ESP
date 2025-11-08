/*******************************************************************************
 * Copyright (c) 2012, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "opener_api.h"
#include "appcontype.h"
#include "trace.h"
#include "cipidentity.h"
#include "ciptcpipinterface.h"
#include "cipqos.h"
#include "driver/gpio.h"

#define DEMO_APP_INPUT_ASSEMBLY_NUM                100
#define DEMO_APP_OUTPUT_ASSEMBLY_NUM               150
#define DEMO_APP_CONFIG_ASSEMBLY_NUM               151
EipUint8 g_assembly_data064[32];
EipUint8 g_assembly_data096[32];
EipUint8 g_assembly_data097[10];

static const gpio_num_t kStatusLedGpio = GPIO_NUM_33;

static void ConfigureStatusLed(void) {
  gpio_config_t led_config = {
    .pin_bit_mask = 1ULL << kStatusLedGpio,
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&led_config);
  gpio_set_level(kStatusLedGpio, 0);
}

EipStatus ApplicationInitialization(void) {
  CreateAssemblyObject( DEMO_APP_OUTPUT_ASSEMBLY_NUM, g_assembly_data096,
                       sizeof(g_assembly_data096));

  CreateAssemblyObject( DEMO_APP_INPUT_ASSEMBLY_NUM, g_assembly_data064,
                       sizeof(g_assembly_data064));

  CreateAssemblyObject( DEMO_APP_CONFIG_ASSEMBLY_NUM, g_assembly_data097,
                       sizeof(g_assembly_data097));

  ConfigureExclusiveOwnerConnectionPoint(0, DEMO_APP_OUTPUT_ASSEMBLY_NUM,
  DEMO_APP_INPUT_ASSEMBLY_NUM,
                                         DEMO_APP_CONFIG_ASSEMBLY_NUM);
  CipRunIdleHeaderSetO2T(false);
  CipRunIdleHeaderSetT2O(false);
  ConfigureStatusLed();

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
  {
    CipClass *p_eth_link_class = GetCipClass(kCipEthernetLinkClassCode);
    InsertGetSetCallback(p_eth_link_class,
                         EthLnkPreGetCallback,
                         kPreGetFunc);
    InsertGetSetCallback(p_eth_link_class,
                         EthLnkPostGetCallback,
                         kPostGetFunc);
    for (int idx = 0; idx < OPENER_ETHLINK_INSTANCE_CNT; ++idx)
    {
      CipAttributeStruct *p_eth_link_attr;
      CipInstance *p_eth_link_inst =
        GetCipInstance(p_eth_link_class, idx + 1);
      OPENER_ASSERT(p_eth_link_inst);

      p_eth_link_attr = GetCipAttribute(p_eth_link_inst, 4);
      p_eth_link_attr->attribute_flags |= (kPreGetFunc | kPostGetFunc);
      p_eth_link_attr = GetCipAttribute(p_eth_link_inst, 5);
      p_eth_link_attr->attribute_flags |= (kPreGetFunc | kPostGetFunc);
    }
  }
#endif

  return kEipStatusOk;
}

void HandleApplication(void) {
}

void CheckIoConnectionEvent(unsigned int output_assembly_id,
                            unsigned int input_assembly_id,
                            IoConnectionEvent io_connection_event) {

  (void) output_assembly_id;
  (void) input_assembly_id;
  (void) io_connection_event;
}

EipStatus AfterAssemblyDataReceived(CipInstance *instance) {
  EipStatus status = kEipStatusOk;

  switch (instance->instance_number) {
    case DEMO_APP_OUTPUT_ASSEMBLY_NUM:
      memcpy(g_assembly_data064, g_assembly_data096,
             sizeof(g_assembly_data064));
      gpio_set_level(kStatusLedGpio,
                     (g_assembly_data096[0] & 0x01) ? 1 : 0);
      break;
    case DEMO_APP_CONFIG_ASSEMBLY_NUM:
      status = kEipStatusOk;
      break;
    default:
      OPENER_TRACE_INFO(
          "Unknown assembly instance ind AfterAssemblyDataReceived");
      break;
  }
  return status;
}

EipBool8 BeforeAssemblyDataSend(CipInstance *instance) {
  (void) instance;
  return true;
}

EipStatus ResetDevice(void) {
  CloseAllConnections();
  CipQosUpdateUsedSetQosValues();
  return kEipStatusOk;
}

EipStatus ResetDeviceToInitialConfiguration(void) {
  g_tcpip.encapsulation_inactivity_timeout = 120;
  CipQosResetAttributesToDefaultValues();
  ResetDevice();
  return kEipStatusOk;
}

void*
CipCalloc(size_t number_of_elements,
          size_t size_of_element) {
  return calloc(number_of_elements, size_of_element);
}

void CipFree(void *data) {
  free(data);
}

void RunIdleChanged(EipUint32 run_idle_value) {
  OPENER_TRACE_INFO("Run/Idle handler triggered\n");
  if ((0x0001 & run_idle_value) == 1) {
    CipIdentitySetExtendedDeviceStatus(kAtLeastOneIoConnectionInRunMode);
  } else {
    CipIdentitySetExtendedDeviceStatus(
        kAtLeastOneIoConnectionEstablishedAllInIdleMode);
  }
  (void) run_idle_value;
}

