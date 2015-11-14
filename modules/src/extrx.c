/**
 *    ||          ____  _ __
 * +------+      / __ )(_) /_______________ _____  ___
 * | 0xBC |     / __  / / __/ ___/ ___/ __ `/_  / / _ \
 * +------+    / /_/ / / /_/ /__/ /  / /_/ / / /_/  __/
 *  ||  ||    /_____/_/\__/\___/_/   \__,_/ /___/\___/
 *
 * Crazyflie control firmware
 *
 * Copyright (C) 2011-2013 Bitcraze AB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, in version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * extrx.c - Module to handle external receiver inputs
 */

/* FreeRtos includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "stm32fxxx.h"
#include "config.h"
#include "system.h"
#include "sppm.h"
#include "nvicconf.h"
#include "commander.h"
#include "uart1.h"
#include "sppm.h"

#define DEBUG_MODULE  "EXTRX"
#include "debug.h"
#include "log.h"

#define EXTRX_NR_CHANNELS             4

#define ENABLE_SPPM
#define ENABLE_EXTRX_LOG


#define EXTRX_NR_CHANNELS  6

#define EXTRX_CH_TRUST     2
#define EXTRX_CH_ROLL      0
#define EXTRX_CH_PITCH     1
#define EXTRX_CH_YAW       3

#define EXTRX_SIGN_ROLL    (-1)
#define EXTRX_SIGN_PITCH   (-1)
#define EXTRX_SIGN_YAW     (-1)

#define EXTRX_SCALE_ROLL   (40.0f)
#define EXTRX_SCALE_PITCH  (40.0f)
#define EXTRX_SCALE_YAW    (400.0f)

static struct CommanderCrtpValues commanderPacket;
static uint16_t ch[EXTRX_NR_CHANNELS];

static void extRxTask(void *param);
static void extRxDecodeSppm(void);
static void extRxDecodeChannels(void);

void extRxInit(void)
{

#ifdef ENABLE_SPPM
  sppmInit();
#endif

#ifdef ENABLE_SPEKTRUM
  uart1Init();
#endif


  xTaskCreate(extRxTask, (const signed char * const) EXTRX_TASK_NAME,
              EXTRX_TASK_STACKSIZE, NULL, EXTRX_TASK_PRI, NULL);
}

static void extRxTask(void *param)
{

  //Wait for the system to be fully started
  systemWaitStart();



  while (true)
  {
    extRxDecodeSppm();
  }
}

static void extRxDecodeChannels(void)
{
  commanderPacket.thrust = sppmConvert2uint16(ch[EXTRX_CH_TRUST]);
  commanderPacket.roll = EXTRX_SIGN_ROLL * sppmConvert2Float(ch[EXTRX_CH_ROLL], -EXTRX_SCALE_ROLL, EXTRX_SCALE_ROLL);
  commanderPacket.pitch = EXTRX_SIGN_PITCH * sppmConvert2Float(ch[EXTRX_CH_PITCH], -EXTRX_SCALE_PITCH, EXTRX_SCALE_PITCH);
  commanderPacket.yaw = EXTRX_SIGN_YAW * sppmConvert2Float(ch[EXTRX_CH_YAW], -EXTRX_SCALE_YAW, EXTRX_SCALE_YAW);
  commanderExtrxSet(&commanderPacket);
}

static void extRxDecodeSppm(void)
{
  uint16_t ppm;
  uint8_t currChannel = 0;

  if (sppmGetTimestamp(&ppm) == pdTRUE)
  {
    if (sppmIsAvailible() &&  ppm < 2100)
    {
      if (currChannel < EXTRX_NR_CHANNELS)
      {
        ch[currChannel] = ppm;
      }
      currChannel++;
    }
    else
    {
      extRxDecodeChannels();
      currChannel = 0;
    }
  }
}

#if 0
static void extRxDecodeSpektrum(void)
{
  while (SerialAvailable(SPEK_SERIAL_PORT) > SPEK_FRAME_SIZE)
  { // More than a frame?  More bytes implies we weren't called for multiple frame times.  We do not want to process 'old' frames in the buffer.
    for (uint8_t i = 0; i < SPEK_FRAME_SIZE; i++)
    {
      SerialRead(SPEK_SERIAL_PORT);
    }  //Toss one full frame of bytes.
  }
  if (spekFrameFlags == 0x01)
  { //The interrupt handler saw at least one valid frame start since we were last here.
    if (SerialAvailable(SPEK_SERIAL_PORT) == SPEK_FRAME_SIZE)
    {  //A complete frame? If not, we'll catch it next time we are called.
      SerialRead(SPEK_SERIAL_PORT);
      SerialRead(SPEK_SERIAL_PORT);        //Eat the header bytes
      for (uint8_t b = 2; b < SPEK_FRAME_SIZE; b += 2)
      {
        uint8_t bh = SerialRead(SPEK_SERIAL_PORT);
        uint8_t bl = SerialRead(SPEK_SERIAL_PORT);
        uint8_t spekChannel = 0x0F & (bh >> SPEK_CHAN_SHIFT);
      if (spekChannel < RC_CHANS) rcValue[spekChannel] = 988 + ((((uint16_t)(bh & SPEK_CHAN_MASK) << 8) + bl) SPEK_DATA_SHIFT);
    }
    spekFrameFlags = 0x00;
    spekFrameData = 0x01;
#if defined(FAILSAFE)
    if(failsafeCnt > 20) failsafeCnt -= 20;
    else failsafeCnt = 0;   // Valid frame, clear FailSafe counter
#endif
  }
  else
  { //Start flag is on, but not enough bytes means there is an incomplete frame in buffer.  This could be OK, if we happened to be called in the middle of a frame.  Or not, if it has been a while since the start flag was set.
    uint32_t spekInterval = (timer0_overflow_count << 8)
        * (64 / clockCyclesPerMicrosecond()) - spekTimeLast;
    if (spekInterval > 2500)
    {
      spekFrameFlags = 0;
    }  //If it has been a while, make the interrupt handler start over.
  }
}
#endif

/* Loggable variables */
#ifdef ENABLE_EXTRX_LOG
LOG_GROUP_START(extrx)
LOG_ADD(LOG_UINT16, ch0, &ch[0])
LOG_ADD(LOG_UINT16, ch1, &ch[1])
LOG_ADD(LOG_UINT16, ch2, &ch[2])
LOG_ADD(LOG_UINT16, ch3, &ch[3])
LOG_ADD(LOG_UINT16, thrust, &commanderPacket.thrust)
LOG_ADD(LOG_FLOAT, roll, &commanderPacket.roll)
LOG_ADD(LOG_FLOAT, pitch, &commanderPacket.pitch)
LOG_ADD(LOG_FLOAT, yaw, &commanderPacket.yaw)
LOG_GROUP_STOP(extrx)
#endif


