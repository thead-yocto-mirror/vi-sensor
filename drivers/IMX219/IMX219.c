/******************************************************************************\ |* Copyright (c) 2020 by VeriSilicon Holdings Co., Ltd. ("VeriSilicon")       *| |* All Rights Reserved.                                                       *| |*                                                                            *| |* The material in this file is confidential and contains trade secrets of    *| |* of VeriSilicon.  This is proprietary information owned or licensed by      *| |* VeriSilicon.  No part of this work may be disclosed, reproduced, copied,   *| |* transmitted, or used in any way for any purpose, without the express       *| |* written permission of VeriSilicon.                                         *| |*                                                                            *|
\******************************************************************************/

#include <ebase/types.h>
#include <ebase/trace.h>
#include <ebase/builtins.h>
#include <common/return_codes.h>
#include <common/misc.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <isi/isi.h>
#include <isi/isi_iss.h>
#include <isi/isi_priv.h>
#include <vvsensor.h>
#include "IMX219_priv.h"
#include "imx219.h"

CREATE_TRACER( IMX219_INFO , "IMX219: ", INFO,    1);
CREATE_TRACER( IMX219_WARN , "IMX219: ", WARNING, 1);
CREATE_TRACER( IMX219_ERROR, "IMX219: ", ERROR,   1);
CREATE_TRACER( IMX219_DEBUG,     "IMX219: ", INFO, 0);
CREATE_TRACER( IMX219_REG_INFO , "IMX219: ", INFO, 0);
CREATE_TRACER( IMX219_REG_DEBUG, "IMX219: ", INFO, 0);

#ifdef SUBDEV_V4L2
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#undef TRACE
#define TRACE(x, ...)
#endif

#define IMX219_MIN_GAIN_STEP    ( 1.0f/16.0f )  /**< min gain step size used by GUI (hardware min = 1/16; 1/16..32/16 depending on actual gain ) */
#define IMX219_MAX_GAIN_AEC     ( 32.0f )       /**< max. gain used by the AEC (arbitrarily chosen, hardware limit = 62.0, driver limit = 32.0 ) */
#define IMX219_VS_MAX_INTEGRATION_TIME (0.0018)

/*****************************************************************************
 *Sensor Info
*****************************************************************************/
static const char SensorName[16] = "IMX219";

static struct vvcam_mode_info pimx219_mode_info[] = {
    {
        .index     = 0,
        .width     = 1920,
        .height    = 1080,
        .fps       = 30,
        .hdr_mode  = SENSOR_MODE_LINEAR, 
        .bit_width = 10,
        .bayer_pattern = BAYER_RGGB,
        .mipi_phy_freq = 912, /* Pixel rate is fixed at 182.4M for all the modes */
        .mipi_line_num = 2,
        .preg_data = (void *)"imx219 sensor liner mode 1920*1080@30",
    },
};

static RESULT IMX219_IsiRegisterWriteIss(IsiSensorHandle_t handle, const uint32_t address, const uint32_t value);

static RESULT IMX219_IsiSensorSetPowerIss(IsiSensorHandle_t handle, bool_t on) {
    RESULT result = RET_SUCCESS;

    int ret = 0;
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL || pIMX219Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    TRACE(IMX219_INFO, "%s (enter)\n", __func__);

    int32_t enable = on;
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_POWER, &enable);
    if (ret != 0) {
        // to do
        //TRACE(IMX219_ERROR, "%s: sensor set power error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(IMX219_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT IMX219_IsiResetSensorIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;
    int ret = 0;

    TRACE(IMX219_INFO, "%s (enter)\n", __func__);

    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL || pIMX219Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_RESET, NULL);
    if (ret != 0) {
        TRACE(IMX219_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }

    TRACE(IMX219_INFO, "%s (exit)\n", __func__);
    return (result);
}

#ifdef SUBDEV_CHAR
static RESULT IMX219_IsiSensorSetClkIss(IsiSensorHandle_t handle, uint32_t clk) {
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;

    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL || pIMX219Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    TRACE(IMX219_INFO, "%s (enter)\n", __func__);

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_CLK, &clk);
    if (ret != 0) {
        // to do
        //TRACE(IMX219_ERROR, "%s: sensor set clk error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(IMX219_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT IMX219_IsiSensorGetClkIss
    (IsiSensorHandle_t handle, uint32_t * pclk) {
    RESULT result = RET_SUCCESS;
    int ret = 0;

    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL || pIMX219Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    TRACE(IMX219_INFO, "%s (enter)\n", __func__);

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_CLK, pclk);
    if (ret != 0) {
        // to do
        //TRACE(IMX219_ERROR, "%s: sensor get clk error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(IMX219_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT IMX219_IsiConfigSensorSCCBIss(IsiSensorHandle_t handle)
{
    return RET_SUCCESS;
}
#endif

static RESULT IMX219_IsiRegisterReadIss
    (IsiSensorHandle_t handle, const uint32_t address, uint32_t * p_value) {
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    TRACE(IMX219_INFO, "%s (enter)\n", __func__);

    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL || pIMX219Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    struct vvcam_sccb_data sccb_data;
    sccb_data.addr = address;
    sccb_data.data = 0;
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_READ_REG, &sccb_data);
    if (ret != 0) {
        TRACE(IMX219_ERROR, "%s: read sensor register error!\n",
              __func__);
        return (RET_FAILURE);
    }

    *p_value = sccb_data.data;

    TRACE(IMX219_INFO, "%s (exit) result = %d\n", __func__, result);
    return (result);
}

static RESULT IMX219_IsiRegisterWriteIss
    (IsiSensorHandle_t handle, const uint32_t address, const uint32_t value) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    TRACE(IMX219_INFO, "%s (enter) write %d\n", __func__, value);

    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL || pIMX219Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    struct vvcam_sccb_data sccb_data;
    sccb_data.addr = address;
    sccb_data.data = value;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_WRITE_REG, &sccb_data);
    if (ret != 0) {
        TRACE(IMX219_ERROR, "%s: write sensor register error!\n",
              __func__);
        return (RET_FAILURE);
    }

    TRACE(IMX219_INFO, "%s (exit) result = %d\n", __func__, result);
    return (result);
}

static RESULT IMX219_IsiQuerySensorSupportIss(HalHandle_t  HalHandle, vvcam_mode_info_array_t *pSensorSupportInfo)
{
    TRACE(IMX219_DEBUG, "enter %s", __func__);
    //int ret = 0;
    struct vvcam_mode_info_array *psensor_mode_info_arry;

    HalContext_t *pHalCtx = HalHandle;
    if ( pHalCtx == NULL ) {
        return RET_NULL_POINTER;
    }

    psensor_mode_info_arry = pSensorSupportInfo;
#if 0
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_QUERY, psensor_mode_info_arry);
    if (ret != 0) {
        TRACE(IMX219_ERROR, "%s: sensor kernel query error! Use isi query\n",__func__);
        psensor_mode_info_arry->count = sizeof(pimx219_mode_info) / sizeof(struct vvcam_mode_info);
        memcpy(psensor_mode_info_arry->modes, pimx219_mode_info, sizeof(pimx219_mode_info));
    }
#endif
    psensor_mode_info_arry->count = sizeof(pimx219_mode_info) / sizeof(struct vvcam_mode_info);
    memcpy(psensor_mode_info_arry->modes, pimx219_mode_info, sizeof(pimx219_mode_info));
    
    TRACE(IMX219_DEBUG, "%s-%s-%d: cnt=%d\n", __FILE__, __func__, __LINE__, psensor_mode_info_arry->count);

    return RET_SUCCESS;
}

static  RESULT IMX219_IsiQuerySensorIss(IsiSensorHandle_t handle, vvcam_mode_info_array_t *pSensorInfo)
{
    RESULT result = RET_SUCCESS;
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL || pIMX219Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;
    IMX219_IsiQuerySensorSupportIss(pHalCtx,pSensorInfo);

    return result;
}

static RESULT IMX219_IsiGetSensorModeIss(IsiSensorHandle_t handle,void *mode)
{
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL) {
        return (RET_WRONG_HANDLE);
    }
    memcpy(mode,&(pIMX219Ctx->SensorMode), sizeof(pIMX219Ctx->SensorMode));

    return ( RET_SUCCESS );
}

static RESULT IMX219_IsiCreateSensorIss(IsiSensorInstanceConfig_t * pConfig) {
    RESULT result = RET_SUCCESS;
    IMX219_Context_t *pIMX219Ctx;

    TRACE(IMX219_INFO, "%s (enter) v1.6\n", __func__);

    if (!pConfig || !pConfig->pSensor)
        return (RET_NULL_POINTER);

    pIMX219Ctx = (IMX219_Context_t *) malloc(sizeof(IMX219_Context_t));
    if (!pIMX219Ctx) {
        TRACE(IMX219_ERROR, "%s: Can't allocate imx219 context\n",
              __func__);
        return (RET_OUTOFMEM);
    }

    MEMSET(pIMX219Ctx, 0, sizeof(IMX219_Context_t));

    result = HalAddRef(pConfig->HalHandle);
    if (result != RET_SUCCESS) {
        free(pIMX219Ctx);
        return (result);
    }

    pIMX219Ctx->IsiCtx.HalHandle = pConfig->HalHandle;
    pIMX219Ctx->IsiCtx.pSensor = pConfig->pSensor;
    pIMX219Ctx->GroupHold = BOOL_FALSE;
    pIMX219Ctx->OldGain = 0;
    pIMX219Ctx->OldIntegrationTime = 0;
    pIMX219Ctx->Configured = BOOL_FALSE;
    pIMX219Ctx->Streaming = BOOL_FALSE;
    pIMX219Ctx->TestPattern = BOOL_FALSE;
    pIMX219Ctx->isAfpsRun = BOOL_FALSE;
    pIMX219Ctx->SensorMode.index = pConfig->SensorModeIndex;
    pConfig->hSensor = (IsiSensorHandle_t) pIMX219Ctx;
#ifdef SUBDEV_CHAR
    struct vvcam_mode_info *SensorDefaultMode = NULL;
    for (int i=0; i < sizeof(pimx219_mode_info)/ sizeof(struct vvcam_mode_info); i++)
    {
        if (pimx219_mode_info[i].index == pIMX219Ctx->SensorMode.index)
        {
            SensorDefaultMode = &(pimx219_mode_info[i]);
            break;
        }
    }

    if (SensorDefaultMode != NULL)
    {
        strcpy(pIMX219Ctx->SensorRegCfgFile, get_vi_config_path());
        switch(SensorDefaultMode->index)
        {
            case 0:
                strcat(pIMX219Ctx->SensorRegCfgFile,
                    "IMX219_mipi4lane_1920x1080@30.txt");
                break;
            default:
                break;
        }

        if (access(pIMX219Ctx->SensorRegCfgFile, F_OK) == 0) {
            pIMX219Ctx->KernelDriverFlag = 0;
            memcpy(&(pIMX219Ctx->SensorMode),SensorDefaultMode,sizeof(struct vvcam_mode_info));
        } else {
            pIMX219Ctx->KernelDriverFlag = 1;
        }
    }else
    {
        pIMX219Ctx->KernelDriverFlag = 1;
    }

    result = IMX219_IsiSensorSetPowerIss(pIMX219Ctx, BOOL_TRUE);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    uint32_t SensorClkIn = 0;
    if (pIMX219Ctx->KernelDriverFlag) {
        result = IMX219_IsiSensorGetClkIss(pIMX219Ctx, &SensorClkIn);
        RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);
    }

    result = IMX219_IsiSensorSetClkIss(pIMX219Ctx, SensorClkIn);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    result = IMX219_IsiResetSensorIss(pIMX219Ctx);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    pIMX219Ctx->pattern = ISI_BPAT_BGBGGRGR;

    if (!pIMX219Ctx->KernelDriverFlag) {
        result = IMX219_IsiConfigSensorSCCBIss(pIMX219Ctx);
        RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);
    }
#endif

    TRACE(IMX219_INFO, "%s (exit pConfig->hSensor = %p)\n", __func__, pConfig->hSensor);
    return (result);
}

static RESULT IMX219_IsiGetRegCfgIss(const char *registerFileName,
                     struct vvcam_sccb_array *arry)
{
    if (NULL == registerFileName) {
        TRACE(IMX219_ERROR, "%s:registerFileName is NULL\n", __func__);
        return (RET_NULL_POINTER);
    }
#ifdef SUBDEV_CHAR
    FILE *fp = NULL;
    fp = fopen(registerFileName, "rb");
    if (!fp) {
        TRACE(IMX219_ERROR, "%s:load register file  %s error!\n",
              __func__, registerFileName);
        return (RET_FAILURE);
    }

    char LineBuf[512];
    uint32_t FileTotalLine = 0;
    while (!feof(fp)) {
        fgets(LineBuf, 512, fp);
        FileTotalLine++;
    }

    arry->sccb_data =
        malloc(FileTotalLine * sizeof(struct vvcam_sccb_data));
    if (arry->sccb_data == NULL) {
        TRACE(IMX219_ERROR, "%s:malloc failed NULL Point!\n", __func__,
              registerFileName);
        return (RET_FAILURE);
    }
    rewind(fp);

    arry->count = 0;
    while (!feof(fp)) {
        memset(LineBuf, 0, sizeof(LineBuf));
        fgets(LineBuf, 512, fp);

        int result =
            sscanf(LineBuf, "0x%x 0x%x",
               &(arry->sccb_data[arry->count].addr),
               &(arry->sccb_data[arry->count].data));
        if (result != 2)
            continue;
        arry->count++;

    }
#endif

    return 0;
}

static RESULT IMX219_IsiInitSensorIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;

    int ret = 0;
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;

    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;
    TRACE(IMX219_INFO, "%s (enter handle = %p)\n", __func__, handle);

    if (pIMX219Ctx == NULL) {
        return (RET_WRONG_HANDLE);
    }
	TRACE(IMX219_INFO, "%s (pIMX219Ctx->KernelDriverFlag = %d)\n", __func__, pIMX219Ctx->KernelDriverFlag);
    if (pIMX219Ctx->KernelDriverFlag) {
        ;
    } else {
        /* sensor doesn't enter LP-11 state upon power up until and unless
        * streaming is started, so upon power up switch the modes to:
        * streaming -> standby
        */
        ret = IMX219_IsiRegisterWriteIss(handle, 0x0100, 0x1);
        if (ret != 0) {
            return (RET_FAILURE);
        }
        ret = IMX219_IsiRegisterWriteIss(handle, 0x0100, 0x0);
        if (ret != 0) {
            return (RET_FAILURE);
        }
        osSleep(1);

		TRACE(IMX219_INFO, "%s (001)\n", __func__);
        struct vvcam_sccb_array arry;
        result = IMX219_IsiGetRegCfgIss(pIMX219Ctx->SensorRegCfgFile, &arry);
        if (result != 0) {
            TRACE(IMX219_ERROR,
                  "%s:IMX219_IsiGetRegCfgIss error!\n", __func__);
            return (RET_FAILURE);
        }

        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_WRITE_ARRAY, &arry);
        if (ret != 0) {
            TRACE(IMX219_ERROR, "%s:Sensor Write Reg arry error!\n",
                  __func__);
            return (RET_FAILURE);
        }
		TRACE(IMX219_INFO, "%s (pIMX219Ctx->SensorMode.index = %d)\n", __func__, pIMX219Ctx->SensorMode.index);
        switch(pIMX219Ctx->SensorMode.index)
        {
            case 0:
                pIMX219Ctx->one_line_exp_time = 0.00001890; // line_time = line_length / pclk
                pIMX219Ctx->FrameLengthLines = 0xAA8;
                pIMX219Ctx->CurFrameLengthLines = pIMX219Ctx->FrameLengthLines;
                pIMX219Ctx->MaxIntegrationLine = pIMX219Ctx->CurFrameLengthLines - 16;
                pIMX219Ctx->MinIntegrationLine = 1;
                pIMX219Ctx->AecMaxGain = 16;
                pIMX219Ctx->AecMinGain = 1;
                break;
            default:
                return (RET_FAILURE);
        }
		pIMX219Ctx->AecIntegrationTimeIncrement = pIMX219Ctx->one_line_exp_time;
		pIMX219Ctx->AecMinIntegrationTime =
			pIMX219Ctx->one_line_exp_time * pIMX219Ctx->MinIntegrationLine;
		pIMX219Ctx->AecMaxIntegrationTime =
			pIMX219Ctx->one_line_exp_time * pIMX219Ctx->MaxIntegrationLine;


        pIMX219Ctx->MaxFps  = pIMX219Ctx->SensorMode.fps;
        pIMX219Ctx->MinFps  = 1;
        pIMX219Ctx->CurrFps = pIMX219Ctx->MaxFps;
    }
    TRACE(IMX219_INFO, "%s (pIMX219Ctx->one_line_exp_time = %f)\n", __func__, pIMX219Ctx->one_line_exp_time);
    TRACE(IMX219_INFO, "%s (pIMX219Ctx->MinIntegrationLine = %d, pIMX219Ctx->MaxIntegrationLine = %d)\n", __func__, pIMX219Ctx->MinIntegrationLine, pIMX219Ctx->MaxIntegrationLine);
    return (result);
}

static RESULT IMX219_IsiReleaseSensorIss(IsiSensorHandle_t handle) {
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(IMX219_INFO, "%s (enter)\n", __func__);

    if (pIMX219Ctx == NULL)
        return (RET_WRONG_HANDLE);

    (void)IMX219_IsiSensorSetStreamingIss(pIMX219Ctx, BOOL_FALSE);
    (void)IMX219_IsiSensorSetPowerIss(pIMX219Ctx, BOOL_FALSE);
    (void)HalDelRef(pIMX219Ctx->IsiCtx.HalHandle);

    MEMSET(pIMX219Ctx, 0, sizeof(IMX219_Context_t));
    free(pIMX219Ctx);
    TRACE(IMX219_INFO, "%s (exit)\n", __func__);
    return (result);
}

struct imx219_fmt {
    int width;
    int height;
    int fps;
};

static RESULT IMX219_IsiSetupSensorIss
    (IsiSensorHandle_t handle, const IsiSensorConfig_t * pConfig) {

    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    RESULT result = RET_SUCCESS;

    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (!pIMX219Ctx) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pConfig) {
        TRACE(IMX219_ERROR,
              "%s: Invalid configuration (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    if (pIMX219Ctx->Streaming != BOOL_FALSE) {
        return RET_WRONG_STATE;
    }

    memcpy(&pIMX219Ctx->Config, pConfig, sizeof(IsiSensorConfig_t));

    /* 1.) SW reset of image sensor (via I2C register interface)  be careful, bits 6..0 are reserved, reset bit is not sticky */
    TRACE(IMX219_DEBUG, "%s: IMX219 System-Reset executed\n", __func__);
    osSleep(100);

    //IMX219_AecSetModeParameters not defined yet as of 2021/8/9.
    //result = IMX219_AecSetModeParameters(pIMX219Ctx, pConfig);
    //if (result != RET_SUCCESS) {
    //    TRACE(IMX219_ERROR, "%s: SetupOutputWindow failed.\n",
    //          __func__);
    //    return (result);
    //}
#if 1
    struct imx219_fmt fmt;
    fmt.width = pConfig->Resolution.width;
    fmt.height = pConfig->Resolution.height;

    ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fmt);//result = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fmt);
#endif
    pIMX219Ctx->Configured = BOOL_TRUE;
    TRACE(IMX219_INFO, "%s: (exit) ret=0x%x \n", __func__, result);
    return result;
}

static RESULT IMX219_IsiChangeSensorResolutionIss(IsiSensorHandle_t handle, uint16_t width, uint16_t height) {
    RESULT result = RET_SUCCESS;
#if 0
    struct imx219_fmt fmt;
    fmt.width = width;
    fmt.height = height;

    int ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fmt);
#endif
    TRACE(IMX219_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT IMX219_IsiSensorSetStreamingIss
    (IsiSensorHandle_t handle, bool_t on) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    TRACE(IMX219_INFO, "%s (enter)\n", __func__);

    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL || pIMX219Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    if (pIMX219Ctx->Configured != BOOL_TRUE)
        return RET_WRONG_STATE;

    int32_t enable = (uint32_t) on;

#if 0
    IMX219_IsiRegisterWriteIss(handle, 0x0601, 0x2); // pattern=IMX219_TEST_PATTERN_SOLID_COLOR
    IMX219_IsiRegisterWriteIss(handle, 0x0603, 0xff); // pattern
    IMX219_IsiRegisterWriteIss(handle, 0x0605, 0x2); // pattern
    IMX219_IsiRegisterWriteIss(handle, 0x0607, 0x2); // pattern
    IMX219_IsiRegisterWriteIss(handle, 0x0609, 0x2); // pattern
#endif

    if (on == true) {
        ret = IMX219_IsiRegisterWriteIss(handle, 0x0100, 0x1);
    } else {
        ret = IMX219_IsiRegisterWriteIss(handle, 0x0100, 0x00);
    }

    if (ret != 0) {
        return (RET_FAILURE);
    }

    pIMX219Ctx->Streaming = on;

    TRACE(IMX219_INFO, "%s (exit)\n", __func__);
    return (result);
}

static int32_t sensor_get_chip_id(IsiSensorHandle_t handle, uint32_t *chip_id)
{
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    int32_t chip_id_high = 0;
    int32_t chip_id_low = 0;

    ret = IMX219_IsiRegisterReadIss(handle, 0x0, &chip_id_high);
    if (ret != 0) {
        TRACE(IMX219_ERROR,
            "%s: Read Sensor correct ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    ret = IMX219_IsiRegisterReadIss(handle, 0x1, &chip_id_low);
    if (ret != 0) {
        TRACE(IMX219_ERROR,
            "%s: Read Sensor correct ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    *chip_id = ((chip_id_high & 0xff)<<8) | (chip_id_low & 0xff);

    return 0;
}

static RESULT IMX219_IsiCheckSensorConnectionIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    uint32_t correct_id = 0x219;
    uint32_t sensor_id = 0;

    TRACE(IMX219_INFO, "%s (enter)\n", __func__);

    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL || pIMX219Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    ret = sensor_get_chip_id(handle, &sensor_id);
    if (ret != 0) {
        TRACE(IMX219_ERROR,
            "%s: Read Sensor chip ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    if (correct_id != sensor_id) {
        TRACE(IMX219_ERROR, "%s:ChipID =0x%x sensor_id=%x error! \n",
              __func__, correct_id, sensor_id);
        return (RET_FAILURE);
    }

    printf("%s ChipID = 0x%08x, sensor_id = 0x%08x, success! \n", __func__,
          correct_id, sensor_id);
    TRACE(IMX219_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT IMX219_IsiGetSensorRevisionIss
    (IsiSensorHandle_t handle, uint32_t * p_value) {
    RESULT result = RET_SUCCESS;
    TRACE(IMX219_INFO, "%s (enter)\n", __func__);

    *p_value = 0X5690;
    TRACE(IMX219_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT IMX219_IsiGetGainLimitsIss
    (IsiSensorHandle_t handle, float *pMinGain, float *pMaxGain) {
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    /*TODO*/

    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (pIMX219Ctx == NULL) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pMinGain == NULL) || (pMaxGain == NULL)) {
        TRACE(IMX219_ERROR, "%s: NULL pointer received!!\n");
        return (RET_NULL_POINTER);
    }

    *pMinGain = pIMX219Ctx->AecMinGain;
    *pMaxGain = pIMX219Ctx->AecMaxGain;

    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);
    return (result);
}

static RESULT IMX219_IsiGetIntegrationTimeLimitsIss
    (IsiSensorHandle_t handle,
     float *pMinIntegrationTime, float *pMaxIntegrationTime) {
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    /*TODO*/

    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);
    if (pIMX219Ctx == NULL) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pMinIntegrationTime == NULL) || (pMaxIntegrationTime == NULL)) {
        TRACE(IMX219_ERROR, "%s: NULL pointer received!!\n");
        return (RET_NULL_POINTER);
    }

    *pMinIntegrationTime = pIMX219Ctx->AecMinIntegrationTime;
    *pMaxIntegrationTime = pIMX219Ctx->AecMaxIntegrationTime;

    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX219_IsiGetGainIss(IsiSensorHandle_t handle, float *pSetGain) {
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (pIMX219Ctx == NULL) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pSetGain == NULL) {
        return (RET_NULL_POINTER);
    }

    *pSetGain = pIMX219Ctx->AecCurGain;

    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX219_IsiGetLongGainIss(IsiSensorHandle_t handle, float *gain)
{
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;

    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (pIMX219Ctx == NULL) {
        TRACE(IMX219_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }

    if (gain == NULL) {
        return (RET_NULL_POINTER);
    }

    *gain = pIMX219Ctx->AecCurLongGain;

    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);

    return (RET_SUCCESS);
}

RESULT IMX219_IsiGetVSGainIss(IsiSensorHandle_t handle, float *pSetGain) {
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (pIMX219Ctx == NULL) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pSetGain == NULL) {
        return (RET_NULL_POINTER);
    }

    *pSetGain = pIMX219Ctx->AecCurVSGain;

    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);

    return (result);
}

RESULT IMX219_IsiGetGainIncrementIss(IsiSensorHandle_t handle, float *pIncr) {
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (pIMX219Ctx == NULL) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pIncr == NULL)
        return (RET_NULL_POINTER);

    *pIncr = pIMX219Ctx->AecGainIncrement;

    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);

    return (result);
}

RESULT IMX219_IsiSetGainIss
    (IsiSensorHandle_t handle,
     float NewGain, float *pSetGain, float *hdr_ratio) {

    RESULT result = RET_SUCCESS;
    int32_t again = 0, dgain = 0, ret = 0;
    float gain_left;


    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL || pIMX219Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;
    //IMX219 specific
    again = 256-256/NewGain;
    if (again >=232) {
        again = 232;
    }
    gain_left = NewGain / (256.0 / (256.0 - (float)again)); // what's left after again
    TRACE(IMX219_INFO, "%s: NewGain=%f again=%d, gain_left=%f dgain integer=%d dgain float=%f dgain float reg=%u\n"
        , __func__, NewGain, again, gain_left, (unsigned int)gain_left, (gain_left - (unsigned int)gain_left), (unsigned int)((float)(gain_left - (unsigned int)gain_left) * 256.0));

    ret = IMX219_IsiRegisterWriteIss(handle, 0x157, again);
    if (ret != 0) {
        return (RET_FAILURE);
    }

    ret = IMX219_IsiRegisterWriteIss(handle, 0x158, ((unsigned int)gain_left > 15 ) ? 15 : (unsigned int)gain_left);
    if (ret != 0) {
        return (RET_FAILURE);
    }

    ret = IMX219_IsiRegisterWriteIss(handle, 0x159,  (unsigned int)((float)(gain_left - (unsigned int)gain_left) * 256.0));
    if (ret != 0) {
        return (RET_FAILURE);
    }

    pIMX219Ctx->AecCurGain = ((float)(NewGain));
    *pSetGain = pIMX219Ctx->AecCurGain;
    TRACE(IMX219_DEBUG, "%s: g=%f\n", __func__, *pSetGain);
    return (result);
}

RESULT IMX219_IsiSetLongGainIss(IsiSensorHandle_t handle, float gain)
{
    int ret = 0;
    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;

    if (!pIMX219Ctx || !pIMX219Ctx->IsiCtx.HalHandle)
    {
        TRACE(IMX219_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }

    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    uint32_t SensorGain = 0;
    SensorGain = gain * pIMX219Ctx->gain_accuracy;
    if (pIMX219Ctx->LastLongGain != SensorGain)
    {

        /*TODO*/
#if 0
        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_LONG_GAIN, &SensorGain);
        if (ret != 0)
        {
            return (RET_FAILURE);
            TRACE(IMX219_ERROR,"%s: set long gain failed\n");

        }
#endif
        pIMX219Ctx->LastLongGain = SensorGain;
        pIMX219Ctx->AecCurLongGain = gain;
    }

    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT IMX219_IsiSetVSGainIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float NewGain, float *pSetGain, float *hdr_ratio) {
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;
    RESULT result = RET_SUCCESS;
#if 0
    float Gain = 0.0f;

    uint32_t ucGain = 0U;
    uint32_t again = 0U;
#endif

    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (!pIMX219Ctx) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetGain || !hdr_ratio)
        return (RET_NULL_POINTER);

    uint32_t SensorGain = 0;
    SensorGain = NewGain * pIMX219Ctx->gain_accuracy;

    /*TODO*/
    //ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_VSGAIN, &SensorGain);

    pIMX219Ctx->AecCurVSGain = NewGain;
    *pSetGain = pIMX219Ctx->AecCurGain;
    TRACE(IMX219_DEBUG, "%s: g=%f\n", __func__, *pSetGain);
    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX219_IsiSetBayerPattern(IsiSensorHandle_t handle, uint8_t pattern)
{

    RESULT result = RET_SUCCESS;
#if 0
    uint8_t h_shift = 0, v_shift = 0;
    uint32_t val_h = 0, val_l = 0;
    uint16_t val = 0;
    uint8_t Start_p = 0;
    bool_t streaming_status;
    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL || pIMX219Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    // pattern 0:B 1:GB 2:GR 3:R
    streaming_status = pIMX219Ctx->Streaming;
    result = IMX219_IsiSensorSetStreamingIss(handle, 0);
    switch (pattern) {
    case BAYER_BGGR:
        Start_p = 0;
        break;
    case BAYER_GBRG:
        Start_p = 1;
        break;
    case BAYER_GRBG:
        Start_p = 2;
        break;
    case BAYER_RGGB:
        Start_p = 3;
        break;
    }

    h_shift = Start_p % 2;
    v_shift = Start_p / 2;

    IMX219_IsiRegisterReadIss(handle, 0x30a0, &val_h);
    IMX219_IsiRegisterReadIss(handle, 0x30a1, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + h_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    IMX219_IsiRegisterWriteIss(handle, 0x30a0, (uint8_t)val_h);
    IMX219_IsiRegisterWriteIss(handle, 0x30a1, (uint8_t)val_l);

    IMX219_IsiRegisterReadIss(handle, 0x30a2, &val_h);
    IMX219_IsiRegisterReadIss(handle, 0x30a3, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + v_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    IMX219_IsiRegisterWriteIss(handle, 0x30a2, (uint8_t)val_h);
    IMX219_IsiRegisterWriteIss(handle, 0x30a3, (uint8_t)val_l);

    IMX219_IsiRegisterReadIss(handle, 0x30a4, &val_h);
    IMX219_IsiRegisterReadIss(handle, 0x30a5, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + h_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    IMX219_IsiRegisterWriteIss(handle, 0x30a4, (uint8_t)val_h);
    IMX219_IsiRegisterWriteIss(handle, 0x30a5, (uint8_t)val_l);

    IMX219_IsiRegisterReadIss(handle, 0x30a6, &val_h);
    IMX219_IsiRegisterReadIss(handle, 0x30a7, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + v_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    IMX219_IsiRegisterWriteIss(handle, 0x30a6, (uint8_t)val_h);
    IMX219_IsiRegisterWriteIss(handle, 0x30a7, (uint8_t)val_l);

    pIMX219Ctx->pattern = pattern;
    result = IMX219_IsiSensorSetStreamingIss(handle, streaming_status);
#endif

    return (result);
}

RESULT IMX219_IsiGetIntegrationTimeIss
    (IsiSensorHandle_t handle, float *pSetIntegrationTime)
{
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (!pIMX219Ctx) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetIntegrationTime)
        return (RET_NULL_POINTER);
    *pSetIntegrationTime = pIMX219Ctx->AecCurIntegrationTime;
    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX219_IsiGetLongIntegrationTimeIss(IsiSensorHandle_t handle, float *pIntegrationTime)
{
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (!pIMX219Ctx) {
        TRACE(IMX219_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }
    if (!pIntegrationTime)
        return (RET_NULL_POINTER);

    pIMX219Ctx->AecCurLongIntegrationTime =  pIMX219Ctx->AecCurIntegrationTime;

    *pIntegrationTime = pIMX219Ctx->AecCurLongIntegrationTime;
    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT IMX219_IsiGetVSIntegrationTimeIss
    (IsiSensorHandle_t handle, float *pSetIntegrationTime)
{
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (!pIMX219Ctx) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    if (!pSetIntegrationTime)
        return (RET_NULL_POINTER);

    *pSetIntegrationTime = pIMX219Ctx->AecCurVSIntegrationTime;
    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX219_IsiGetIntegrationTimeIncrementIss
    (IsiSensorHandle_t handle, float *pIncr)
{
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (!pIMX219Ctx) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pIncr)
        return (RET_NULL_POINTER);

    //_smallest_ increment the sensor/driver can handle (e.g. used for sliders in the application)
    *pIncr = pIMX219Ctx->AecIntegrationTimeIncrement;
    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX219_IsiSetIntegrationTimeIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float *pSetIntegrationTime,
     uint8_t * pNumberOfFramesToSkip, float *hdr_ratio)
{
    RESULT result = RET_SUCCESS;

    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    uint32_t exp_line = 0;
    uint32_t exp_line_old = 0;
    int ret = 0;

    TRACE(IMX219_INFO, "%s: (enter handle = %p)\n", __func__, handle);

    if (!pIMX219Ctx) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetIntegrationTime || !pNumberOfFramesToSkip) {
        TRACE(IMX219_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }
    exp_line = NewIntegrationTime / pIMX219Ctx->one_line_exp_time;
    exp_line_old = exp_line;
    exp_line =
        MIN(pIMX219Ctx->MaxIntegrationLine,
        MAX(pIMX219Ctx->MinIntegrationLine, exp_line));

    TRACE(IMX219_DEBUG, "%s: set AEC_PK_EXPO=0x%05x min_exp_line = %d, max_exp_line = %d\n", __func__, exp_line, pIMX219Ctx->MinIntegrationLine, pIMX219Ctx->MaxIntegrationLine);

    if (exp_line != pIMX219Ctx->OldIntegrationTime) {

        /*TODO*/
        //ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_EXP, &exp_line);
        pIMX219Ctx->OldIntegrationTime = exp_line;    // remember current integration time
        pIMX219Ctx->AecCurIntegrationTime =
            exp_line * pIMX219Ctx->one_line_exp_time;

        *pNumberOfFramesToSkip = 1U;    //skip 1 frame
    } else {
        *pNumberOfFramesToSkip = 0U;    //no frame skip
    }

    if (NewIntegrationTime > pIMX219Ctx->MaxIntegrationLine * pIMX219Ctx->one_line_exp_time)
        NewIntegrationTime = pIMX219Ctx->MaxIntegrationLine * pIMX219Ctx->one_line_exp_time;
    
    // IMX219 specific
    // int vts = exp_line + 16;

    ret = IMX219_IsiRegisterWriteIss(handle, 0x015A, (exp_line>>8));
    if (ret != 0) {
        return (RET_FAILURE);
    }
    ret = IMX219_IsiRegisterWriteIss(handle, 0x015B, (exp_line&0xff));
    if (ret != 0) {
        return (RET_FAILURE);
    }

    if (exp_line_old != exp_line) {
        *pSetIntegrationTime = pIMX219Ctx->AecCurIntegrationTime;
    } else {
        *pSetIntegrationTime = NewIntegrationTime;
    }

    TRACE(IMX219_DEBUG, "%s: Ti=%f\n", __func__, *pSetIntegrationTime);
    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX219_IsiSetLongIntegrationTimeIss(IsiSensorHandle_t handle,float IntegrationTime)
{
    int ret;
    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (!handle || !pIMX219Ctx->IsiCtx.HalHandle)
    {
        TRACE(IMX219_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    uint32_t exp_line = 0;
    exp_line = IntegrationTime / pIMX219Ctx->one_line_exp_time;
    exp_line = MIN(pIMX219Ctx->MaxIntegrationLine, MAX(pIMX219Ctx->MinIntegrationLine, exp_line));

    if (exp_line != pIMX219Ctx->LastLongExpLine)
    {
        if (pIMX219Ctx->KernelDriverFlag)
        {
            ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_LONG_EXP, &exp_line);
            if (ret != 0)
            {
                TRACE(IMX219_ERROR,"%s: set long gain failed\n");
                return RET_FAILURE;
            }
        }

        pIMX219Ctx->LastLongExpLine = exp_line;
        pIMX219Ctx->AecCurLongIntegrationTime =  pIMX219Ctx->LastLongExpLine*pIMX219Ctx->one_line_exp_time;
    }


    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT IMX219_IsiSetVSIntegrationTimeIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float *pSetVSIntegrationTime,
     uint8_t * pNumberOfFramesToSkip, float *hdr_ratio)
{
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    uint32_t exp_line = 0;

    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (!pIMX219Ctx) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetVSIntegrationTime || !pNumberOfFramesToSkip) {
        TRACE(IMX219_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    TRACE(IMX219_INFO,
          "%s:  maxIntegrationTime-=%f minIntegrationTime = %f\n", __func__,
          pIMX219Ctx->AecMaxIntegrationTime,
          pIMX219Ctx->AecMinIntegrationTime);


    exp_line = NewIntegrationTime / pIMX219Ctx->one_line_exp_time;
    exp_line =
        MIN(pIMX219Ctx->MaxIntegrationLine,
        MAX(pIMX219Ctx->MinIntegrationLine, exp_line));

    if (exp_line != pIMX219Ctx->OldVsIntegrationTime) {
    /*TODO*/
    //    ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_VSEXP, &exp_line);
    } else if (1){

        pIMX219Ctx->OldVsIntegrationTime = exp_line;
        pIMX219Ctx->AecCurVSIntegrationTime = exp_line * pIMX219Ctx->one_line_exp_time;    //remember current integration time
        *pNumberOfFramesToSkip = 1U;    //skip 1 frame
    } else {
        *pNumberOfFramesToSkip = 0U;    //no frame skip
    }

    *pSetVSIntegrationTime = pIMX219Ctx->AecCurVSIntegrationTime;

    TRACE(IMX219_DEBUG, "%s: NewIntegrationTime=%f\n", __func__,
          NewIntegrationTime);
    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX219_IsiExposureControlIss
    (IsiSensorHandle_t handle,
     float NewGain,
     float NewIntegrationTime,
     uint8_t * pNumberOfFramesToSkip,
     float *pSetGain, float *pSetIntegrationTime, float *hdr_ratio)
{
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    int TmpGain;
    /*TODO*/

    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (pIMX219Ctx == NULL) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pNumberOfFramesToSkip == NULL) || (pSetGain == NULL)
        || (pSetIntegrationTime == NULL)) {
        TRACE(IMX219_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    TRACE(IMX219_ERROR, "%s: g=%f, Ti=%f\n", __func__, NewGain,
          NewIntegrationTime);

    result = IMX219_IsiSetIntegrationTimeIss(handle, NewIntegrationTime,
                        pSetIntegrationTime,
                        pNumberOfFramesToSkip, hdr_ratio);
    result = IMX219_IsiSetGainIss(handle, NewGain, pSetGain, hdr_ratio);

    pIMX219Ctx->CurHdrRatio = *hdr_ratio;

    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);

    return result;
}

RESULT IMX219_IsiGetCurrentExposureIss
    (IsiSensorHandle_t handle, float *pSetGain, float *pSetIntegrationTime, float *hdr_ratio) {
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (pIMX219Ctx == NULL) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pSetGain == NULL) || (pSetIntegrationTime == NULL))
        return (RET_NULL_POINTER);

    *pSetGain = pIMX219Ctx->AecCurGain;
    *pSetIntegrationTime = pIMX219Ctx->AecCurIntegrationTime;
    *hdr_ratio = pIMX219Ctx->CurHdrRatio;

    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX219_IsiGetResolutionIss(IsiSensorHandle_t handle, uint16_t *pwidth, uint16_t *pheight) {
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (pIMX219Ctx == NULL) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    *pwidth = pIMX219Ctx->SensorMode.width;
    *pheight =  pIMX219Ctx->SensorMode.height;

    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX219_IsiGetSensorFpsIss(IsiSensorHandle_t handle, uint32_t * pfps)
{
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    RESULT result = RET_SUCCESS;


    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (pIMX219Ctx == NULL) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    if (pIMX219Ctx->KernelDriverFlag) {
       /*TODO*/
        ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_FPS, pfps);
        pIMX219Ctx->CurrFps = *pfps;
    }

    *pfps = pIMX219Ctx->CurrFps;

    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX219_IsiSetSensorFpsIss(IsiSensorHandle_t handle, uint32_t fps)
{
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    if (pIMX219Ctx == NULL) {
        TRACE(IMX219_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    if (fps > pIMX219Ctx->MaxFps) {
        TRACE(IMX219_ERROR,
              "%s: set fps(%d) out of range, correct to %d (%d, %d)\n",
              __func__, fps, pIMX219Ctx->MaxFps, pIMX219Ctx->MinFps,
              pIMX219Ctx->MaxFps);
        fps = pIMX219Ctx->MaxFps;
    }
    if (fps < pIMX219Ctx->MinFps) {
        TRACE(IMX219_ERROR,
              "%s: set fps(%d) out of range, correct to %d (%d, %d)\n",
              __func__, fps, pIMX219Ctx->MinFps, pIMX219Ctx->MinFps,
              pIMX219Ctx->MaxFps);
        fps = pIMX219Ctx->MinFps;
    }
    if (pIMX219Ctx->KernelDriverFlag) {
        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fps);
        if (ret != 0) {
            TRACE(IMX219_ERROR, "%s: set sensor fps=%d error\n",
                  __func__);
            return (RET_FAILURE);
        }

        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_SENSOR_MODE, &(pIMX219Ctx->SensorMode));
        {
            pIMX219Ctx->MaxIntegrationLine = pIMX219Ctx->SensorMode.ae_info.max_integration_time;
            pIMX219Ctx->AecMaxIntegrationTime = pIMX219Ctx->MaxIntegrationLine * pIMX219Ctx->one_line_exp_time;
        }
#ifdef SUBDEV_CHAR
        struct vvcam_ae_info_s ae_info;
        ret =
            ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_AE_INFO, &ae_info);
        if (ret != 0) {
            TRACE(IMX219_ERROR, "%s:sensor get ae info error!\n",
                  __func__);
            return (RET_FAILURE);
        }
        pIMX219Ctx->one_line_exp_time =
            (float)ae_info.one_line_exp_time_ns / 1000000000;
        pIMX219Ctx->MaxIntegrationLine = ae_info.max_integration_time;
        pIMX219Ctx->AecMaxIntegrationTime =
            pIMX219Ctx->MaxIntegrationLine *
            pIMX219Ctx->one_line_exp_time;
#endif
    }

    TRACE(IMX219_INFO, "%s: set sensor fps = %d\n", __func__,
          pIMX219Ctx->CurrFps);

    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);
    return (result);
}

static RESULT IMX219_IsiActivateTestPattern(IsiSensorHandle_t handle,
                        const bool_t enable)
{
    RESULT result = RET_SUCCESS;

    TRACE(IMX219_INFO, "%s: (enter)\n", __func__);

    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL || pIMX219Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    if (pIMX219Ctx->Configured != BOOL_TRUE)
        return RET_WRONG_STATE;

    if (BOOL_TRUE == enable) {
        //result = IMX219_IsiRegisterWriteIss(handle, 0x3253, 0x80);
    } else {
        //result = IMX219_IsiRegisterWriteIss(handle, 0x3253, 0x00);
    }
    pIMX219Ctx->TestPattern = enable;

    TRACE(IMX219_INFO, "%s: (exit)\n", __func__);

    return (result);
}

static RESULT IMX219_IsiSensorSetBlcIss(IsiSensorHandle_t handle, sensor_blc_t * pblc)
{
    int32_t ret = 0;
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL || pIMX219Ctx->IsiCtx.HalHandle == NULL) {
        return RET_WRONG_HANDLE;
    }

    if (pblc == NULL)
        return RET_NULL_POINTER;

    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_BLC, pblc);
    if (ret != 0)
    {
         TRACE(IMX219_ERROR, "%s: set wb error\n", __func__);
    }

    return RET_SUCCESS;
}

static RESULT IMX219_IsiSensorSetWBIss(IsiSensorHandle_t handle, sensor_white_balance_t * pwb)
{
    int32_t ret = 0;
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL || pIMX219Ctx->IsiCtx.HalHandle == NULL) {
        return RET_WRONG_HANDLE;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    if (pwb == NULL)
        return RET_NULL_POINTER;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_WB, pwb);
    if (ret != 0)
    {
         TRACE(IMX219_ERROR, "%s: set wb error\n", __func__);
    }

    return RET_SUCCESS;
}

static RESULT IMX219_IsiGetSensorAWBModeIss(IsiSensorHandle_t  handle, IsiSensorAwbMode_t *pawbmode)
{
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL || pIMX219Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    if (pIMX219Ctx->SensorMode.hdr_mode == SENSOR_MODE_HDR_NATIVE){
        *pawbmode = ISI_SENSOR_AWB_MODE_SENSOR;
    }else{
        *pawbmode = ISI_SENSOR_AWB_MODE_NORMAL;
    }
    return RET_SUCCESS;
}

static RESULT IMX219_IsiSensorGetExpandCurveIss(IsiSensorHandle_t handle, sensor_expand_curve_t * pexpand_curve)
{
    int32_t ret = 0;
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;
    if (pIMX219Ctx == NULL || pIMX219Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pIMX219Ctx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_EXPAND_CURVE, pexpand_curve);
    if (ret != 0)
    {
        TRACE(IMX219_ERROR, "%s: get  expand cure error\n", __func__);
        return RET_FAILURE;
    }

    return RET_SUCCESS;
}

static RESULT IMX219_IsiGetCapsIss(IsiSensorHandle_t handle,
                         IsiSensorCaps_t * pIsiSensorCaps)
{
    IMX219_Context_t *pIMX219Ctx = (IMX219_Context_t *) handle;

    RESULT result = RET_SUCCESS;

    TRACE(IMX219_INFO, "%s (enter)\n", __func__);

    if (pIMX219Ctx == NULL) {
        return (RET_WRONG_HANDLE);
    }

    if (pIsiSensorCaps == NULL) {
        return (RET_NULL_POINTER);
    }

    pIsiSensorCaps->BusWidth = pIMX219Ctx->SensorMode.bit_width;
    pIsiSensorCaps->Mode = ISI_MODE_BAYER;
    pIsiSensorCaps->FieldSelection = ISI_FIELDSEL_BOTH;
    pIsiSensorCaps->YCSequence = ISI_YCSEQ_YCBYCR;
    pIsiSensorCaps->Conv422 = ISI_CONV422_NOCOSITED;
    pIsiSensorCaps->BPat = pIMX219Ctx->SensorMode.bayer_pattern;
    pIsiSensorCaps->HPol = ISI_HPOL_REFPOS;
    pIsiSensorCaps->VPol = ISI_VPOL_NEG;
    pIsiSensorCaps->Edge = ISI_EDGE_RISING;
    pIsiSensorCaps->Resolution.width = pIMX219Ctx->SensorMode.width;
    pIsiSensorCaps->Resolution.height = pIMX219Ctx->SensorMode.height;
    pIsiSensorCaps->SmiaMode = ISI_SMIA_OFF;
    pIsiSensorCaps->MipiLanes = ISI_MIPI_2LANES;

    if (pIsiSensorCaps->BusWidth == 10) {
        pIsiSensorCaps->MipiMode      = ISI_MIPI_MODE_RAW_10;
    }else if (pIsiSensorCaps->BusWidth == 12){
        pIsiSensorCaps->MipiMode      = ISI_MIPI_MODE_RAW_12;
    }else{
        pIsiSensorCaps->MipiMode      = ISI_MIPI_OFF;
    }

    TRACE(IMX219_INFO, "%s (exit)\n", __func__);
    return result;
}

RESULT IMX219_IsiGetSensorIss(IsiSensor_t *pIsiSensor)
{
    RESULT result = RET_SUCCESS;
    TRACE( IMX219_INFO, "%s (enter)\n", __func__);

    if ( pIsiSensor != NULL ) {
        pIsiSensor->pszName                         = SensorName;
        pIsiSensor->pIsiCreateSensorIss             = IMX219_IsiCreateSensorIss;

        pIsiSensor->pIsiInitSensorIss               = IMX219_IsiInitSensorIss;
        pIsiSensor->pIsiGetSensorModeIss            = IMX219_IsiGetSensorModeIss;
        pIsiSensor->pIsiResetSensorIss              = IMX219_IsiResetSensorIss;
        pIsiSensor->pIsiReleaseSensorIss            = IMX219_IsiReleaseSensorIss;
        pIsiSensor->pIsiGetCapsIss                  = IMX219_IsiGetCapsIss;
        pIsiSensor->pIsiSetupSensorIss              = IMX219_IsiSetupSensorIss;
        pIsiSensor->pIsiChangeSensorResolutionIss   = IMX219_IsiChangeSensorResolutionIss;
        pIsiSensor->pIsiSensorSetStreamingIss       = IMX219_IsiSensorSetStreamingIss;
        pIsiSensor->pIsiSensorSetPowerIss           = IMX219_IsiSensorSetPowerIss;
        pIsiSensor->pIsiCheckSensorConnectionIss    = IMX219_IsiCheckSensorConnectionIss;
        pIsiSensor->pIsiGetSensorRevisionIss        = IMX219_IsiGetSensorRevisionIss;
        pIsiSensor->pIsiRegisterReadIss             = IMX219_IsiRegisterReadIss;
        pIsiSensor->pIsiRegisterWriteIss            = IMX219_IsiRegisterWriteIss;

        /* AEC functions */
        pIsiSensor->pIsiExposureControlIss          = IMX219_IsiExposureControlIss;
        pIsiSensor->pIsiGetGainLimitsIss            = IMX219_IsiGetGainLimitsIss;
        pIsiSensor->pIsiGetIntegrationTimeLimitsIss = IMX219_IsiGetIntegrationTimeLimitsIss;
        pIsiSensor->pIsiGetCurrentExposureIss       = IMX219_IsiGetCurrentExposureIss;
        pIsiSensor->pIsiGetVSGainIss                    = IMX219_IsiGetVSGainIss;
        pIsiSensor->pIsiGetGainIss                      = IMX219_IsiGetGainIss;
        pIsiSensor->pIsiGetLongGainIss                  = IMX219_IsiGetLongGainIss;
        pIsiSensor->pIsiGetGainIncrementIss             = IMX219_IsiGetGainIncrementIss;
        pIsiSensor->pIsiSetGainIss                      = IMX219_IsiSetGainIss;
        pIsiSensor->pIsiGetIntegrationTimeIss           = IMX219_IsiGetIntegrationTimeIss;
        pIsiSensor->pIsiGetVSIntegrationTimeIss         = IMX219_IsiGetVSIntegrationTimeIss;
        pIsiSensor->pIsiGetLongIntegrationTimeIss       = IMX219_IsiGetLongIntegrationTimeIss;
        pIsiSensor->pIsiGetIntegrationTimeIncrementIss  = IMX219_IsiGetIntegrationTimeIncrementIss;
        pIsiSensor->pIsiSetIntegrationTimeIss           = IMX219_IsiSetIntegrationTimeIss;
        pIsiSensor->pIsiQuerySensorIss                  = IMX219_IsiQuerySensorIss;
        pIsiSensor->pIsiGetResolutionIss                = IMX219_IsiGetResolutionIss;
        pIsiSensor->pIsiGetSensorFpsIss                 = IMX219_IsiGetSensorFpsIss;
        pIsiSensor->pIsiSetSensorFpsIss                 = IMX219_IsiSetSensorFpsIss;
        pIsiSensor->pIsiSensorGetExpandCurveIss         = IMX219_IsiSensorGetExpandCurveIss;

        /* AWB specific functions */

        /* Testpattern */
        pIsiSensor->pIsiActivateTestPattern         = IMX219_IsiActivateTestPattern;
        pIsiSensor->pIsiSetBayerPattern             = IMX219_IsiSetBayerPattern;

        pIsiSensor->pIsiSensorSetBlcIss             = IMX219_IsiSensorSetBlcIss;
        pIsiSensor->pIsiSensorSetWBIss              = IMX219_IsiSensorSetWBIss;
        pIsiSensor->pIsiGetSensorAWBModeIss         = IMX219_IsiGetSensorAWBModeIss;

    } else {
        result = RET_NULL_POINTER;
    }

    TRACE( IMX219_INFO, "%s (exit)\n", __func__);
    return ( result );
}

/*****************************************************************************
* each sensor driver need declare this struct for isi load
*****************************************************************************/
IsiCamDrvConfig_t IMX219_IsiCamDrvConfig = {
    0,
    IMX219_IsiQuerySensorSupportIss,
    IMX219_IsiGetSensorIss,
    {
     SensorName,            /**< IsiSensor_t.pszName */
     0,            /**< IsiSensor_t.pIsiInitIss>*/
     0,            /**< IsiSensor_t.pIsiResetSensorIss>*/
     0,            /**< IsiSensor_t.pRegisterTable */
     0,            /**< IsiSensor_t.pIsiSensorCaps */
     0,            /**< IsiSensor_t.pIsiCreateSensorIss */
     0,            /**< IsiSensor_t.pIsiReleaseSensorIss */
     0,            /**< IsiSensor_t.pIsiGetCapsIss */
     0,            /**< IsiSensor_t.pIsiSetupSensorIss */
     0,            /**< IsiSensor_t.pIsiChangeSensorResolutionIss */
     0,            /**< IsiSensor_t.pIsiSensorSetStreamingIss */
     0,            /**< IsiSensor_t.pIsiSensorSetPowerIss */
     0,            /**< IsiSensor_t.pIsiCheckSensorConnectionIss */
     0,            /**< IsiSensor_t.pIsiGetSensorRevisionIss */
     0,            /**< IsiSensor_t.pIsiRegisterReadIss */
     0,            /**< IsiSensor_t.pIsiRegisterWriteIss */

     0,            /**< IsiSensor_t.pIsiExposureControlIss */
     0,            /**< IsiSensor_t.pIsiGetGainLimitsIss */
     0,            /**< IsiSensor_t.pIsiGetIntegrationTimeLimitsIss */
     0,            /**< IsiSensor_t.pIsiGetCurrentExposureIss */
     0,            /**< IsiSensor_t.pIsiGetGainIss */
     0,            /**< IsiSensor_t.pIsiGetVSGainIss */
     0,            /**< IsiSensor_t.pIsiGetGainIncrementIss */
     0,            /**< IsiSensor_t.pIsiGetGainIncrementIss */
     0,            /**< IsiSensor_t.pIsiSetGainIss */
     0,            /**< IsiSensor_t.pIsiGetIntegrationTimeIss */
     0,            /**< IsiSensor_t.pIsiGetIntegrationTimeIncrementIss */
     0,            /**< IsiSensor_t.pIsiSetIntegrationTimeIss */
     0,            /**< IsiSensor_t.pIsiGetResolutionIss */
     0,            /**< IsiSensor_t.pIsiGetAfpsInfoIss */

     0,            /**< IsiSensor_t.pIsiMdiInitMotoDriveMds */
     0,            /**< IsiSensor_t.pIsiMdiSetupMotoDrive */
     0,            /**< IsiSensor_t.pIsiMdiFocusSet */
     0,            /**< IsiSensor_t.pIsiMdiFocusGet */
     0,            /**< IsiSensor_t.pIsiMdiFocusCalibrate */
     0,            /**< IsiSensor_t.pIsiGetSensorMipiInfoIss */
     0,            /**< IsiSensor_t.pIsiActivateTestPattern */
     0,            /**< IsiSensor_t.pIsiSetBayerPattern */
     }
};
