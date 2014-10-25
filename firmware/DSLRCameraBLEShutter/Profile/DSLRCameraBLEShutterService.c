/**************************************************************************************************
  Filename:       DSLRCameraBLEShutterService.c
  Author:         Joe Shang <shangchuanren@gmail.com>
  Description:    This file contains the DSLR Camera BLE Shutter GATT profile 
                  for use with BLE shutter.
**************************************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "bcomdef.h"
#include "OSAL.h"
#include "linkdb.h"
#include "att.h"
#include "gatt.h"
#include "gatt_uuid.h"
#include "gattservapp.h"
#include "gapbondmgr.h"

#include "DSLRCameraBLEShutterService.h"

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPEDEFS
 */

// DSLR Camera BLE Shutter Service UUID
CONST uint8 bleShutterServUUID[ATT_BT_UUID_SIZE] = 
{
    LO_UINT16(BLESHUTTER_SERV_UUID), HI_UINT16(BLESHUTTER_SERV_UUID)
};

// Characteristic Focus UUID
CONST uint8 bleShutterFocusUUID[ATT_BT_UUID_SIZE] = 
{
    LO_UINT16(BLESHUTTER_FOCUS_UUID), HI_UINT16(BLESHUTTER_FOCUS_UUID)
};

// Characteristic Shooting UUID
CONST uint8 bleShutterShootingUUID[ATT_BT_UUID_SIZE] = 
{
    LO_UINT16(BLESHUTTER_SHOOTING_UUID), HI_UINT16(BLESHUTTER_SHOOTING_UUID)
};

// Characteristic Stop UUID
CONST uint8 bleShutterStopUUID[ATT_BT_UUID_SIZE] = 
{
    LO_UINT16(BLESHUTTER_STOP_UUID), HI_UINT16(BLESHUTTER_STOP_UUID)
};

// Characteristic Progress UUID
CONST uint8 bleShutterProgressUUID[ATT_BT_UUID_SIZE] = 
{
    LO_UINT16(BLESHUTTER_PROGRESS_UUID), HI_UINT16(BLESHUTTER_PROGRESS_UUID)
};

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

static bleShutterCBs_t *bleShutter_AppCBs = NULL;

/*********************************************************************
 * Profile Attributes - variables
 */

// DSLR Camera BLE Shutter Service attribute
static CONST gattAttrType_t bleShutterService = { ATT_BT_UUID_SIZE, bleShutterServUUID };


// Characteristic Focus Properties
static uint8 bleShutterFocusProps = GATT_PROP_WRITE;
// Characteristic Focus Value
static uint8 bleShutterFocus = 0;
// Characteristic Focus Description
static uint8 bleShutterFocusUserDesp[] = "Focus\0";

// Characteristic Shooting Properties
static uint8 bleShutterShootingProps = GATT_PROP_WRITE;
// Characteristic Shooting Value
static uint8 bleShutterShooting[BLESHUTTER_SHOOTING_LEN] = { 0 };
// Characteristic Shooting Description
static uint8 bleShutterShootingUserDesp[] = "Shooting\0";

// Characteristic Stop Properties
static uint8 bleShutterStopProps = GATT_PROP_WRITE;
// Characteristic Stop Value
static uint8 bleShutterStop = 0;
// Characteristic Stop Description
static uint8 bleShutterStopUserDesp[] = "Stop\0";

// Characteristic Progress Properties
static uint8 bleShutterProgressProps = GATT_PROP_NOTIFY;
// Characteristic Progress Value
static uint8 bleShutterProgress[BLESHUTTER_PROGRESS_LEN] = { 0 };
// Characteristic Progress Configuration Each client has its own
// instantiation of the Client Characteristic Configuration. Reads of the
// Client Characteristic Configuration only shows the configuration for
// that client and writes only affect the configuration of that client.
static gattCharCfg_t bleShutterProgressConfig[GATT_MAX_NUM_CONN];
// Characteristic Progress Description
static uint8 bleShutterProgressUserDesp[] = "Progress\0";

/*********************************************************************
 * Profile Attributes - Table
 */

static gattAttribute_t bleShutterAttrTbl[] =
{
    // BLE Shutter Service
    { 
        { ATT_BT_UUID_SIZE, primaryServiceUUID }, /* type */
        GATT_PERMIT_READ,                         /* permissions */
        0,                                        /* handle */
        (uint8 *)&bleShutterService               /* pValue */
    },

    // Characteristic Focus Declaration
    { 
        { ATT_BT_UUID_SIZE, characterUUID },
        GATT_PERMIT_READ, 
        0,
        &bleShutterFocusProps 
    },

    // Characteristic Focus Value 
    { 
        { ATT_BT_UUID_SIZE, bleShutterFocusUUID },
        GATT_PERMIT_WRITE,
        0, 
        &bleShutterFocus 
    },

    // Characteristic Focus User Description
    { 
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_READ, 
        0, 
        bleShutterFocusUserDesp 
    },      

    // Characteristic Shooting Declaration
    { 
        { ATT_BT_UUID_SIZE, characterUUID },
        GATT_PERMIT_READ, 
        0,
        &bleShutterShootingProps 
    },

    // Characteristic Value Shooting
    { 
        { ATT_BT_UUID_SIZE, bleShutterShootingUUID },
        GATT_PERMIT_WRITE,
        0, 
        bleShutterShooting 
    },

    // Characteristic Shooting User Description
    { 
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_READ, 
        0, 
        bleShutterShootingUserDesp 
    },           

    // Characteristic Stop Declaration
    { 
        { ATT_BT_UUID_SIZE, characterUUID },
        GATT_PERMIT_READ, 
        0,
        &bleShutterStopProps 
    },

    // Characteristic Stop Value 
    { 
        { ATT_BT_UUID_SIZE, bleShutterStopUUID },
        GATT_PERMIT_WRITE, 
        0, 
        &bleShutterStop 
    },

    // Characteristic Stop User Description
    { 
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_READ, 
        0, 
        bleShutterStopUserDesp 
    },

    // Characteristic Progress Declaration
    { 
        { ATT_BT_UUID_SIZE, characterUUID },
        GATT_PERMIT_READ, 
        0,
        &bleShutterProgressProps 
    },

    // Characteristic Progress Value 
    { 
        { ATT_BT_UUID_SIZE, bleShutterProgressUUID },
        0, 
        0, 
        bleShutterProgress 
    },

    // Characteristic Progress configuration
    { 
        { ATT_BT_UUID_SIZE, clientCharCfgUUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE, 
        0, 
        (uint8 *)bleShutterProgressConfig 
    },

    // Characteristic Progress User Description
    { 
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_READ, 
        0, 
        bleShutterProgressUserDesp 
    }

};


/*********************************************************************
 * LOCAL FUNCTIONS
 */
static uint8 bleShutter_ReadAttrCB( uint16 connHandle, gattAttribute_t *pAttr, 
        uint8 *pValue, uint8 *pLen, uint16 offset, uint8 maxLen );
static bStatus_t bleShutter_WriteAttrCB( uint16 connHandle, gattAttribute_t *pAttr,
        uint8 *pValue, uint8 len, uint16 offset );

static void bleShutter_HandleConnStatusCB( uint16 connHandle, uint8 changeType );


/*********************************************************************
 * PROFILE CALLBACKS
 */
// DSLR Camera BLE Shutter Service Callbacks
CONST gattServiceCBs_t bleShutterCBs =
{
    bleShutter_ReadAttrCB,  // Read callback function pointer
    bleShutter_WriteAttrCB, // Write callback function pointer
    NULL                    // Authorization callback function pointer
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      BLEShutter_AddService
 *
 * @brief   Initializes the BLE Shutter service by registering
 *          GATT attributes with the GATT server.
 *
 * @param   services - services to add. This is a bit map and can
 *                     contain more than one service.
 *
 * @return  Success or Failure
 */
bStatus_t BLEShutter_AddService( uint32 services )
{
    uint8 status = SUCCESS;

    // Initialize Client Characteristic Configuration attributes
    GATTServApp_InitCharCfg( INVALID_CONNHANDLE, bleShutterProgressConfig );

    // Register with Link DB to receive link status change callback
    VOID linkDB_Register( bleShutter_HandleConnStatusCB );  

    if ( services & BLESHUTTER_SERVICE )
    {
        // Register GATT attribute list and CBs with GATT Server App
        status = GATTServApp_RegisterService( bleShutterAttrTbl, 
                GATT_NUM_ATTRS( bleShutterAttrTbl ),
                &bleShutterCBs );
    }

    return ( status );
}


/*********************************************************************
 * @fn      BLEShutter_RegisterAppCBs
 *
 * @brief   Registers the application callback function. Only call 
 *          this function once.
 *
 * @param   callbacks - pointer to application callbacks.
 *
 * @return  SUCCESS or bleAlreadyInRequestedMode
 */
bStatus_t BLEShutter_RegisterAppCBs( bleShutterCBs_t *appCallbacks )
{
    if ( appCallbacks )
    {
        bleShutter_AppCBs = appCallbacks;

        return ( SUCCESS );
    }
    else
    {
        return ( bleAlreadyInRequestedMode );
    }
}


/*********************************************************************
 * @fn      BLEShutter_SetParameter
 *
 * @brief   Set a BLE Shutter parameter.
 *
 * @param   param - Profile parameter ID
 * @param   len - length of data to right
 * @param   value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate 
 *          data type (example: data type of uint16 will be cast to 
 *          uint16 pointer).
 *
 * @return  bStatus_t
 */
bStatus_t BLEShutter_SetParameter( uint8 param, uint8 len, void *value )
{
    bStatus_t ret = SUCCESS;
    switch ( param )
    {
        case BLESHUTTER_FOCUS:
            if ( len == sizeof ( uint8 ) ) 
            {
                bleShutterFocus = *((uint8*)value);
            }
            else
            {
                ret = bleInvalidRange;
            }
            break;

        case BLESHUTTER_SHOOTING:
            if ( len == BLESHUTTER_SHOOTING_LEN )
            {
                VOID osal_memcpy( bleShutterShooting, value, BLESHUTTER_SHOOTING_LEN );
            }
            else
            {
                ret = bleInvalidRange;
            }
            break;

        case BLESHUTTER_STOP:
            if ( len == sizeof ( uint8 ) ) 
            {
                bleShutterStop = *((uint8*)value);
            }
            else
            {
                ret = bleInvalidRange;
            }
            break;

        case BLESHUTTER_PROGRESS:
            if ( len == BLESHUTTER_PROGRESS_LEN ) 
            {
                VOID osal_memcpy( bleShutterProgress, value, BLESHUTTER_PROGRESS_LEN);

                // See if Notification has been enabled
                GATTServApp_ProcessCharCfg( bleShutterProgressConfig, bleShutterProgress, FALSE,
                        bleShutterAttrTbl, GATT_NUM_ATTRS( bleShutterAttrTbl ),
                        INVALID_TASK_ID );
            }
            else
            {
                ret = bleInvalidRange;
            }
            break;

        default:
            ret = INVALIDPARAMETER;
            break;
    }

    return ( ret );
}

/*********************************************************************
 * @fn      BLEShutter_GetParameter
 *
 * @brief   Get a BLE Shutter parameter.
 *
 * @param   param - Profile parameter ID
 * @param   value - pointer to data to put.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate 
 *          data type (example: data type of uint16 will be cast to 
 *          uint16 pointer).
 *
 * @return  bStatus_t
 */
bStatus_t BLEShutter_GetParameter( uint8 param, void *value )
{
    bStatus_t ret = SUCCESS;
    switch ( param )
    {
        case BLESHUTTER_FOCUS:
            *((uint8*)value) = bleShutterFocus;
            break;

        case BLESHUTTER_SHOOTING:
            VOID osal_memcpy( value, bleShutterShooting, BLESHUTTER_SHOOTING_LEN );
            break;      

        case BLESHUTTER_STOP:
            *((uint8*)value) = bleShutterStop;
            break;  

        case BLESHUTTER_PROGRESS:
            VOID osal_memcpy( value, bleShutterProgress, BLESHUTTER_PROGRESS_LEN );
            break;      

        default:
            ret = INVALIDPARAMETER;
            break;
    }

    return ( ret );
}

/*********************************************************************
 * @fn          bleShutter_ReadAttrCB
 *
 * @brief       Read an attribute.
 *
 * @param       connHandle - connection message was received on
 * @param       pAttr - pointer to attribute
 * @param       pValue - pointer to data to be read
 * @param       pLen - length of data to be read
 * @param       offset - offset of the first octet to be read
 * @param       maxLen - maximum length of data to be read
 *
 * @return      Success or Failure
 */
static uint8 bleShutter_ReadAttrCB( uint16 connHandle, gattAttribute_t *pAttr, 
        uint8 *pValue, uint8 *pLen, uint16 offset, uint8 maxLen )
{
    bStatus_t status = SUCCESS;

    // If attribute permissions require authorization to read, return error
    if ( gattPermitAuthorRead( pAttr->permissions ) )
    {
        // Insufficient authorization
        return ( ATT_ERR_INSUFFICIENT_AUTHOR );
    }

    // Make sure it's not a blob operation (no attributes in the profile are long)
    if ( offset > 0 )
    {
        return ( ATT_ERR_ATTR_NOT_LONG );
    }

    if ( pAttr->type.len == ATT_BT_UUID_SIZE )
    {
        // 16-bit UUID
        uint16 uuid = BUILD_UINT16( pAttr->type.uuid[0], pAttr->type.uuid[1]);
        switch ( uuid )
        {
            // No need for "GATT_SERVICE_UUID" or "GATT_CLIENT_CHAR_CFG_UUID" cases;
            // gattserverapp handles those reads
            case BLESHUTTER_PROGRESS_UUID:
                *pLen = BLESHUTTER_PROGRESS_LEN;
                VOID osal_memcpy( pValue, pAttr->pValue, BLESHUTTER_PROGRESS_LEN );
                break;

            default:
                *pLen = 0;
                status = ATT_ERR_ATTR_NOT_FOUND;
                break;
        }
    }
    else
    {
        // 128-bit UUID
        *pLen = 0;
        status = ATT_ERR_INVALID_HANDLE;
    }

    return ( status );
}

/*********************************************************************
 * @fn      bleShutter_WriteAttrCB
 *
 * @brief   Validate attribute data prior to a write operation
 *
 * @param   connHandle - connection message was received on
 * @param   pAttr - pointer to attribute
 * @param   pValue - pointer to data to be written
 * @param   len - length of data
 * @param   offset - offset of the first octet to be written
 *
 * @return  Success or Failure
 */
static bStatus_t bleShutter_WriteAttrCB( uint16 connHandle, gattAttribute_t *pAttr,
        uint8 *pValue, uint8 len, uint16 offset )
{
    bStatus_t status = SUCCESS;
    uint8 notifyApp = 0xFF;

    // If attribute permissions require authorization to write, return error
    if ( gattPermitAuthorWrite( pAttr->permissions ) )
    {
        // Insufficient authorization
        return ( ATT_ERR_INSUFFICIENT_AUTHOR );
    }

    if ( pAttr->type.len == ATT_BT_UUID_SIZE )
    {
        // 16-bit UUID
        uint16 uuid = BUILD_UINT16( pAttr->type.uuid[0], pAttr->type.uuid[1]);
        switch ( uuid )
        {
            case BLESHUTTER_FOCUS_UUID:
            case BLESHUTTER_SHOOTING_UUID:
            case BLESHUTTER_STOP_UUID:
            case BLESHUTTER_PROGRESS_UUID:
                VOID osal_memcpy( pAttr->pValue, pValue, len );
                notifyApp = uuid - BLESHUTTER_SERV_UUID;

                break;

            case GATT_CLIENT_CHAR_CFG_UUID:
                status = GATTServApp_ProcessCCCWriteReq( connHandle, pAttr, pValue, len,
                        offset, GATT_CLIENT_CFG_NOTIFY );
                break;

            default:
                status = ATT_ERR_ATTR_NOT_FOUND;
                break;
        }
    }
    else
    {
        // 128-bit UUID
        status = ATT_ERR_INVALID_HANDLE;
    }

    // If a charactersitic value changed then callback function to notify application of change
    if ( (notifyApp != 0xFF ) && bleShutter_AppCBs && bleShutter_AppCBs->pfnBLEShutterChange )
    {
        bleShutter_AppCBs->pfnBLEShutterChange( notifyApp );  
    }

    return ( status );
}

/*********************************************************************
 * @fn          bleShutter_HandleConnStatusCB
 *
 * @brief       BLE Shutter link status change handler function.
 *
 * @param       connHandle - connection handle
 * @param       changeType - type of change
 *
 * @return      none
 */
static void bleShutter_HandleConnStatusCB( uint16 connHandle, uint8 changeType )
{ 
    // Make sure this is not loopback connection
    if ( connHandle != LOOPBACK_CONNHANDLE )
    {
        // Reset Client Char Config if connection has dropped
        if ( ( changeType == LINKDB_STATUS_UPDATE_REMOVED )      ||
                ( ( changeType == LINKDB_STATUS_UPDATE_STATEFLAGS ) && 
                  ( !linkDB_Up( connHandle ) ) ) )
        { 
            GATTServApp_InitCharCfg( connHandle, bleShutterProgressConfig );
        }
    }
}


/*********************************************************************
 *********************************************************************/
