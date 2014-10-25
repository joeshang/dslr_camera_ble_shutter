/**************************************************************************************************
  Filename:       DSLRCameraBLEShutterService.h
  Author:         Joe Shang <shangchuanren@gmail.com>
  Description:    This file contains the DSLR Camera BLE Shutter GATT profile 
                  definitions and prototypes.
**************************************************************************************************/

#ifndef DSLRCAMERABLESHUTTERSERVICE_H
#define DSLRCAMERABLESHUTTERSERVICE_H

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

#define BLESHUTTER_FOCUS                    1
#define BLESHUTTER_SHOOTING                 2
#define BLESHUTTER_STOP                     3
#define BLESHUTTER_PROGRESS                 4

// DSLR Camera BLE Shutter Service UUID
#define BLESHUTTER_SERV_UUID                0xFFF0

// DSLR Camera BLE Shutter Characteristics UUID
#define BLESHUTTER_FOCUS_UUID               BLESHUTTER_SERV_UUID + BLESHUTTER_FOCUS 
#define BLESHUTTER_SHOOTING_UUID            BLESHUTTER_SERV_UUID + BLESHUTTER_SHOOTING
#define BLESHUTTER_STOP_UUID                BLESHUTTER_SERV_UUID + BLESHUTTER_STOP
#define BLESHUTTER_PROGRESS_UUID            BLESHUTTER_SERV_UUID + BLESHUTTER_PROGRESS
  
// Simple Keys Profile Services bit fields
#define BLESHUTTER_SERVICE                  0x00000001

#define BLESHUTTER_SHOOTING_LEN             14
#define BLESHUTTER_PROGRESS_LEN             2

/*********************************************************************
 * TYPEDEFS
 */

  
/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * Profile Callbacks
 */

// Callback when a characteristic value has changed
typedef void (*bleShutterChange_t)( uint8 paramID );

typedef struct
{
  bleShutterChange_t        pfnBLEShutterChange;  // Called when characteristic value changes
} bleShutterCBs_t;

/*********************************************************************
 * API FUNCTIONS 
 */


/*
 * BLEShutter_AddService - Initializes the DSLR Camera BLE Shutter service by
 *          resgistering GATT attributes with the GATT server.
 * @param   services - services to add. This is a bit map and can
 *                     contain more than one service.
 */

extern bStatus_t BLEShutter_AddService( uint32 services );

/*
 * BLEShutter_RegisterAppCB - Registers the application callback function.
 *                    Only call this function once.
 *
 *    appCallbacks - pointer to application callbacks.
 */
extern bStatus_t BLEShutter_RegisterAppCBs( bleShutterCBs_t *appCallback );

/*
 * BLEShutter_SetParameter - Set a DSLR Camera BLE Shutter Service parameter.
 *
 *    param - Profile parameter ID
 *    len - length of data to right
 *    value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate 
 *          data type (example: data type of uint16 will be cast to 
 *          uint16 pointer).
 */
extern bStatus_t BLEShutter_SetParameter( uint8 param, uint8 len, void *value );
  
/*
 * BLEShutter_GetParameter - Get a DSLR Camera BLE Shutter Service parameter.
 *
 *    param - Profile parameter ID
 *    value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate 
 *          data type (example: data type of uint16 will be cast to 
 *          uint16 pointer).
 */
extern bStatus_t BLEShutter_GetParameter( uint8 param, void *value );


/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* DSLRCAMERABLESHUTTERSERVICE_H */
