/*
 * Copyright 2025 NXP
 *
 * NXP Confidential and Proprietary. This software is owned or controlled by NXP
 * and may only be used strictly in accordance with the applicable license
 * terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you
 * have read, and that you agree to comply with and are bound by, such license
 * terms. If you do not agree to be bound by the applicable license terms, then
 * you may not retain, install, activate or otherwise use the software.
 */
/*
 * ecat_conf.h - EtherCAT Library Configuration
 *
 * Copyright (C) 1998-2024  by port GmbH  Halle(Saale)/Germany
 *
 *------------------------------------------------------------
 * CC library reference version — do NOT replace with ICC-generated ecat_conf.h.
 * ICC generates wrong SM addresses and omits required defines for the RPC build.
 * Source: rd-riop-ecat-1.5.0/goal/projects/goal_ecat_rpc_lib/00_cc/edt/nxp/evkmimxrt1180/ecat_conf.h
 */

/**
 * \file ecat_conf.h
 * \author port GmbH
 *
 * This module contains the configuration
 * of the EtherCAT Library for the EtherCAT device.
 */



#ifndef ECAT_CONF_H
#define ECAT_CONF_H	1
#define EC_CONFIG_ICCTOOL_VERSION 0x00070100

/* active configuration : 0 */
#define EC_CONFIG_USE_TARGET_0 1


/*
 * General Settings
 */

#define EC_CONFIG_DYNAMIC_OD                     1
#define EC_CONFIG_NO_LOCAL_OD                    1
#define EC_CONFIG_BOOTSTRAP                 	1
#define EC_CONFIG_BOOTSTRAP_MBOXIN_ADR      	0x1100
#define EC_CONFIG_BOOTSTRAP_MBOXIN_SIZE     	0x300
#define EC_CONFIG_BOOTSTRAP_MBOXOUT_ADR     	0x1400
#define EC_CONFIG_BOOTSTRAP_MBOXOUT_SIZE    	0x300
#define EC_CONFIG_DYNAMIC_OD                	1
#define EC_CONFIG_DYN_CFG                   	1
#define EC_CONFIG_EMERGENCY                 	1
#define EC_CONFIG_EMERGENCY_QUEUESIZE       	8
#define EC_CONFIG_EOE                       	1
#define EC_CONFIG_FOE                       	1
#define EC_CONFIG_LED_EMULATION             	1
#define EC_CONFIG_PDI_INTERRUPT             	1
#define EC_CONFIG_PDO_RAM_BUF_SIZE          	128
#define EC_CONFIG_REQUEST_TIMEOUT           	100
#define EC_CONFIG_RESPONSE_TIMEOUT          	1000
#define EC_CONFIG_SDO_INFO                  	1
#define EC_CONFIG_SDO_SEG_BUF_LEN           	1024
#define EC_CONFIG_SLAVE                     	1
#define EC_CONFIG_STATE_TIMEOUT_BTOINIT     	5000
#define EC_CONFIG_STATE_TIMEOUT_BTOSAFE     	200
#define EC_CONFIG_STATE_TIMEOUT_INITPREOP   	3000
#define EC_CONFIG_STATE_TIMEOUT_SAFEOP      	10000
#define EC_CONFIG_MAX_OD_ELEMENTS           	14
#define EC_CONFIG_MAX_RPDO_DATA             	5
#define EC_CONFIG_MAX_TPDO_DATA             	5
#define EC_CONFIG_PDO_BYPASS_BUF_LEN   	64
#define EC_CONFIG_PDO_INPUT_BYPASS     	1
#define PDO_INPUT_BYPASS_FCT	 goal_ecatInDmMap
#define EC_CONFIG_PDO_OUTPUT_BYPASS    	1
#define PDO_OUTPUT_BYPASS_FCT	 goal_ecatOutDmMap


/*
 * Hardware configuration 0: 0
 */


/*
 * CPU Setup
 */
#ifdef EC_CONFIG_USE_TARGET_0
#define EC_CONFIG_CPU_FAMILY_GOAL           	1
#define EC_CONFIG_LIB_TIMER                 	1
#define EC_CONFIG_TIMER_INC                 	10
#include <goal_ethercat.h>
#endif /* EC_CONFIG_USE_TARGET_0*/


/*
 * Compiler Setup
 */
#ifdef EC_CONFIG_USE_TARGET_0
#define EC_CONFIG_COMPILER_GOAL             	1
#  include <ec_default.h>
#endif /* EC_CONFIG_USE_TARGET_0 */




/*
 * EtherCAT Services
 */

/* Sync Manager 0 */
#define EC_CONFIG_SM_0_COMM_TYPE 1
#define EC_CONFIG_SM_0_START_ADR 0x1000
#define EC_CONFIG_SM_0_DEFAULT_SIZE 128
#define EC_CONFIG_SM_0_MIN_SIZE 128
#define EC_CONFIG_SM_0_MAX_SIZE 128
#define EC_CONFIG_SM_0_IRQ_ECAT_EVENT_ENABLED 0
#define EC_CONFIG_SM_0_IRQ_PDI_EVENT_ENABLED 1
#define EC_CONFIG_SM_0_WATCHDOG_ENABLED 0
#define EC_CONFIG_SM_0_ENABLED 1

/* Sync Manager 1 */
#define EC_CONFIG_SM_1_COMM_TYPE 2
#define EC_CONFIG_SM_1_START_ADR 0x1080
#define EC_CONFIG_SM_1_DEFAULT_SIZE 128
#define EC_CONFIG_SM_1_MIN_SIZE 128
#define EC_CONFIG_SM_1_MAX_SIZE 128
#define EC_CONFIG_SM_1_IRQ_ECAT_EVENT_ENABLED 0
#define EC_CONFIG_SM_1_IRQ_PDI_EVENT_ENABLED 1
#define EC_CONFIG_SM_1_WATCHDOG_ENABLED 0
#define EC_CONFIG_SM_1_ENABLED 1

/* Sync Manager 2 */
#define EC_CONFIG_SM_2_COMM_TYPE 3
#define EC_CONFIG_SM_2_START_ADR 0x1100
#define EC_CONFIG_SM_2_DEFAULT_SIZE 5
#define EC_CONFIG_SM_2_IRQ_ECAT_EVENT_ENABLED 0
#define EC_CONFIG_SM_2_IRQ_PDI_EVENT_ENABLED 1
#define EC_CONFIG_SM_2_WATCHDOG_ENABLED 1
#define EC_CONFIG_SM_2_ENABLED 1

/* Sync Manager 3 */
#define EC_CONFIG_SM_3_COMM_TYPE 4
#define EC_CONFIG_SM_3_START_ADR 0x1400
#define EC_CONFIG_SM_3_DEFAULT_SIZE 5
#define EC_CONFIG_SM_3_IRQ_ECAT_EVENT_ENABLED 0
#define EC_CONFIG_SM_3_IRQ_PDI_EVENT_ENABLED 1
#define EC_CONFIG_SM_3_WATCHDOG_ENABLED 0
#define EC_CONFIG_SM_3_ENABLED 1


/* Sync Manager general */
/* number of Sync Manager used for the standard receive mailbox */
#define EC_CONFIG_SM_STD_RX_MB 0
/* number of Sync Manager used for the standard send mailbox */
#define EC_CONFIG_SM_STD_TX_MB 1


#define EC_CONFIG_DYN_PDO_MAPPING           	1
#define EC_CONFIG_MAPPING_CNT               	4
#define EC_CONFIG_PDO_CONSUMER              	1
#define EC_CONFIG_PDO_CONS_MAP_CNT          	2
#define EC_CONFIG_PDO_PRODUCER              	1
#define EC_CONFIG_PDO_PROD_MAP_CNT          	2
#define EC_CONFIG_SDO_SERVER                	1

/*
 * special DEFINE part - don't edit here, it is changed by
 * the Industrial Communication Creator
 */
#define EC_CONFIG_SPECIAL_START	1
/* access word-wise, use special mem-access */
#define EC_CONFIG_BSP 1

#define EC_CONFIG_SPECIAL_END	1

/*
 * user specific part - will not be changed by
 * the Industrial Communication Creator
 */
#define EC_CONFIG_USER_SPEC_START	1

#define EC_CONFIG_USER_SPEC_END	1

#endif /* ECAT_CONF_H */
