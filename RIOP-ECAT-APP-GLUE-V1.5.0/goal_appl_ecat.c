/** @file
 *
 * @brief EtherCAT Sample application
 *
 * This application demonstrates the usage of the application callback function.
 *
 * @copyright
 * Copyright 2010-2026.
 * This software is protected Intellectual Property and may only be used
 * according to the license agreement.
 */


#include "goal_includes.h"
#include "goal_ecat.h"
#include "goal_appl_ecat.h"
#include "goal_appl_ecat_objects.h"


/****************************************************************************/
/* Local defines */
/****************************************************************************/
#define APPL_ECAT_MIN_CYCLE_TIME 200000         /**< minimum cycle time in ns */
#define APPL_ECAT_EXPL_DEV_ID 5                 /**< demo value for Explicit Device ID */


/****************************************************************************/
/* Local prototypes */
/****************************************************************************/
static GOAL_STATUS_T appl_ecatSdoDownload(
    GOAL_ECAT_T *pHdlEcat,                      /**< GOAL EtherCAT handle */
    uint16_t index,                             /**< index of object */
    uint8_t subIndex,                           /**< sub-index of object */
    uint8_t *pData,                             /**< pointer to new data */
    uint32_t size                               /**< data size */
);

static GOAL_STATUS_T appl_ecatSdoUpload(
    GOAL_ECAT_T *pHdlEcat,                      /**< GOAL EtherCAT handle */
    uint16_t index,                             /**< index of object */
    uint8_t subIndex,                           /**< sub-index of object */
    uint8_t **ppData,                           /**< [out] pointer reference for object value */
    uint32_t *pSize                             /**< [out] object's value size */
);

static void appl_ecatPdoReceived(
    GOAL_ECAT_T *pHdlEcat                       /**< GOAL EtherCAT handle */
);

static void appl_ecatPdoTxPrepare(
    GOAL_ECAT_T *pHdlEcat                       /**< GOAL EtherCAT handle */
);

static GOAL_STATUS_T appl_ecatDcSettingsCheck(
    GOAL_ECAT_T *pHdlEcat,                      /**< GOAL EtherCAT handle */
    uint32_t dcCycleTime,                       /**< current DC cycle time in ns */
    uint32_t *pMinCycleTime                     /**< minimum cylce time supported by application in ns */
);

static void appl_ecatExplDevIdGet(
    GOAL_ECAT_T *pHdlEcat,                      /**< GOAL EtherCAT handle */
    uint16_t *pDevId                            /**< [out] explicit device ID */
);

static void appl_ecatEsmTransitionDone(
    GOAL_ECAT_T *pHdlEcat,                      /**< GOAL EtherCAT handle */
    uint16_t newState,                          /**< new ESM state entered */
    GOAL_BOOL_T errFlag,                        /**< error flag */
    uint16_t statusCode                         /**< AL status code */
);

static GOAL_STATUS_T appl_ecatEsmTransitionRequest(
    GOAL_ECAT_T *pHdlEcat,                      /**< GOAL EtherCAT handle */
    uint16_t esmState,                          /**< requested ESM state */
    uint16_t statusCode,                        /**< current AL status code */
    uint16_t *pApplStatusCode                   /**< application specific status code */
);


/****************************************************************************/
/** Application initialization
 *
 * This function is used to initialize application specific resources.
 *
 * @retval GOAL_OK successful
 * @retval other failed
 */
GOAL_STATUS_T appl_ecatApplInit(
    GOAL_ECAT_T *pHdlEcat                       /**< GOAL EtherCAT handle */
)
{
    UNUSEDARG(pHdlEcat);

    return GOAL_OK;
}


/****************************************************************************/
/** EtherCAT Callback Handler
 *
 * This function collects all callbacks from the stack and decides if the
 * callback must be handled.
 *
 * @retval GOAL_OK successful
 * @retval other failed
 */
GOAL_STATUS_T appl_ecatCallback(
    GOAL_ECAT_T *pHdlEcat,                      /**< GOAL EtherCAT handle */
    GOAL_ECAT_CB_ID_T id,                       /**< callback id */
    GOAL_ECAT_CB_DATA_T *pCb                    /**< callback parameters */
)
{
    GOAL_STATUS_T res = GOAL_OK;                /* result */

    switch (id) {
        case GOAL_ECAT_CB_ID_SDO_DOWNLOAD:
            res = appl_ecatSdoDownload(pHdlEcat, pCb->data[0].index, pCb->data[1].subIndex, pCb->data[2].pData, pCb->data[3].size);
            break;

        case GOAL_ECAT_CB_ID_SDO_UPLOAD:
            res = appl_ecatSdoUpload(pHdlEcat, pCb->data[0].index, pCb->data[1].subIndex, &pCb->data[2].pData, &pCb->data[3].size);
            break;

        case GOAL_ECAT_CB_ID_RxPDO_RECEIVED:
            appl_ecatPdoReceived(pHdlEcat);
            break;

        case GOAL_ECAT_CB_ID_TxPDO_PREPARE:
            appl_ecatPdoTxPrepare(pHdlEcat);
            break;

        case GOAL_ECAT_CB_ID_SYNC_FAIL:
            goal_logErr("EtherCAT slave lost synchronization");
            break;

        case GOAL_ECAT_CB_ID_NEW_DL_STATE:
            goal_logInfo("EtherCAT Data Link layer changed: 0x%04x", pCb->data[0].dlState);
            break;

        case GOAL_ECAT_CB_ID_NEW_DC_CONFIG:
            res = appl_ecatDcSettingsCheck(pHdlEcat, pCb->data[0].dcCycleTime, &pCb->data[1].dcCycleTimeMin);
            break;

        case GOAL_ECAT_CB_ID_DC_FAIL:
            goal_logErr("too many DC events were missed");
            break;

        case GOAL_ECAT_CB_ID_SM_WATCHDOG_EXPIRED:
            goal_logErr("Process Data Watchdog expired");
            break;

        case GOAL_ECAT_CB_ID_EXPLICIT_DEV_ID:
            appl_ecatExplDevIdGet(pHdlEcat, &pCb->data[0].explDevId);
            break;

        case GOAL_ECAT_CB_ID_NEW_ESM_STATE_ENTERED:
            appl_ecatEsmTransitionDone(pHdlEcat, pCb->data[0].esmState, pCb->data[1].esmError, pCb->data[2].statusCode);
            break;

        case GOAL_ECAT_CB_ID_NEW_ESM_STATE_REQUESTED:
            appl_ecatEsmTransitionRequest(pHdlEcat, pCb->data[0].esmState, pCb->data[1].statusCode, &pCb->data[2].appStatusCode);
            break;

        case GOAL_ECAT_CB_ID_FOE_READ_REQ:
        case GOAL_ECAT_CB_ID_FOE_READ_DATA:
        case GOAL_ECAT_CB_ID_FOE_WRITE_REQ:
        case GOAL_ECAT_CB_ID_FOE_WRITE_DATA:
        case GOAL_ECAT_CB_ID_FOE_ERROR:
        case GOAL_ECAT_CB_ID_RUN_LED_STATE:
        case GOAL_ECAT_CB_ID_ERROR_LED_STATE:
            break;

        default:
            goal_logErr("unknown callback Id: %d", (int) id);
            res = GOAL_ERROR;
            break;
    }

    return res;
}


/****************************************************************************/
/** Handle a SDO Download Request
 *
 * This function is called if the EtherCAT master wants to write a new value to
 * an object. The new value can be read from @em pData.
 * The application can reject the write access by returning an error code.
 *
 * @retval GOAL_OK success, allow write access
 * @retval GOAL_ERR_SDO_HARDWARE hardware error
 * @retval GOAL_ERR_SDO_TOO_LOW value to low
 * @retval GOAL_ERR_SDO_TOO_HIGH value to high
 * @retval GOAL_ERR_SDO_APPL application specific reason
 * @retval GOAL_ERR_SDO_DEV_STATE current device state insufficient
 * @retval GOAL_ERR_SDO_GENERAL unspecified SDO error
 */
static GOAL_STATUS_T appl_ecatSdoDownload(
    GOAL_ECAT_T *pHdlEcat,                      /**< GOAL EtherCAT handle */
    uint16_t index,                             /**< index of object */
    uint8_t subIndex,                           /**< sub-index of object */
    uint8_t *pData,                             /**< pointer to new data */
    uint32_t size                               /**< data size */
)
{
    UNUSEDARG(pHdlEcat);
    UNUSEDARG(index);
    UNUSEDARG(subIndex);
    UNUSEDARG(pData);
    UNUSEDARG(size);

    goal_logInfo("index: 0x%04x, sub: %d", index, subIndex);

    return GOAL_OK;
}


/****************************************************************************/
/** Handle a SDO Upload Request
 *
 * This function is called if the EtherCAT master wants to read a value from
 * an object. The application can update a value before it is sent out. The
 * pointer reference must be set to the new value and its size must be written
 * to @em pSize.
 * The application can reject the read access by returning an error code.
 *
 * @retval GOAL_OK success, allow write access
 * @retval GOAL_ERR_SDO_HARDWARE hardware error
 * @retval GOAL_ERR_SDO_APPL application specific reason
 * @retval GOAL_ERR_SDO_DEV_STATE current device state insufficient
 * @retval GOAL_ERR_SDO_GENERAL unspecified SDO error
 */
static GOAL_STATUS_T appl_ecatSdoUpload(
    GOAL_ECAT_T *pHdlEcat,                      /**< GOAL EtherCAT handle */
    uint16_t index,                             /**< index of object */
    uint8_t subIndex,                           /**< sub-index of object */
    uint8_t **ppData,                           /**< [out] pointer reference for object value */
    uint32_t *pSize                             /**< [out] object's value size */
)
{
    UNUSEDARG(pHdlEcat);
    UNUSEDARG(index);
    UNUSEDARG(subIndex);
    UNUSEDARG(ppData);
    UNUSEDARG(pSize);

    goal_logInfo("index: 0x%04x, sub: %d", index, subIndex);

    return GOAL_OK;
}


/****************************************************************************/
/** New Process Data has been received (Outputs)
 *
 * All objects that are mapped to RxPDOs have been updated.
 */
static void appl_ecatPdoReceived(
    GOAL_ECAT_T *pHdlEcat                       /**< GOAL EtherCAT handle */
)
{
    UNUSEDARG(pHdlEcat);

    /* nothing todo */

}


/****************************************************************************/
/** Prepare Process Data that will be sent (Inputs)
 *
 * This function is called before the Inputs are sent to the master. The
 * application has the chance to update the objects that will be mapped to the
 * TxPDOs.
 */
static void appl_ecatPdoTxPrepare(
    GOAL_ECAT_T *pHdlEcat                       /**< GOAL EtherCAT handle */
)
{
    UNUSEDARG(pHdlEcat);

    /* nothing to do */

}


/****************************************************************************/
/** New DC configuration received
 *
 * This function is called if the master has written a new DC configuration.
 * The application must provide its smallest supported cycle time.
 * If the current cycle time exceeds the limit the settings can be rejected.
 *
 * @retval GOAL_OK accept settings
 * @retval GOAL_ERROR reject settings
 */
static GOAL_STATUS_T appl_ecatDcSettingsCheck(
    GOAL_ECAT_T *pHdlEcat,                      /**< GOAL EtherCAT handle */
    uint32_t dcCycleTime,                       /**< current DC cycle time in ns */
    uint32_t *pMinCycleTime                     /**< [out] minimum cycle time supported by application in ns */
)
{
    UNUSEDARG(pHdlEcat);

    *pMinCycleTime = APPL_ECAT_MIN_CYCLE_TIME;

    return (APPL_ECAT_MIN_CYCLE_TIME <= dcCycleTime) ? GOAL_OK : GOAL_ERROR;
}


/****************************************************************************/
/** Get Explicit Device ID
 *
 * This function must provide the Explicit Device ID. It is used by the master
 * to explicitly identify a slave. The device ID must be configurable by
 * non-EtherCAT means (e.g. hardware switches, terminal, display, etc.).
 * The Device ID must not equal 0.
 */
static void appl_ecatExplDevIdGet(
    GOAL_ECAT_T *pHdlEcat,                      /**< GOAL EtherCAT handle */
    uint16_t *pDevId                            /**< [out] explicit device ID */
)
{
    UNUSEDARG(pHdlEcat);

    /* for this demo a fixed value is used, this value is expected by the CTT */
    *pDevId = APPL_ECAT_EXPL_DEV_ID;
}


/****************************************************************************/
/** New ESM state entered
 *
 * This function is called if the EtherCAT State Machine has entered a new
 * state. If an error occurred @em errFlag is set and @em statusCode indicates
 * the reason.
 */
static void appl_ecatEsmTransitionDone(
    GOAL_ECAT_T *pHdlEcat,                      /**< GOAL EtherCAT handle */
    uint16_t newState,                          /**< new ESM state entered */
    GOAL_BOOL_T errFlag,                        /**< error flag */
    uint16_t statusCode                         /**< AL status code */
)
{
    UNUSEDARG(pHdlEcat);
    UNUSEDARG(newState);
    UNUSEDARG(errFlag);
    UNUSEDARG(statusCode);

    if (GOAL_TRUE == errFlag) {
        goal_logErr("error in ESM state: %s, AL status code: 0x%04x",
                     (GOAL_ECAT_ESM_STATE_INIT == newState) ? "INIT"
                     : (GOAL_ECAT_ESM_STATE_PREOP == newState) ? "PreOp"
                     : (GOAL_ECAT_ESM_STATE_BOOTSTRAP == newState) ? "BOOTSTRAP"
                     : (GOAL_ECAT_ESM_STATE_SAFEOP == newState) ? "SafeOp"
                     : (GOAL_ECAT_ESM_STATE_OP == newState) ? "OP"
                     : "unknown", statusCode);
    }
    else {
        goal_logInfo("new ESM state: %s",
                     (GOAL_ECAT_ESM_STATE_INIT == newState) ? "INIT"
                     : (GOAL_ECAT_ESM_STATE_PREOP == newState) ? "PreOp"
                     : (GOAL_ECAT_ESM_STATE_BOOTSTRAP == newState) ? "BOOTSTRAP"
                     : (GOAL_ECAT_ESM_STATE_SAFEOP == newState) ? "SafeOp"
                     : (GOAL_ECAT_ESM_STATE_OP == newState) ? "OP"
                     : "unknown");
    }
}


/****************************************************************************/
/** New ESM state transition requested
 *
 * This function is called if the master requested a transition of the EtherCAT
 * State Machine. The new state is in @em esmState If an error occurred the
 * status code is shown in @em statusCode.
 *
 * The application can reject the transition via return value. In that case @em
 * pApplStatusCode must contain the appropriate AL status code. One of the
 * GOAL_ECAT_STATE_CODE_ macros shall be used.
 *
 * @retval GOAL_OK accept transition
 * @retval GOAL_ERROR reject transition
 */
static GOAL_STATUS_T appl_ecatEsmTransitionRequest(
    GOAL_ECAT_T *pHdlEcat,                      /**< GOAL EtherCAT handle */
    uint16_t esmState,                          /**< requested ESM state */
    uint16_t statusCode,                        /**< current AL status code */
    uint16_t *pApplStatusCode                   /**< application specific status code */
)
{
    UNUSEDARG(pHdlEcat);
    UNUSEDARG(esmState);
    UNUSEDARG(statusCode);
    UNUSEDARG(pApplStatusCode);

    goal_logInfo("requested state: %s",
                 (GOAL_ECAT_ESM_STATE_INIT == esmState) ? "INIT"
                 : (GOAL_ECAT_ESM_STATE_PREOP == esmState) ? "PreOp"
                 : (GOAL_ECAT_ESM_STATE_BOOTSTRAP == esmState) ? "BOOTSTRAP"
                 : (GOAL_ECAT_ESM_STATE_SAFEOP == esmState) ? "SafeOp"
                 : (GOAL_ECAT_ESM_STATE_OP == esmState) ? "OP"
                 : "unknown");

    return GOAL_OK;
}
