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


#ifndef GOAL_APPL_ECAT_H
#define GOAL_APPL_ECAT_H


#include "goal_includes.h"
#include "goal_ecat.h"


/****************************************************************************/
/* Prototypes */
/****************************************************************************/
GOAL_STATUS_T appl_ecatApplInit(
    GOAL_ECAT_T *pHdlEcat                       /**< GOAL EtherCAT handle */
);

GOAL_STATUS_T appl_ecatCallback(
    GOAL_ECAT_T *pHdlEcat,                      /**< GOAL EtherCAT handle */
    GOAL_ECAT_CB_ID_T id,                       /**< callback id */
    GOAL_ECAT_CB_DATA_T *pCb                    /**< callback parameters */
);

#endif /* GOAL_APPL_ECAT_H */
