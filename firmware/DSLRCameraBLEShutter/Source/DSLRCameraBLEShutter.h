/**************************************************************************************************
  Filename:       DSLRCameraBLEShutter.h
  Author:         Joe Shang <shangchuanren@gmail.com>
  Description:    This file contains the DSLR camera's BLE shutter application
                  definitions and prototypes.
**************************************************************************************************/

#ifndef DSLRCAMERABLESHUTTER_H
#define DSLRCAMERABLESHUTTER_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */

/*********************************************************************
 * CONSTANTS
 */


// DSLR Camera BLE Shutter Task Events
#define DCBS_START_DEVICE_EVT                               0x0001
#define DCBS_FOCUS_RELEASE_EVT                              0x0002
#define DCBS_SHOOTING_ACTIVE_EVT                            0x0004
#define DCBS_SHOOTING_RELEASE_EVT                           0x0008

#define DCBS_DEFAULT_ACTIVE_PERIOD                          500


/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * FUNCTIONS
 */

/*
 * Task Initialization for the BLE Application
 */
extern void DSLRCameraBLEShutter_Init( uint8 task_id );

/*
 * Task Event Processor for the BLE Application
 */
extern uint16 DSLRCameraBLEShutter_ProcessEvent( uint8 task_id, uint16 events );

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* DSLRCAMERABLESHUTTER_H */
