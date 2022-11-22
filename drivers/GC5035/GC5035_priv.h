/******************************************************************************\
|* Copyright (c) 2020 by VeriSilicon Holdings Co., Ltd. ("VeriSilicon")       *|
|* All Rights Reserved.                                                       *|
|*                                                                            *|
|* The material in this file is confidential and contains trade secrets of    *|
|* of VeriSilicon.  This is proprietary information owned or licensed by      *|
|* VeriSilicon.  No part of this work may be disclosed, reproduced, copied,   *|
|* transmitted, or used in any way for any purpose, without the express       *|
|* written permission of VeriSilicon.                                         *|
|*                                                                            *|
\******************************************************************************/
/**
 * @file GC5035_priv.h
 *
 * @brief Interface description for image sensor specific implementation (iss).
 *
 *****************************************************************************/
/**
 * @page module_name_page Module Name
 * Describe here what this module does.
 *
 * For a detailed list of functions and implementation detail refer to:
 * - @ref module_name
 *
 * @defgroup gc5035_priv
 * @{
 *
 */
#ifndef __GC5035_PRIV_H__
#define __GC5035_PRIV_H__

#include <ebase/types.h>
#include <common/return_codes.h>
#include <hal/hal_api.h>
#include <isi/isi_common.h>
#include "vvsensor.h"



#ifdef __cplusplus
extern "C"
{
#endif



/*****************************************************************************
 * SC control registers
 *****************************************************************************/
#define GC5035_PIDH                         (0x300A)  //R  - Product ID High Byte MSBs
#define GC5035_PIDL                         (0x300B)  //R  - Product ID Low Byte LSBs

/*****************************************************************************
 * Default values
 *****************************************************************************/

 // Make sure that these static settings are reflecting the capabilities defined
// in IsiGetCapsIss (further dynamic setup may alter these default settings but
// often does not if there is no choice available).

/*****************************************************************************
 * SC control registers
 *****************************************************************************/
#define GC5035_PIDH_DEFAULT                        (0x27) //read only
#define GC5035_PIDL_DEFAULT                        (0x70) //read only

typedef struct GC5035_Context_s
{
    IsiSensorContext_t  IsiCtx;                 /**< common context of ISI and ISI driver layer; @note: MUST BE FIRST IN DRIVER CONTEXT */

    struct vvcam_mode_info SensorMode;
    uint32_t            KernelDriverFlag;
    char                SensorRegCfgFile[128];

    uint32_t              HdrMode;
    uint32_t              Resolution;
    uint32_t              MaxFps;
    uint32_t              MinFps;
    uint32_t              CurrFps;
    //// modify below here ////

    IsiSensorConfig_t   Config;                 /**< sensor configuration */
    bool_t              Configured;             /**< flags that config was applied to sensor */
    bool_t              Streaming;              /**< flags that csensor is streaming data */
    bool_t              TestPattern;            /**< flags that sensor is streaming test-pattern */

    bool_t              isAfpsRun;              /**< if true, just do anything required for Afps parameter calculation, but DON'T access SensorHW! */

    float               one_line_exp_time;
    uint16_t            MaxIntegrationLine;
    uint16_t            MinIntegrationLine;
    uint32_t            gain_accuracy;

    uint16_t            FrameLengthLines;       /**< frame line length */
    uint16_t            CurFrameLengthLines;

    float               AecMinGain;
    float               AecMaxGain;
    float               AecMinIntegrationTime;
    float               AecMaxIntegrationTime;

    float               AecIntegrationTimeIncrement; /**< _smallest_ increment the sensor/driver can handle (e.g. used for sliders in the application) */
    float               AecGainIncrement;            /**< _smallest_ increment the sensor/driver can handle (e.g. used for sliders in the application) */

    float               AecCurIntegrationTime;
    float               AecCurVSIntegrationTime;
    float               AecCurLongIntegrationTime;
    float               AecCurGain;
    float               AecCurVSGain;
    float               AecCurLongGain;

    uint32_t            LastExpLine;
    uint32_t            LastVsExpLine;
    uint32_t            LastLongExpLine;

    uint32_t            LastGain;
    uint32_t            LastVsGain;
    uint32_t            LastLongGain;

    bool                GroupHold;
    uint32_t            OldGain;
    uint32_t            OldVsGain;
    uint32_t            OldIntegrationTime;
    uint32_t            OldVsIntegrationTime;
    uint32_t            OldGainHcg;
    uint32_t            OldAGainHcg;
    uint32_t            OldGainLcg;
    uint32_t            OldAGainLcg;
    int                 subdev;
    uint8_t             pattern;

    float               CurHdrRatio;
} GC5035_Context_t;

static RESULT GC5035_IsiCreateSensorIss(IsiSensorInstanceConfig_t *
                          pConfig);

static RESULT GC5035_IsiInitSensorIss(IsiSensorHandle_t handle);

static RESULT GC5035_IsiReleaseSensorIss(IsiSensorHandle_t handle);

static RESULT GC5035_IsiGetCapsIss(IsiSensorHandle_t handle,
                         IsiSensorCaps_t * pIsiSensorCaps);

static RESULT GC5035_IsiSetupSensorIss(IsiSensorHandle_t handle,
                         const IsiSensorConfig_t *
                         pConfig);

static RESULT GC5035_IsiSensorSetStreamingIss(IsiSensorHandle_t handle,
                               bool_t on);

static RESULT GC5035_IsiSensorSetPowerIss(IsiSensorHandle_t handle,
                            bool_t on);

static RESULT GC5035_IsiGetSensorRevisionIss(IsiSensorHandle_t handle,
                               uint32_t * p_value);

static RESULT GC5035_IsiSetBayerPattern(IsiSensorHandle_t handle,
                          uint8_t pattern);

static RESULT GC5035_IsiGetGainLimitsIss(IsiSensorHandle_t handle,
                             float *pMinGain,
                             float *pMaxGain);

static RESULT GC5035_IsiGetIntegrationTimeLimitsIss(IsiSensorHandle_t
                                 handle,
                                 float
                                 *pMinIntegrationTime,
                                 float
                                 *pMaxIntegrationTime);

static RESULT GC5035_IsiExposureControlIss(IsiSensorHandle_t handle,
                            float NewGain,
                            float NewIntegrationTime,
                            uint8_t *
                            pNumberOfFramesToSkip,
                            float *pSetGain,
                            float *pSetIntegrationTime,
                            float *hdr_ratio);

static RESULT GC5035_IsiGetGainIss(IsiSensorHandle_t handle,
                        float *pSetGain);

static RESULT GC5035_IsiGetVSGainIss(IsiSensorHandle_t handle,
                          float *pSetGain);

static RESULT GC5035_IsiGetGainIncrementIss(IsiSensorHandle_t handle,
                             float *pIncr);

static RESULT GC5035_IsiSetGainIss(IsiSensorHandle_t handle,
                        float NewGain, float *pSetGain,
                        float *hdr_ratio);

static RESULT GC5035_IsiSetVSGainIss(IsiSensorHandle_t handle,
                          float NewIntegrationTime,
                          float NewGain, float *pSetGain,
                          float *hdr_ratio);

static RESULT GC5035_IsiGetIntegrationTimeIss(IsiSensorHandle_t handle,
                               float
                               *pSetIntegrationTime);

static RESULT GC5035_IsiGetVSIntegrationTimeIss(IsiSensorHandle_t
                             handle,
                             float
                             *pSetIntegrationTime);

static RESULT GC5035_IsiGetIntegrationTimeIncrementIss(IsiSensorHandle_t handle,
                             float *pIncr);

static RESULT GC5035_IsiSetIntegrationTimeIss(IsiSensorHandle_t handle,
                               float NewIntegrationTime,
                               float
                               *pSetIntegrationTime,
                               uint8_t *
                               pNumberOfFramesToSkip,
                               float *hdr_ratio);

static RESULT GC5035_IsiSetVSIntegrationTimeIss(IsiSensorHandle_t
                             handle,
                             float
                             NewIntegrationTime,
                             float
                             *pSetIntegrationTime,
                             uint8_t *
                             pNumberOfFramesToSkip,
                             float *hdr_ratio);

RESULT GC5035_IsiGetResolutionIss(IsiSensorHandle_t handle, uint16_t *pwidth, uint16_t *pheight);

static RESULT GC5035_IsiResetSensorIss(IsiSensorHandle_t handle);


#ifdef __cplusplus
}
#endif

/* @} gc5035priv */

#endif    /* __GC5035PRIV_H__ */

