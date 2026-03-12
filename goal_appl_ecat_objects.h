/** @file
 *
 * @brief EtherCAT Sample application
 *
 * This module contains functions to create the objects (device model) of
 * the EtherCAT slave
 *
 * @copyright
 * Copyright 2026 NXP
 *
 * NXP Confidential and Proprietary. This software is owned or controlled by NXP
 * and may only be used strictly in accordance with the applicable license
 * terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you
 * have read, and that you agree to comply with and are bound by, such license
 * terms. If you do not agree to be bound by the applicable license terms, then
 * you may not retain, install, activate or otherwise use the software.
 *
 */


#ifndef GOAL_APPL_ECAT_OBJECTS_H
#define GOAL_APPL_ECAT_OBJECTS_H


#include <goal_includes.h>
#include <goal_ecat.h>


/****************************************************************************/
/* Global variables declaration */
/****************************************************************************/
extern uint8_t di1_in;                          /**< 0x6000:0x01 Input 1 */
extern uint8_t di2_in;                          /**< 0x6000:0x02 Input 2 */
extern uint8_t di1_out;                         /**< 0x7000:0x01 Output 1 */
extern uint8_t di2_out;                         /**< 0x7000:0x02 Output 2 */


/****************************************************************************/
/* Prototypes */
/****************************************************************************/
GOAL_STATUS_T appl_ecatCreateObjects(
    GOAL_ECAT_T *pHdlEcat                       /**< GOAL EtherCAT handle */
);

#endif /* GOAL_APPL_ECAT_OBJECTS_H */
