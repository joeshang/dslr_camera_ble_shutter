/**************************************************************************************************
Filename:       DSLRCameraBLEShutter.c
Author:         Joe Shang <shangchuanren@gmail.com>
Description:    This file contains the DSLR camera's BLE shutter application
for use with the CC2540 Bluetooth Low Energy Protocol Stack.
 **************************************************************************************************/

/*********************************************************************
 * INCLUDES
 */

#include "bcomdef.h"
#include "OSAL.h"
#include "OSAL_PwrMgr.h"

#include "OnBoard.h"
#include "hal_adc.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_lcd.h"

#include "gatt.h"

#include "hci.h"

#include "gapgattserver.h"
#include "gattservapp.h"
#include "devinfoservice.h"
#include "DSLRCameraBLEShutterService.h"

#if defined( CC2540_MINIDK )
#include "simplekeys.h"
#endif

#include "peripheral.h"

#include "gapbondmgr.h"

#include "DSLRCameraBLEShutter.h"

#if defined FEATURE_OAD
#include "oad.h"
#include "oad_target.h"
#endif

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

// What is the advertising interval when device is discoverable (units of 625us, 160=100ms)
#define DEFAULT_ADVERTISING_INTERVAL          160

// Limited discoverable mode advertises for 30.72s, and then stops
// General discoverable mode advertises indefinitely

#if defined ( CC2540_MINIDK )
#define DEFAULT_DISCOVERABLE_MODE             GAP_ADTYPE_FLAGS_LIMITED
#else
#define DEFAULT_DISCOVERABLE_MODE             GAP_ADTYPE_FLAGS_GENERAL
#endif  // defined ( CC2540_MINIDK )

// Minimum connection interval (units of 1.25ms, 80=100ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL     80

// Maximum connection interval (units of 1.25ms, 800=1000ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL     800

// Slave latency to use if automatic parameter update request is enabled
#define DEFAULT_DESIRED_SLAVE_LATENCY         0

// Supervision timeout value (units of 10ms, 1000=10s) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_CONN_TIMEOUT          1000

// Whether to enable automatic parameter update request when a connection is formed
#define DEFAULT_ENABLE_UPDATE_REQUEST         TRUE

// Connection Pause Peripheral time value (in seconds)
#define DEFAULT_CONN_PAUSE_PERIPHERAL         6

// Company Identifier: Texas Instruments Inc. (13)
#define TI_COMPANY_ID                         0x000D

#define INVALID_CONNHANDLE                    0xFFFF

// Length of bd addr as a string
#define B_ADDR_STR_LEN                        15

#define SHUTTER_SBIT                          P0_1
#define SHUTTER_BV                            BV(1)
#define SHUTTER_DIR                           P0DIR
#define SHUTTER_ACTIVE                        0
#define SHUTTER_RELEASE                       1
#define FOCUS_SBIT                            P0_7
#define FOCUS_DIR                             P0DIR
#define FOCUS_BV                              BV(7)
#define FOCUS_ACTIVE                          0
#define FOCUS_RELEASE                         1

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */
static uint8 dslrCameraBLEShutter_TaskID;   // Task ID for internal task/event processing

static gaprole_States_t gapProfileState = GAPROLE_INIT;

// GAP - SCAN RSP data (max size = 31 bytes)
static uint8 scanRspData[] =
{
    // complete name
    0x15,   // length of this data
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    0x44,   // 'D'
    0x53,   // 'S'
    0x4c,   // 'L'
    0x52,   // 'R'
    0x43,   // 'C'
    0x61,   // 'a'
    0x6d,   // 'm'
    0x65,   // 'e'
    0x72,   // 'r'
    0x61,   // 'a'
    0x42,   // 'B'
    0x4c,   // 'L'
    0x45,   // 'E'
    0x53,   // 'S'
    0x68,   // 'h'
    0x75,   // 'u'
    0x74,   // 't'
    0x74,   // 't'
    0x65,   // 'e'
    0x72,   // 'r'

    // connection interval range
    0x05,   // length of this data
    GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
    LO_UINT16( DEFAULT_DESIRED_MIN_CONN_INTERVAL ),   // 100ms
    HI_UINT16( DEFAULT_DESIRED_MIN_CONN_INTERVAL ),
    LO_UINT16( DEFAULT_DESIRED_MAX_CONN_INTERVAL ),   // 1s
    HI_UINT16( DEFAULT_DESIRED_MAX_CONN_INTERVAL ),

    // Tx power level
    0x02,   // length of this data
    GAP_ADTYPE_POWER_LEVEL,
    0       // 0dBm
};

// GAP - Advertisement data (max size = 31 bytes, though this is
// best kept short to conserve power while advertisting)
static uint8 advertData[] =
{
    // Flags; this sets the device to use limited discoverable
    // mode (advertises for 30 seconds at a time) instead of general
    // discoverable mode (advertises indefinitely)
    0x02,   // length of this data
    GAP_ADTYPE_FLAGS,
    DEFAULT_DISCOVERABLE_MODE | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

    // service UUID, to notify central devices what services are included
    // in this peripheral
    0x03,   // length of this data
    GAP_ADTYPE_16BIT_MORE,      // some of the UUID's, but not all
    LO_UINT16( BLESHUTTER_SERV_UUID ),
    HI_UINT16( BLESHUTTER_SERV_UUID ),
};

// GAP GATT Attributes
static uint8 attDeviceName[GAP_DEVICE_NAME_LEN] = "DSLR BLE Shutter";

static uint16 progressCount     = 0;
static uint16 targetCount       = 1;
static uint32 delayBeforeStart  = 0;
static uint32 shutterExposure   = 0;
static uint32 repeatInterval    = 0;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void DSLRCameraBLEShutter_ProcessOSALMsg( osal_event_hdr_t *pMsg );
static void peripheralStateNotificationCB( gaprole_States_t newState );
static void bleShutterChangeCB( uint8 paramID );

static void activeShutter();
static void releaseShutter();
static void activeFocus();
static void releaseFocus();

#if defined( CC2540_MINIDK )
static void DSLRCameraBLEShutter_HandleKeys( uint8 shift, uint8 keys );
#endif

#if (defined HAL_LCD) && (HAL_LCD == TRUE)
static char *bdAddr2Str ( uint8 *pAddr );
#endif // (defined HAL_LCD) && (HAL_LCD == TRUE)



/*********************************************************************
 * PROFILE CALLBACKS
 */

// GAP Role Callbacks
static gapRolesCBs_t DSLRCameraBLEShutter_PeripheralCBs =
{
    peripheralStateNotificationCB,  // Profile State Change Callbacks
    NULL                            // When a valid RSSI is read from controller (not used by application)
};

// GAP Bond Manager Callbacks
static gapBondCBs_t DSLRCameraBLEShutter_BondMgrCBs =
{
    NULL,                     // Passcode callback (not used by application)
    NULL                      // Pairing / Bonding state Callback (not used by application)
};

// BLE Shutter GATT Profile Callbacks
static bleShutterCBs_t DSLRCameraBLEShutter_BLEShutterCBs =
{
    bleShutterChangeCB    // Charactersitic value change callback
};
/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      DSLRCameraBLEShutter_Init
 *
 * @brief   Initialization function for the DSLR Camera BLE Shutter App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notificaiton ... ).
 *
 * @param   task_id - the ID assigned by OSAL.  This ID should be
 *                    used to send messages and set timers.
 *
 * @return  none
 */
void DSLRCameraBLEShutter_Init( uint8 task_id )
{
    dslrCameraBLEShutter_TaskID = task_id;

    // Setup the GAP
    VOID GAP_SetParamValue( TGAP_CONN_PAUSE_PERIPHERAL, DEFAULT_CONN_PAUSE_PERIPHERAL );

    // Setup the GAP Peripheral Role Profile
    {
#if defined( CC2540_MINIDK )
        // For the CC2540DK-MINI keyfob, device doesn't start advertising until button is pressed
        uint8 initial_advertising_enable = FALSE;
#else
        // For other hardware platforms, device starts advertising upon initialization
        uint8 initial_advertising_enable = TRUE;
#endif

        // By setting this to zero, the device will go into the waiting state after
        // being discoverable for 30.72 second, and will not being advertising again
        // until the enabler is set back to TRUE
        uint16 gapRole_AdvertOffTime = 0;

        uint8 enable_update_request = DEFAULT_ENABLE_UPDATE_REQUEST;
        uint16 desired_min_interval = DEFAULT_DESIRED_MIN_CONN_INTERVAL;
        uint16 desired_max_interval = DEFAULT_DESIRED_MAX_CONN_INTERVAL;
        uint16 desired_slave_latency = DEFAULT_DESIRED_SLAVE_LATENCY;
        uint16 desired_conn_timeout = DEFAULT_DESIRED_CONN_TIMEOUT;

        // Set the GAP Role Parameters
        GAPRole_SetParameter( GAPROLE_ADVERT_ENABLED, sizeof( uint8 ), &initial_advertising_enable );
        GAPRole_SetParameter( GAPROLE_ADVERT_OFF_TIME, sizeof( uint16 ), &gapRole_AdvertOffTime );

        GAPRole_SetParameter( GAPROLE_SCAN_RSP_DATA, sizeof ( scanRspData ), scanRspData );
        GAPRole_SetParameter( GAPROLE_ADVERT_DATA, sizeof( advertData ), advertData );

        GAPRole_SetParameter( GAPROLE_PARAM_UPDATE_ENABLE, sizeof( uint8 ), &enable_update_request );
        GAPRole_SetParameter( GAPROLE_MIN_CONN_INTERVAL, sizeof( uint16 ), &desired_min_interval );
        GAPRole_SetParameter( GAPROLE_MAX_CONN_INTERVAL, sizeof( uint16 ), &desired_max_interval );
        GAPRole_SetParameter( GAPROLE_SLAVE_LATENCY, sizeof( uint16 ), &desired_slave_latency );
        GAPRole_SetParameter( GAPROLE_TIMEOUT_MULTIPLIER, sizeof( uint16 ), &desired_conn_timeout );
    }

    // Set the GAP Characteristics
    GGS_SetParameter( GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, attDeviceName );

    // Set advertising interval
    {
        uint16 advInt = DEFAULT_ADVERTISING_INTERVAL;

        GAP_SetParamValue( TGAP_LIM_DISC_ADV_INT_MIN, advInt );
        GAP_SetParamValue( TGAP_LIM_DISC_ADV_INT_MAX, advInt );
        GAP_SetParamValue( TGAP_GEN_DISC_ADV_INT_MIN, advInt );
        GAP_SetParamValue( TGAP_GEN_DISC_ADV_INT_MAX, advInt );
    }

    // Setup the GAP Bond Manager
    {
        uint32 passkey = 0; // passkey "000000"
        uint8 pairMode = GAPBOND_PAIRING_MODE_WAIT_FOR_REQ;
        uint8 mitm = TRUE;
        uint8 ioCap = GAPBOND_IO_CAP_DISPLAY_ONLY;
        uint8 bonding = TRUE;
        GAPBondMgr_SetParameter( GAPBOND_DEFAULT_PASSCODE, sizeof ( uint32 ), &passkey );
        GAPBondMgr_SetParameter( GAPBOND_PAIRING_MODE, sizeof ( uint8 ), &pairMode );
        GAPBondMgr_SetParameter( GAPBOND_MITM_PROTECTION, sizeof ( uint8 ), &mitm );
        GAPBondMgr_SetParameter( GAPBOND_IO_CAPABILITIES, sizeof ( uint8 ), &ioCap );
        GAPBondMgr_SetParameter( GAPBOND_BONDING_ENABLED, sizeof ( uint8 ), &bonding );
    }

    // Initialize GATT attributes
    GGS_AddService( GATT_ALL_SERVICES );            // GAP
    GATTServApp_AddService( GATT_ALL_SERVICES );    // GATT attributes
    DevInfo_AddService();                           // Device Information Service
    BLEShutter_AddService( GATT_ALL_SERVICES );  // DSLR Camera BLE Shutter Service
#if defined FEATURE_OAD
    VOID OADTarget_AddService();                    // OAD Profile
#endif

    // Setup the BLEShutter Characteristic Values
    {
        uint8 focus = 0;
        uint8 stop = 0;
        uint8 shooting[BLESHUTTER_SHOOTING_LEN] = { 1, 0 };

        BLEShutter_SetParameter( BLESHUTTER_FOCUS, sizeof ( uint8 ), &focus);
        BLEShutter_SetParameter( BLESHUTTER_STOP,  sizeof ( uint8 ), &stop);
        BLEShutter_SetParameter( BLESHUTTER_PROGRESS, BLESHUTTER_PROGRESS_LEN, &progressCount);
        BLEShutter_SetParameter( BLESHUTTER_SHOOTING, BLESHUTTER_SHOOTING_LEN, shooting);
    }


#if defined( CC2540_MINIDK )

    SK_AddService( GATT_ALL_SERVICES ); // Simple Keys Profile

    // Register for all key events - This app will handle all key events
    RegisterForKeys( dslrCameraBLEShutter_TaskID );

    // makes sure LEDs are off
    HalLedSet( (HAL_LED_1 | HAL_LED_2), HAL_LED_MODE_OFF );

    // For keyfob board set GPIO pins into a power-optimized state
    // Note that there is still some leakage current from the buzzer,
    // accelerometer, LEDs, and buttons on the PCB.

    P0SEL = 0; // Configure Port 0 as GPIO
    P1SEL = 0; // Configure Port 1 as GPIO
    P2SEL = 0; // Configure Port 2 as GPIO

    P0DIR = 0xFC; // Port 0 pins P0.0 and P0.1 as input (buttons),
    // all others (P0.2-P0.7) as output
    P1DIR = 0xFF; // All port 1 pins (P1.0-P1.7) as output
    P2DIR = 0x1F; // All port 1 pins (P2.0-P2.4) as output

    P0 = 0x03; // All pins on port 0 to low except for P0.0 and P0.1 (buttons)
    P1 = 0;   // All pins on port 1 to low
    P2 = 0;   // All pins on port 2 to low

#endif // #if defined( CC2540_MINIDK )

    // Initialize Shutter and Focus related GPIO
    SHUTTER_DIR |= SHUTTER_BV;
    FOCUS_DIR |= FOCUS_BV;

    SHUTTER_SBIT = SHUTTER_RELEASE;
    FOCUS_SBIT = SHUTTER_RELEASE;

#if (defined HAL_LCD) && (HAL_LCD == TRUE)

#if defined FEATURE_OAD
#if defined (HAL_IMAGE_A)
    HalLcdWriteStringValue( "BLE Peri-A", OAD_VER_NUM( _imgHdr.ver ), 16, HAL_LCD_LINE_1 );
#else
    HalLcdWriteStringValue( "BLE Peri-B", OAD_VER_NUM( _imgHdr.ver ), 16, HAL_LCD_LINE_1 );
#endif // HAL_IMAGE_A
#else
    HalLcdWriteString( "BLE Shutter", HAL_LCD_LINE_1 );
#endif // FEATURE_OAD

#endif // (defined HAL_LCD) && (HAL_LCD == TRUE)

    // Register callback with BLE Shutter Service
    VOID BLEShutter_RegisterAppCBs( &DSLRCameraBLEShutter_BLEShutterCBs );

    // Enable clock divide on halt
    // This reduces active current while radio is active and CC254x MCU
    // is halted
    HCI_EXT_ClkDivOnHaltCmd( HCI_EXT_ENABLE_CLK_DIVIDE_ON_HALT );

#if defined ( DC_DC_P0_7 )

    // Enable stack to toggle bypass control on TPS62730 (DC/DC converter)
    HCI_EXT_MapPmIoPortCmd( HCI_EXT_PM_IO_PORT_P0, HCI_EXT_PM_IO_PORT_PIN7 );

#endif // defined ( DC_DC_P0_7 )

    // Setup a delayed profile startup
    osal_set_event( dslrCameraBLEShutter_TaskID, DCBS_START_DEVICE_EVT );

}

/*********************************************************************
 * @fn      DSLRCameraBLEShutter_ProcessEvent
 *
 * @brief   DSLR Camera BLE Shutter Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  events not processed
 */
uint16 DSLRCameraBLEShutter_ProcessEvent( uint8 task_id, uint16 events )
{

    VOID task_id; // OSAL required parameter that isn't used in this function

    if ( events & SYS_EVENT_MSG )
    {
        uint8 *pMsg;

        if ( (pMsg = osal_msg_receive( dslrCameraBLEShutter_TaskID )) != NULL )
        {
            DSLRCameraBLEShutter_ProcessOSALMsg( (osal_event_hdr_t *)pMsg );

            // Release the OSAL message
            VOID osal_msg_deallocate( pMsg );
        }

        // return unprocessed events
        return (events ^ SYS_EVENT_MSG);
    }

    if ( events & DCBS_START_DEVICE_EVT )
    {
        // Start the Device
        VOID GAPRole_StartDevice( &DSLRCameraBLEShutter_PeripheralCBs );

        // Start Bond Manager
        VOID GAPBondMgr_Register( &DSLRCameraBLEShutter_BondMgrCBs );

        return ( events ^ DCBS_START_DEVICE_EVT );
    }

    if ( events & DCBS_FOCUS_RELEASE_EVT )
    {
        releaseFocus();

        return ( events ^ DCBS_FOCUS_RELEASE_EVT );
    }

    if ( events & DCBS_SHOOTING_ACTIVE_EVT )
    {
        HalLcdWriteString( "Shooting Active Event",  HAL_LCD_LINE_8 );
        activeShutter();

        return ( events ^ DCBS_SHOOTING_ACTIVE_EVT );
    }

    if ( events & DCBS_SHOOTING_RELEASE_EVT )
    {
        HalLcdWriteString( "Shooting Release Event",  HAL_LCD_LINE_8 );
        releaseShutter();

        return ( events ^ DCBS_SHOOTING_RELEASE_EVT );
    }

    // Discard unknown events
    return 0;
}

/*********************************************************************
 * @fn      DSLRCameraBLEShutter_ProcessOSALMsg
 *
 * @brief   Process an incoming task message.
 *
 * @param   pMsg - message to process
 *
 * @return  none
 */
static void DSLRCameraBLEShutter_ProcessOSALMsg( osal_event_hdr_t *pMsg )
{
    switch ( pMsg->event )
    {
#if defined( CC2540_MINIDK )
        case KEY_CHANGE:
            DSLRCameraBLEShutter_HandleKeys( ((keyChange_t *)pMsg)->state, ((keyChange_t *)pMsg)->keys );
            break;
#endif // #if defined( CC2540_MINIDK )

        default:
            // do nothing
            break;
    }
}

#if defined( CC2540_MINIDK )
/*********************************************************************
 * @fn      DSLRCameraBLEShutter_HandleKeys
 *
 * @brief   Handles all key events for this device.
 *
 * @param   shift - true if in shift/alt.
 * @param   keys - bit field for key events. Valid entries:
 *                 HAL_KEY_SW_2
 *                 HAL_KEY_SW_1
 *
 * @return  none
 */
static void DSLRCameraBLEShutter_HandleKeys( uint8 shift, uint8 keys )
{
    uint8 SK_Keys = 0;

    VOID shift;  // Intentionally unreferenced parameter

    if ( keys & HAL_KEY_SW_1 )
    {
        SK_Keys |= SK_KEY_LEFT;
    }

    if ( keys & HAL_KEY_SW_2 )
    {

        SK_Keys |= SK_KEY_RIGHT;

        // if device is not in a connection, pressing the right key should toggle
        // advertising on and off
        // Note:  If PLUS_BROADCASTER is define this condition is ignored and
        //        Device may advertise during connections as well. 
#ifndef PLUS_BROADCASTER  
        if( gapProfileState != GAPROLE_CONNECTED )
        {
#endif // PLUS_BROADCASTER
            uint8 current_adv_enabled_status;
            uint8 new_adv_enabled_status;

            //Find the current GAP advertisement status
            GAPRole_GetParameter( GAPROLE_ADVERT_ENABLED, &current_adv_enabled_status );

            if( current_adv_enabled_status == FALSE )
            {
                new_adv_enabled_status = TRUE;
            }
            else
            {
                new_adv_enabled_status = FALSE;
            }

            //change the GAP advertisement status to opposite of current status
            GAPRole_SetParameter( GAPROLE_ADVERT_ENABLED, sizeof( uint8 ), &new_adv_enabled_status );
#ifndef PLUS_BROADCASTER
        }
#endif // PLUS_BROADCASTER
    }

    // Set the value of the keys state to the Simple Keys Profile;
    // This will send out a notification of the keys state if enabled
    SK_SetParameter( SK_KEY_ATTR, sizeof ( uint8 ), &SK_Keys );
}
#endif // #if defined( CC2540_MINIDK )

/*********************************************************************
 * @fn      peripheralStateNotificationCB
 *
 * @brief   Notification from the profile of a state change.
 *
 * @param   newState - new state
 *
 * @return  none
 */
static void peripheralStateNotificationCB( gaprole_States_t newState )
{
#ifdef PLUS_BROADCASTER
    static uint8 first_conn_flag = 0;
#endif // PLUS_BROADCASTER


    switch ( newState )
    {
        case GAPROLE_STARTED:
            {
                uint8 ownAddress[B_ADDR_LEN];
                uint8 systemId[DEVINFO_SYSTEM_ID_LEN];

                GAPRole_GetParameter(GAPROLE_BD_ADDR, ownAddress);

                // use 6 bytes of device address for 8 bytes of system ID value
                systemId[0] = ownAddress[0];
                systemId[1] = ownAddress[1];
                systemId[2] = ownAddress[2];

                // set middle bytes to zero
                systemId[4] = 0x00;
                systemId[3] = 0x00;

                // shift three bytes up
                systemId[7] = ownAddress[5];
                systemId[6] = ownAddress[4];
                systemId[5] = ownAddress[3];

                DevInfo_SetParameter(DEVINFO_SYSTEM_ID, DEVINFO_SYSTEM_ID_LEN, systemId);

#if (defined HAL_LCD) && (HAL_LCD == TRUE)
                // Display device address
                HalLcdWriteString( bdAddr2Str( ownAddress ),  HAL_LCD_LINE_2 );
                HalLcdWriteString( "Initialized",  HAL_LCD_LINE_3 );
#endif // (defined HAL_LCD) && (HAL_LCD == TRUE)
            }
            break;

        case GAPROLE_ADVERTISING:
            {
#if (defined HAL_LCD) && (HAL_LCD == TRUE)
                HalLcdWriteString( "Advertising",  HAL_LCD_LINE_3 );
#endif // (defined HAL_LCD) && (HAL_LCD == TRUE)
            }
            break;

        case GAPROLE_CONNECTED:
            {        
#if (defined HAL_LCD) && (HAL_LCD == TRUE)
                HalLcdWriteString( "Connected",  HAL_LCD_LINE_3 );
#endif // (defined HAL_LCD) && (HAL_LCD == TRUE)

#ifdef PLUS_BROADCASTER
                // Only turn advertising on for this state when we first connect
                // otherwise, when we go from connected_advertising back to this state
                // we will be turning advertising back on.
                if ( first_conn_flag == 0 ) 
                {
                    uint8 adv_enabled_status = 1;
                    GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8), &adv_enabled_status); // Turn on Advertising
                    first_conn_flag = 1;
                }
#endif // PLUS_BROADCASTER
            }
            break;

        case GAPROLE_CONNECTED_ADV:
            {
#if (defined HAL_LCD) && (HAL_LCD == TRUE)
                HalLcdWriteString( "Connected Advertising",  HAL_LCD_LINE_3 );
#endif // (defined HAL_LCD) && (HAL_LCD == TRUE)
            }
            break;      
        case GAPROLE_WAITING:
            {
#if (defined HAL_LCD) && (HAL_LCD == TRUE)
                HalLcdWriteString( "Disconnected",  HAL_LCD_LINE_3 );
#endif // (defined HAL_LCD) && (HAL_LCD == TRUE)
            }
            break;

        case GAPROLE_WAITING_AFTER_TIMEOUT:
            {
#if (defined HAL_LCD) && (HAL_LCD == TRUE)
                HalLcdWriteString( "Timed Out",  HAL_LCD_LINE_3 );
#endif // (defined HAL_LCD) && (HAL_LCD == TRUE)

#ifdef PLUS_BROADCASTER
                // Reset flag for next connection.
                first_conn_flag = 0;
#endif //#ifdef (PLUS_BROADCASTER)
            }
            break;

        case GAPROLE_ERROR:
            {
#if (defined HAL_LCD) && (HAL_LCD == TRUE)
                HalLcdWriteString( "Error",  HAL_LCD_LINE_3 );
#endif // (defined HAL_LCD) && (HAL_LCD == TRUE)
            }
            break;

        default:
            {
#if (defined HAL_LCD) && (HAL_LCD == TRUE)
                HalLcdWriteString( "",  HAL_LCD_LINE_3 );
#endif // (defined HAL_LCD) && (HAL_LCD == TRUE)
            }
            break;

    }

    gapProfileState = newState;

#if !defined( CC2540_MINIDK )
    VOID gapProfileState;     // added to prevent compiler warning with
    // "CC2540 Slave" configurations
#endif


}

/*********************************************************************
 * @fn      bleShutterChangeCB
 *
 * @brief   Callback from DSLR Camera BLE Shutter indicating a value change
 *
 * @param   paramID - parameter ID of the value that was changed.
 *
 * @return  none
 */
static void bleShutterChangeCB( uint8 paramID )
{
    switch( paramID )
    {
        case BLESHUTTER_FOCUS:
            {
                uint8 focus;
                BLEShutter_GetParameter( BLESHUTTER_FOCUS, &focus );

                activeFocus();

#if (defined HAL_LCD) && (HAL_LCD == TRUE)
                HalLcdWriteStringValue( "Focus:", (uint16)(focus), 10,  HAL_LCD_LINE_3 );
#endif // (defined HAL_LCD) && (HAL_LCD == TRUE)
            }
            break;

        case BLESHUTTER_SHOOTING:
            {
                uint8 shooting[BLESHUTTER_SHOOTING_LEN];
                BLEShutter_GetParameter( BLESHUTTER_SHOOTING, shooting );

                uint8 index = 0;
                VOID osal_memcpy( &targetCount, shooting + index, sizeof(uint16) );
                index += sizeof(uint16);
                VOID osal_memcpy( &delayBeforeStart, shooting + index, sizeof(uint32) );
                index += sizeof(uint32);
                VOID osal_memcpy( &shutterExposure, shooting + index, sizeof(uint32) );
                index += sizeof(uint32);
                VOID osal_memcpy( &repeatInterval, shooting + index, sizeof(uint32) );
                progressCount = 0;

#if (defined HAL_LCD) && (HAL_LCD == TRUE)
                HalLcdWriteString( "Shooting:", HAL_LCD_LINE_3 );
                HalLcdWriteStringValue( "- count:", targetCount, 10,  HAL_LCD_LINE_4 );
                HalLcdWriteStringValueValue( "- delay:", (uint16)(delayBeforeStart >> 16), 10,
                        (uint16)(delayBeforeStart), 10, HAL_LCD_LINE_5 );
                HalLcdWriteStringValueValue( "- exposure:", (uint16)(shutterExposure >> 16), 10,
                        (uint16)(shutterExposure), 10, HAL_LCD_LINE_6 );
                HalLcdWriteStringValueValue( "- interval:", (uint16)(repeatInterval >> 16), 10,
                        (uint16)(repeatInterval), 10, HAL_LCD_LINE_7 );
#endif // (defined HAL_LCD) && (HAL_LCD == TRUE)

                if (delayBeforeStart)
                {
                    osal_start_timerEx( dslrCameraBLEShutter_TaskID, DCBS_SHOOTING_ACTIVE_EVT, delayBeforeStart );
                    HalLcdWriteString( "Start Delay Timer",  HAL_LCD_LINE_8 );
                }
                else
                {
                    activeShutter();
                }

            }
            break;

        case BLESHUTTER_STOP:
            {
                uint8 stop;
                BLEShutter_GetParameter( BLESHUTTER_STOP, &stop );

                osal_stop_timerEx( dslrCameraBLEShutter_TaskID, DCBS_FOCUS_RELEASE_EVT );
                osal_stop_timerEx( dslrCameraBLEShutter_TaskID, DCBS_SHOOTING_ACTIVE_EVT );
                osal_stop_timerEx( dslrCameraBLEShutter_TaskID, DCBS_SHOOTING_RELEASE_EVT );
                SHUTTER_SBIT = SHUTTER_RELEASE;
                FOCUS_SBIT = FOCUS_RELEASE;

#if (defined HAL_LCD) && (HAL_LCD == TRUE)
                HalLcdWriteStringValue( "Stop:", (uint16)(stop), 10,  HAL_LCD_LINE_3 );
#endif // (defined HAL_LCD) && (HAL_LCD == TRUE)

            }
            break;

        default:
            // should not reach here!
            break;
    }
}

static void activeShutter()
{
    SHUTTER_SBIT = SHUTTER_ACTIVE;

    if (shutterExposure != 0xFFFFFFFF)
    {
        if (shutterExposure == 0)
        {
            shutterExposure = DCBS_DEFAULT_ACTIVE_PERIOD;
        }
        HalLcdWriteString( "Start Exposure Timer",  HAL_LCD_LINE_8 );
        osal_start_timerEx( dslrCameraBLEShutter_TaskID, DCBS_SHOOTING_RELEASE_EVT, shutterExposure );
    }
}

static void releaseShutter()
{
    SHUTTER_SBIT = SHUTTER_RELEASE;
    
    progressCount++;
    BLEShutter_SetParameter( BLESHUTTER_PROGRESS, BLESHUTTER_PROGRESS_LEN, &progressCount);
    if (progressCount < targetCount)
    {
        HalLcdWriteString( "Start Interval Timer",  HAL_LCD_LINE_8 );
        osal_start_timerEx( dslrCameraBLEShutter_TaskID, DCBS_SHOOTING_ACTIVE_EVT, repeatInterval );
    }
}

static void activeFocus()
{
    FOCUS_SBIT = FOCUS_ACTIVE;

    osal_start_timerEx( dslrCameraBLEShutter_TaskID, DCBS_FOCUS_RELEASE_EVT, DCBS_DEFAULT_ACTIVE_PERIOD );
}

static void releaseFocus()
{
    FOCUS_SBIT = FOCUS_RELEASE;
}

#if (defined HAL_LCD) && (HAL_LCD == TRUE)
/*********************************************************************
 * @fn      bdAddr2Str
 *
 * @brief   Convert Bluetooth address to string. Only needed when
 *          LCD display is used.
 *
 * @return  none
 */
char *bdAddr2Str( uint8 *pAddr )
{
    uint8       i;
    char        hex[] = "0123456789ABCDEF";
    static char str[B_ADDR_STR_LEN];
    char        *pStr = str;

    *pStr++ = '0';
    *pStr++ = 'x';

    // Start from end of addr
    pAddr += B_ADDR_LEN;

    for ( i = B_ADDR_LEN; i > 0; i-- )
    {
        *pStr++ = hex[*--pAddr >> 4];
        *pStr++ = hex[*pAddr & 0x0F];
    }

    *pStr = 0;

    return str;
}
#endif // (defined HAL_LCD) && (HAL_LCD == TRUE)

/*********************************************************************
 *********************************************************************/
