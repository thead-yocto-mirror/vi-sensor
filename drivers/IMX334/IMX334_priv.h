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
 * @file IMX334_priv.h
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
 * @defgroup IMX334_priv
 * @{
 *
 */
#ifndef __IMX334_PRIV_H__
#define __IMX334_PRIV_H__

#include <ebase/types.h>
#include <common/return_codes.h>
#include <hal/hal_api.h>
#include <isi/isi_common.h>
#include "vvsensor.h"



#ifdef __cplusplus
extern "C"
{
#endif

typedef struct IMX334_Context_s
{
    IsiSensorContext_t  IsiCtx;                 /**< common context of ISI and ISI driver layer; @note: MUST BE FIRST IN DRIVER CONTEXT */

    struct vvcam_mode_info SensorMode;
    uint32_t            KernelDriverFlag;
    char                SensorRegCfgFile[128];

    uint32_t              HdrMode;
    IsiResolution_t       Resolution;
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

    float               AecCurGain;
    float               AecCurIntegrationTime;
    float               AecCurGainSEF1;
    float               AecCurIntegrationTimeSEF1;

    bool                GroupHold;
    uint32_t            OldGain;
    uint32_t            OldIntegrationTime;
    uint32_t            OldGainSEF1;
    uint32_t            OldIntegrationTimeSEF1;

    int                 subdev;
    bool                enableHdr;
    uint8_t             pattern;
    float               CurHdrRatio;
} IMX334_Context_t;

static RESULT IMX334_IsiCreateSensorIss(IsiSensorInstanceConfig_t *
                          pConfig);

static RESULT IMX334_IsiInitSensorIss(IsiSensorHandle_t handle);

static RESULT IMX334_IsiReleaseSensorIss(IsiSensorHandle_t handle);

static RESULT IMX334_IsiGetCapsIss(IsiSensorHandle_t handle,
                         IsiSensorCaps_t * pIsiSensorCaps);

static RESULT IMX334_IsiSetupSensorIss(IsiSensorHandle_t handle,
                         const IsiSensorConfig_t *
                         pConfig);

static RESULT IMX334_IsiSensorSetStreamingIss(IsiSensorHandle_t handle,
                               bool_t on);

static RESULT IMX334_IsiSensorSetPowerIss(IsiSensorHandle_t handle,
                            bool_t on);

static RESULT IMX334_IsiGetSensorRevisionIss(IsiSensorHandle_t handle,
                               uint32_t * p_value);

static RESULT IMX334_IsiSetBayerPattern(IsiSensorHandle_t handle,
                          uint8_t pattern);

static RESULT IMX334_IsiGetGainLimitsIss(IsiSensorHandle_t handle,
                             float *pMinGain,
                             float *pMaxGain);

static RESULT IMX334_IsiGetIntegrationTimeLimitsIss(IsiSensorHandle_t
                                 handle,
                                 float
                                 *pMinIntegrationTime,
                                 float
                                 *pMaxIntegrationTime);

static RESULT IMX334_IsiExposureControlIss(IsiSensorHandle_t handle,
                            float NewGain,
                            float NewIntegrationTime,
                            uint8_t *
                            pNumberOfFramesToSkip,
                            float *pSetGain,
                            float *pSetIntegrationTime,
                            float *hdr_ratio);

static RESULT IMX334_IsiGetGainIss(IsiSensorHandle_t handle,
                        float *pSetGain);

static RESULT IMX334_IsiGetSEF1GainIss(IsiSensorHandle_t handle,
                          float *pSetGain);

static RESULT IMX334_IsiGetGainIncrementIss(IsiSensorHandle_t handle,
                             float *pIncr);

static RESULT IMX334_IsiSetGainIss(IsiSensorHandle_t handle,
                        float NewGain, float *pSetGain,
                        float *hdr_ratio);

static RESULT IMX334_IsiSetSEF1GainIss(IsiSensorHandle_t handle,
                          float NewIntegrationTime,
                          float NewGain, float *pSetGain,
                          float *hdr_ratio);

static RESULT IMX334_IsiGetIntegrationTimeIss(IsiSensorHandle_t handle,
                               float
                               *pSetIntegrationTime);

static RESULT IMX334_IsiGetSEF1IntegrationTimeIss(IsiSensorHandle_t
                             handle,
                             float
                             *pSetIntegrationTime);

static RESULT IMX334_IsiGetIntegrationTimeIncrementIss(IsiSensorHandle_t handle,
                             float *pIncr);

static RESULT IMX334_IsiSetIntegrationTimeIss(IsiSensorHandle_t handle,
                               float NewIntegrationTime,
                               float
                               *pSetIntegrationTime,
                               uint8_t *
                               pNumberOfFramesToSkip,
                               float *hdr_ratio);

static RESULT IMX334_IsiSetSEF1IntegrationTimeIss(IsiSensorHandle_t
                             handle,
                             float
                             NewIntegrationTime,
                             float
                             *pSetIntegrationTime,
                             uint8_t *
                             pNumberOfFramesToSkip,
                             float *hdr_ratio);

static RESULT IMX334_IsiGetResolutionIss
    (IsiSensorHandle_t handle, uint16_t *pwidth, uint16_t *pheight);

static RESULT IMX334_IsiResetSensorIss(IsiSensorHandle_t handle);


#ifdef __cplusplus
}
#endif

/* @} IMX334priv */

#endif    /* __IMX334PRIV_H__ */

