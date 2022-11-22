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

#include <ebase/types.h>
#include <ebase/trace.h>
#include <ebase/builtins.h>
#include <common/return_codes.h>
#include <common/misc.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <math.h>
#include <isi/isi.h>
#include <isi/isi_iss.h>
#include <isi/isi_priv.h>
#include <vvsensor.h>
#include "IMX334_priv.h"

CREATE_TRACER( IMX334_INFO , "IMX334: ", INFO,    1);
CREATE_TRACER( IMX334_WARN , "IMX334: ", WARNING, 1);
CREATE_TRACER( IMX334_ERROR, "IMX334: ", ERROR,   1);
CREATE_TRACER( IMX334_DEBUG,     "IMX334: ", INFO, 1);
CREATE_TRACER( IMX334_REG_INFO , "IMX334: ", INFO, 1);
CREATE_TRACER( IMX334_REG_DEBUG, "IMX334: ", INFO, 1);

#ifdef SUBDEV_V4L2
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#undef TRACE
#define TRACE(x, ...)
#endif

#define IMX334_MIN_GAIN_STEP    ( 1.0f/16.0f )  /**< min gain step size used by GUI (hardware min = 1/16; 1/16..32/16 depending on actual gain ) */
#define IMX334_MAX_GAIN_AEC     ( 32.0f )       /**< max. gain used by the AEC (arbitrarily chosen, hardware limit = 62.0, driver limit = 32.0 ) */
#define IMX334_PLL_PCLK         74250000
#define IMX334_HMAX             0xaec
#define IMX334_VMAX             0xac4

extern const IsiRegDescription_t IMX334_g_aRegDescription[];
//const IsiSensorCaps_t IMX334_g_IsiSensorDefaultConfig;

/*****************************************************************************
 *Sensor Info
*****************************************************************************/
static const char SensorName[16] = "IMX334";

static struct vvcam_mode_info pIMX334_mode_info[] = {
	{
		.index     = 0,
		.width     = 3864,
		.height    = 2180,
		.fps       = 30,
		.hdr_mode  = SENSOR_MODE_LINEAR,
		.bit_width = 12,
		.bayer_pattern = 3,
        .mipi_phy_freq = 800, //mbps
        .mipi_line_num = 4,
        .preg_data = (void *)"imx334 3864x2180",
	},
    /*
     {
		.index     = 0,
		.width     = 3840,
		.height    = 2160,
		.fps       = 30,
		.hdr_mode  = SENSOR_MODE_LINEAR,
		.bit_width = 12,
		.bayer_pattern = 3,
        .mipi_phy_freq = 900, //mbps
        .mipi_line_num = 4,
        .preg_data = (void *)"imx334 3840x2160",
	},
    */
    /*
	{
		.index     = 1,
		.width     = 3840,
		.height    = 2160,
		.fps      = 30,
		.hdr_mode = SENSOR_MODE_HDR_STITCH,
		.stitching_mode = SENSOR_STITCHING_3DOL,
		.bit_width = 12,
		.bayer_pattern = BAYER_GBRG,
        .mipi_phy_freq = 800, //mbps
        .mipi_line_num = 4,
        .preg_data = (void *)"imx334 3840x2160",
	},
	{
		.index     = 2,
		.width    = 1280,
		.height   = 720,
		.fps      = 60,
		.hdr_mode = SENSOR_MODE_LINEAR,
		.bit_width = 10,
		.bayer_pattern = BAYER_GBRG,
        .mipi_phy_freq = 600, //mbps
        .mipi_line_num = 4,
        .preg_data = (void *)"imx334 1920x1080",
	}
    */
};

/*
static const uint32_t SensorDrvSupportResoluton[] = {
    ISI_RES_TV720P,
    ISI_RES_TV1080P
};
*/

static RESULT IMX334_IsiSensorSetPowerIss(IsiSensorHandle_t handle, bool_t on) {
    RESULT result = RET_SUCCESS;

    int ret = 0;
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    if (pIMX334Ctx == NULL || pIMX334Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;

    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    int32_t enable = on;
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_POWER, &enable);
    if (ret != 0) {
        TRACE(IMX334_ERROR, "%s: sensor set power error!\n", __func__);
        return (RET_FAILURE);
    }

    TRACE(IMX334_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT IMX334_IsiResetSensorIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;
    int ret = 0;

    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    if (pIMX334Ctx == NULL || pIMX334Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_RESET, NULL);
    if (ret != 0) {
        TRACE(IMX334_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }

    sleep(0.01);

    TRACE(IMX334_INFO, "%s (exit)\n", __func__);
    return (result);
}

#ifdef SUBDEV_CHAR
static RESULT IMX334_IsiSensorSetClkIss(IsiSensorHandle_t handle, uint32_t clk) {
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;

    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    if (pIMX334Ctx == NULL || pIMX334Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;

    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_CLK, &clk);
    if (ret != 0) {
        TRACE(IMX334_ERROR, "%s: sensor set clk error!\n", __func__);
        return (RET_FAILURE);
    }

    TRACE(IMX334_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT IMX334_IsiSensorGetClkIss(IsiSensorHandle_t handle, uint32_t * pclk) {
    RESULT result = RET_SUCCESS;
    int ret = 0;

    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    if (pIMX334Ctx == NULL || pIMX334Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;

    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_CLK, pclk);
    if (ret != 0) {
        TRACE(IMX334_ERROR, "%s: sensor get clk error!\n", __func__);
        return (RET_FAILURE);
    }

    TRACE(IMX334_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT IMX334_IsiConfigSensorSCCBIss(IsiSensorHandle_t handle)
{
    RESULT result = RET_SUCCESS;
    int ret = 0;
    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    if (pIMX334Ctx == NULL || pIMX334Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;

    static const IsiSccbInfo_t SensorSccbInfo = {
        .slave_addr = (0x34 >> 1),
        .addr_byte = 2,
        .data_byte = 1,
    };

    struct vvcam_sccb_cfg_s sensor_sccb_config;
    sensor_sccb_config.slave_addr = SensorSccbInfo.slave_addr;
    sensor_sccb_config.addr_byte = SensorSccbInfo.addr_byte;
    sensor_sccb_config.data_byte = SensorSccbInfo.data_byte;

    ret =
        ioctl(pHalCtx->sensor_fd, VVSENSORIOC_SENSOR_SCCB_CFG,
          &sensor_sccb_config);
    if (ret != 0) {
        TRACE(IMX334_ERROR, "%s: sensor config sccb info error!\n",
              __func__);
        return (RET_FAILURE);
    }

    TRACE(IMX334_INFO, "%s (exit) result = %d\n", __func__, result);
    return (result);
}
#endif

static RESULT IMX334_IsiRegisterReadIss (IsiSensorHandle_t handle, const uint32_t address, uint32_t * p_value) {
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    if (pIMX334Ctx == NULL || pIMX334Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;

    struct vvcam_sccb_data sccb_data;
    sccb_data.addr = address;
    sccb_data.data = 0;
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_READ_REG, &sccb_data);
    if (ret != 0) {
        TRACE(IMX334_ERROR, "%s: read sensor register error!\n",
              __func__);
        return (RET_FAILURE);
    }

    *p_value = sccb_data.data;

    TRACE(IMX334_INFO, "%s (exit) result = %d\n", __func__, result);
    return (result);
}

static RESULT IMX334_IsiRegisterWriteIss(IsiSensorHandle_t handle, const uint32_t address, const uint32_t value) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    if (pIMX334Ctx == NULL || pIMX334Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;

    struct vvcam_sccb_data sccb_data;
    sccb_data.addr = address;
    sccb_data.data = value;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_WRITE_REG, &sccb_data);
    if (ret != 0) {
        TRACE(IMX334_ERROR, "%s: write sensor register error!\n",
              __func__);
        return (RET_FAILURE);
    }

    TRACE(IMX334_INFO, "%s (exit) result = %d\n", __func__, result);
    return (result);
}

static RESULT IMX334_IsiQuerySensorSupportIss(HalHandle_t  HalHandle, vvcam_mode_info_array_t *pSensorSupportInfo)
{
    int ret = 0;
    struct vvcam_mode_info_array *psensor_mode_info_arry;

    HalContext_t *pHalCtx = HalHandle;
    if ( pHalCtx == NULL ) {
        return RET_NULL_POINTER;
    }

    psensor_mode_info_arry = pSensorSupportInfo;
    psensor_mode_info_arry->count = sizeof(pIMX334_mode_info) / sizeof(struct vvcam_mode_info);
    memcpy(psensor_mode_info_arry->modes, pIMX334_mode_info, sizeof(pIMX334_mode_info));

    TRACE(IMX334_INFO, "SensorQuery:\n");
    TRACE(IMX334_INFO, "*********************************\n");
    for(int i=0; i<psensor_mode_info_arry->count; i++)
    {
        TRACE( IMX334_INFO, "Current Sensor Mode:\n");
        TRACE( IMX334_INFO, "Mode Index: %d \n",psensor_mode_info_arry->modes[i].index);
        TRACE( IMX334_INFO, "Resolution: %d * %d\n",psensor_mode_info_arry->modes[i].width,psensor_mode_info_arry->modes[i].height);
        TRACE( IMX334_INFO, "fps: %d \n",psensor_mode_info_arry->modes[i].fps);
        TRACE( IMX334_INFO, "hdr_mode: %d \n",psensor_mode_info_arry->modes[i].hdr_mode);
        TRACE( IMX334_INFO, "stitching_mode: %d \n",psensor_mode_info_arry->modes[i].stitching_mode);
        TRACE( IMX334_INFO, "bit_width: %d \n",psensor_mode_info_arry->modes[i].bit_width);
        TRACE( IMX334_INFO, "bayer_pattern: %d \n",psensor_mode_info_arry->modes[i].bayer_pattern);
        TRACE( IMX334_INFO, "---------------------------------\n");
    }
    TRACE(IMX334_INFO, "*********************************\n");
    return RET_SUCCESS;
}

static  RESULT IMX334_IsiQuerySensorIss(IsiSensorHandle_t handle, vvcam_mode_info_array_t *pSensorInfo)
{
    RESULT result = RET_SUCCESS;
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    if (pIMX334Ctx == NULL || pIMX334Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;
    IMX334_IsiQuerySensorSupportIss(pHalCtx,pSensorInfo);

    return result;
}

static RESULT IMX334_IsiGetSensorModeIss(IsiSensorHandle_t handle,void *mode)
{
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    if (pIMX334Ctx == NULL) {
        return (RET_WRONG_HANDLE);
    }
    memcpy(mode,&(pIMX334Ctx->SensorMode), sizeof(pIMX334Ctx->SensorMode));

    return ( RET_SUCCESS );
}

static RESULT IMX334_IsiCreateSensorIss(IsiSensorInstanceConfig_t * pConfig) {
    RESULT result = RET_SUCCESS;
    IMX334_Context_t *pIMX334Ctx;

    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    if (!pConfig || !pConfig->pSensor)
        return (RET_NULL_POINTER);

    pIMX334Ctx = (IMX334_Context_t *) malloc(sizeof(IMX334_Context_t));
    if (!pIMX334Ctx) {
        TRACE(IMX334_ERROR, "%s: Can't allocate IMX334 context\n",
              __func__);
        return (RET_OUTOFMEM);
    }

    MEMSET(pIMX334Ctx, 0, sizeof(IMX334_Context_t));

    result = HalAddRef(pConfig->HalHandle);
    if (result != RET_SUCCESS) {
        free(pIMX334Ctx);
        return (result);
    }

    pIMX334Ctx->IsiCtx.HalHandle = pConfig->HalHandle;
    pIMX334Ctx->IsiCtx.pSensor = pConfig->pSensor;
    pIMX334Ctx->GroupHold = BOOL_FALSE;
    pIMX334Ctx->OldGain = 1.0;
    pIMX334Ctx->OldIntegrationTime = 0.01;
    pIMX334Ctx->Configured = BOOL_FALSE;
    pIMX334Ctx->Streaming = BOOL_FALSE;
    pIMX334Ctx->TestPattern = BOOL_FALSE;
    pIMX334Ctx->isAfpsRun = BOOL_FALSE;
    pIMX334Ctx->SensorMode.index = pConfig->SensorModeIndex;
    pConfig->hSensor = (IsiSensorHandle_t) pIMX334Ctx;
#ifdef SUBDEV_CHAR
    struct vvcam_mode_info *SensorDefaultMode = NULL;
    for (int i=0; i < sizeof(pIMX334_mode_info)/ sizeof(struct vvcam_mode_info); i++)
    {
        if (pIMX334_mode_info[i].index == pIMX334Ctx->SensorMode.index)
        {
            SensorDefaultMode = &(pIMX334_mode_info[i]);
            break;
        }
    }

    if (SensorDefaultMode != NULL)
    {
        strcpy(pIMX334Ctx->SensorRegCfgFile, get_vi_config_path());
        switch(SensorDefaultMode->index)
        {
            case 0:
                strcat(pIMX334Ctx->SensorRegCfgFile,
                    "IMX334_mipi4lane_3864_2180_raw12_800mbps_init.txt");
                break;
                /*
            case 1: //3Dol mode
                strcat(pIMX334Ctx->SensorRegCfgFile,
                    "IMX334_mipi4lane_3840_2160_raw12_800mbps_3dol_init.txt");
                break;
            */
            default:
                break;
        }

        if (access(pIMX334Ctx->SensorRegCfgFile, F_OK) == 0) {
            pIMX334Ctx->KernelDriverFlag = 0;
            memcpy(&(pIMX334Ctx->SensorMode),SensorDefaultMode,sizeof(struct vvcam_mode_info));
        } else {
            TRACE(IMX334_ERROR, "%s, %d, load %s: error\n", __func__, __LINE__, pIMX334Ctx->SensorRegCfgFile);
            return -1;
        }
    }else
    {
        pIMX334Ctx->KernelDriverFlag = 1;
    }

    result = IMX334_IsiSensorSetPowerIss(pIMX334Ctx, BOOL_TRUE);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    uint32_t SensorClkIn;
    if (pIMX334Ctx->KernelDriverFlag) {
        result = IMX334_IsiSensorGetClkIss(pIMX334Ctx, &SensorClkIn);
        RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);
    }

    result = IMX334_IsiSensorSetClkIss(pIMX334Ctx, SensorClkIn);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    result = IMX334_IsiResetSensorIss(pIMX334Ctx);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    if (!pIMX334Ctx->KernelDriverFlag) {
        result = IMX334_IsiConfigSensorSCCBIss(pIMX334Ctx);
        RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);
    }

    pIMX334Ctx->pattern = 3;
#endif

#ifdef SUBDEV_V4L2
    pIMX334Ctx->pattern = 3;
    pIMX334Ctx->subdev = HalGetFdHandle(pConfig->HalHandle, HAL_MODULE_SENSOR);
    pIMX334Ctx->KernelDriverFlag = 1;
#endif
    TRACE(IMX334_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT IMX334_IsiGetRegCfgIss(const char *registerFileName,
                     struct vvcam_sccb_array *arry)
{
    if (NULL == registerFileName) {
        TRACE(IMX334_ERROR, "%s:registerFileName is NULL\n", __func__);
        return (RET_NULL_POINTER);
    }
#ifdef SUBDEV_CHAR
    FILE *fp = NULL;
    fp = fopen(registerFileName, "rb");
    if (!fp) {
        TRACE(IMX334_ERROR, "%s:load register file  %s error!\n",
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
        TRACE(IMX334_ERROR, "%s:malloc failed NULL Point!\n", __func__,
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

#if 0
static RESULT IMX334_IsiGetPicSize(uint32_t Resolution, uint32_t * pwidth,
                   uint32_t * pheight)
{
    switch (Resolution) {
    case ISI_RES_TV1080P:
        {
            *pwidth = 1920;
            *pheight = 1080;
            break;
        }
    case ISI_RES_TV720P:
        {
            *pwidth = 1280;
            *pheight = 720;
            break;
        }
    default:
        {
            return (RET_NOTSUPP);
        }
    }
    return (RET_SUCCESS);
}
#endif

static RESULT IMX334_IsiInitSensorIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;

    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;

    TRACE(IMX334_INFO, "%s (enter)\n", __func__);
    if (pIMX334Ctx == NULL) {
        return (RET_WRONG_HANDLE);
    }
    if (pIMX334Ctx->KernelDriverFlag) {
#ifdef SUBDEV_CHAR

        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_INIT, &(pIMX334Ctx->SensorMode));
        if (ret != 0) {
            TRACE(IMX334_ERROR, "%s:sensor init error!\n",
                  __func__);
            return (RET_FAILURE);
        }

        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_SENSOR_MODE, &(pIMX334Ctx->SensorMode));
        if (ret != 0) {
            TRACE(IMX334_ERROR, "%s:sensor get mode info error!\n",
                  __func__);
            return (RET_FAILURE);
        }

        struct vvcam_ae_info_s ae_info;
        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_AE_INFO, &ae_info);
        if (ret != 0) {
            TRACE(IMX334_ERROR, "%s:sensor get ae info error!\n",
                  __func__);
            return (RET_FAILURE);
        }
        pIMX334Ctx->one_line_exp_time  = IMX334_HMAX/IMX334_PLL_PCLK;
        pIMX334Ctx->MaxIntegrationLine = ae_info.max_integration_time;
        pIMX334Ctx->MinIntegrationLine = ae_info.min_integration_time;
        pIMX334Ctx->gain_accuracy = ae_info.gain_accuracy;
        pIMX334Ctx->AecMinGain =  1.0;
        pIMX334Ctx->AecMaxGain = 36;

        pIMX334Ctx->MaxFps  = pIMX334Ctx->SensorMode.fps;
        pIMX334Ctx->MinFps  = 1;
        pIMX334Ctx->CurrFps = pIMX334Ctx->MaxFps;
#endif

#ifdef SUBDEV_V4L2
        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_SENSOR_MODE, &(pIMX334Ctx->SensorMode));
        if (ret != 0) {
            TRACE(IMX334_ERROR, "%s:sensor get mode info error!\n",
                  __func__);
            return (RET_FAILURE);
        }
        pIMX334Ctx->one_line_exp_time = IMX334_HMAX/IMX334_PLL_PCLK;
        pIMX334Ctx->FrameLengthLines = 0xac4;
        pIMX334Ctx->CurFrameLengthLines = pIMX334Ctx->FrameLengthLines;
        pIMX334Ctx->MaxIntegrationLine = pIMX334Ctx->CurFrameLengthLines - 3;
        pIMX334Ctx->MinIntegrationLine = 1;
        pIMX334Ctx->AecMaxGain = 24;
        pIMX334Ctx->AecMinGain = 3;
        pIMX334Ctx->CurrFps = pIMX334Ctx->MaxFps;
        pIMX334Ctx->gain_accuracy = 1024;

        if (pIMX334Ctx->SensorMode.hdr_mode != SENSOR_MODE_LINEAR)
        {
            pIMX334Ctx->enableHdr = 1;
        }
#endif

    } else {
        struct vvcam_sccb_array arry;
        result = IMX334_IsiGetRegCfgIss(pIMX334Ctx->SensorRegCfgFile, &arry);
        if (result != 0) {
            TRACE(IMX334_ERROR,
                  "%s:IMX334_IsiGetRegCfgIss error!\n", __func__);
            return (RET_FAILURE);
        }

        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_WRITE_ARRAY, &arry);
        if (ret != 0) {
            TRACE(IMX334_ERROR, "%s:Sensor Write Reg arry error!\n",
                  __func__);
            return (RET_FAILURE);
        }

        switch(pIMX334Ctx->SensorMode.index)
        {
            case 0:
                pIMX334Ctx->one_line_exp_time = IMX334_HMAX/IMX334_PLL_PCLK;
                pIMX334Ctx->FrameLengthLines = 0xac4;
                pIMX334Ctx->CurFrameLengthLines = pIMX334Ctx->FrameLengthLines;
                pIMX334Ctx->MaxIntegrationLine = pIMX334Ctx->CurFrameLengthLines - 3;
                pIMX334Ctx->MinIntegrationLine = 1;
                pIMX334Ctx->AecMaxGain = 24;
                pIMX334Ctx->AecMinGain = 3;
                break;
            case 1:
                pIMX334Ctx->one_line_exp_time = IMX334_HMAX/IMX334_PLL_PCLK;
                pIMX334Ctx->FrameLengthLines =  0xac4;
                pIMX334Ctx->CurFrameLengthLines = pIMX334Ctx->FrameLengthLines;
                pIMX334Ctx->MaxIntegrationLine = pIMX334Ctx->CurFrameLengthLines - 3;
                pIMX334Ctx->MinIntegrationLine = 1;
                pIMX334Ctx->AecMaxGain = 21;
                pIMX334Ctx->AecMinGain = 3;
                break;
            default:
                return ( RET_NOTAVAILABLE );
                break;
        }
        pIMX334Ctx->MaxFps  = pIMX334Ctx->SensorMode.fps;
        pIMX334Ctx->MinFps  = 1;
        pIMX334Ctx->CurrFps = pIMX334Ctx->MaxFps;
    }


    TRACE(IMX334_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT IMX334_IsiReleaseSensorIss(IsiSensorHandle_t handle) {
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    if (pIMX334Ctx == NULL)
        return (RET_WRONG_HANDLE);

    (void)IMX334_IsiSensorSetStreamingIss(pIMX334Ctx, BOOL_FALSE);
    (void)IMX334_IsiSensorSetPowerIss(pIMX334Ctx, BOOL_FALSE);
    (void)HalDelRef(pIMX334Ctx->IsiCtx.HalHandle);

    MEMSET(pIMX334Ctx, 0, sizeof(IMX334_Context_t));
    free(pIMX334Ctx);
    TRACE(IMX334_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT IMX334_IsiGetCapsIss
    (IsiSensorHandle_t handle, IsiSensorCaps_t * pIsiSensorCaps) {
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;

    RESULT result = RET_SUCCESS;

    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    if (pIMX334Ctx == NULL)
        return (RET_WRONG_HANDLE);

    if (pIsiSensorCaps == NULL) {
        return (RET_NULL_POINTER);
    } else {
        pIsiSensorCaps->BusWidth = 12;//ISI_BUSWIDTH_12BIT;
        pIsiSensorCaps->Mode = ISI_MODE_BAYER;
        pIsiSensorCaps->FieldSelection = ISI_FIELDSEL_BOTH;
        pIsiSensorCaps->YCSequence = ISI_YCSEQ_YCBYCR;    //  ISI_YCSEQ_YCRYCB;//ISI_YCSEQ_YCBYCR;           /**< only Bayer supported, will not be evaluated */
        pIsiSensorCaps->Conv422 = ISI_CONV422_NOCOSITED;    //ISI_CONV422_INTER;//ISI_CONV422_NOCOSITED;
        pIsiSensorCaps->BPat = pIMX334Ctx->pattern;
        pIsiSensorCaps->HPol = ISI_HPOL_REFPOS;
        pIsiSensorCaps->VPol = ISI_VPOL_NEG;    //ISI_VPOL_POS;//ISI_VPOL_NEG;
        pIsiSensorCaps->Edge = ISI_EDGE_RISING;    //ISI_EDGE_FALLING;//ISI_EDGE_RISING;
        //pIsiSensorCaps->Bls = ISI_BLS_OFF;
        //pIsiSensorCaps->Gamma = ISI_GAMMA_OFF;
        //pIsiSensorCaps->CConv = ISI_CCONV_OFF;

        pIsiSensorCaps->Resolution = pIMX334Ctx->Resolution;    //( ISI_RES_TV1080P24   | ISI_RES_TV1080P20 | ISI_RES_TV1080P15 | ISI_RES_TV1080P6

        //pIsiSensorCaps->BLC = (ISI_BLC_AUTO | ISI_BLC_OFF);
        //pIsiSensorCaps->AGC = ISI_AGC_OFF;    //  ( ISI_AGC_AUTO | ISI_AGC_OFF );
        //pIsiSensorCaps->AWB = ISI_AWB_OFF;    // ( ISI_AWB_AUTO | ISI_AWB_OFF );
        //pIsiSensorCaps->AEC = (ISI_AEC_OFF);    //  ISI_AEC_AUTO | ISI_AEC_OFF );
        //pIsiSensorCaps->DPCC = (ISI_DPCC_AUTO | ISI_DPCC_OFF);

        //pIsiSensorCaps->DwnSz = ISI_DWNSZ_SUBSMPL;
        /*
        pIsiSensorCaps->CieProfile = (ISI_CIEPROF_A
                          | ISI_CIEPROF_D50
                          | ISI_CIEPROF_D65
                          | ISI_CIEPROF_D75
                          | ISI_CIEPROF_F2
                          | ISI_CIEPROF_F11);
                          */
        pIsiSensorCaps->SmiaMode = ISI_SMIA_OFF;
        pIsiSensorCaps->MipiMode = ISI_MIPI_OFF;    //= ISI_MIPI_MODE_RAW_10;
        pIsiSensorCaps->MipiLanes = ISI_MIPI_4LANES;
        //pIsiSensorCaps->AfpsResolutions = ISI_AFPS_NOTSUPP;    //(ISI_RES_TV1080P15); //ISI_AFPS_NOTSUPP; //( ISI_RES_TV1080P24 | ISI_RES_TV1080P20 | ISI_RES_TV1080P15  | ISI_RES_TV1080P6);
        //pIsiSensorCaps->enableHdr = pIMX334Ctx->enableHdr;
    }

    TRACE(IMX334_INFO, "%s (exit)\n", __func__);
    return (result);
}
/*
const IsiSensorCaps_t IMX334_g_IsiSensorDefaultConfig = {
    ISI_BUSWIDTH_12BIT,    // BusWidth
    ISI_MODE_BAYER,        // Mode
    ISI_FIELDSEL_BOTH,    // FieldSel
    ISI_YCSEQ_YCBYCR,    //ISI_YCSEQ_YCRYCB,  //ISI_YCSEQ_YCBYCR,           // YCSeq
    ISI_CONV422_NOCOSITED,    //ISI_CONV422_INTER, //ISI_CONV422_NOCOSITED,      // Conv422
    ISI_BPAT_BGBGGRGR,
    ISI_HPOL_REFPOS,    // HPol
    ISI_VPOL_NEG,        //ISI_VPOL_POS,      //ISI_VPOL_NEG,               // VPol
    ISI_EDGE_RISING,    //ISI_EDGE_FALLING,  //ISI_EDGE_RISING,            // Edge
    ISI_BLS_OFF,        // Bls
    ISI_GAMMA_OFF,        // Gamma
    ISI_CCONV_OFF,        // CConv
    ISI_RES_TV1080P,    //ISI_RES_TV1080P24,          // Res
    ISI_DWNSZ_SUBSMPL,    // DwnSz
    ISI_BLC_AUTO,        // BLC
    ISI_AGC_OFF,        //ISI_AGC_OFF,                // AGC
    ISI_AWB_OFF,        //ISI_AWB_OFF,                // AWB
    ISI_AEC_OFF,        //ISI_AEC_OFF,                // AEC
    ISI_DPCC_OFF,        // DPCC
    ISI_CIEPROF_F11,    // CieProfile, this is also used as start profile for AWB (if not altered by menu settings)
    ISI_SMIA_OFF,        // SmiaMode
    ISI_MIPI_OFF,        //ISI_MIPI_MODE_RAW_10,       // MipiMode
    ISI_MIPI_4LANES,
    ISI_AFPS_NOTSUPP,    //ISI_RES_TV1080P15, //   ISI_AFPS_NOTSUPP(ISI_AFPS_NOTSUPP | ISI_RES_TV1080P6)
    //( ISI_AFPS_NOTSUPP | ISI_RES_TV1080P24 | ISI_RES_TV1080P20 | ISI_RES_TV1080P15 ) // AfpsResolutions
    0,
};
*/

static RESULT IMX334_AecSetModeParameters
    (IMX334_Context_t * pIMX334Ctx, const IsiSensorConfig_t * pConfig) {
    RESULT result = RET_SUCCESS;
    TRACE(IMX334_INFO, "%s%s: (enter)\n", __func__,
          pIMX334Ctx->isAfpsRun ? "(AFPS)" : "");

    pIMX334Ctx->AecIntegrationTimeIncrement = pIMX334Ctx->one_line_exp_time;
    pIMX334Ctx->AecMinIntegrationTime = 0.001;
    pIMX334Ctx->AecMaxIntegrationTime = 0.033;

    TRACE(IMX334_DEBUG, "%s%s: AecMaxIntegrationTime = %f \n", __func__,
          pIMX334Ctx->isAfpsRun ? "(AFPS)" : "",
          pIMX334Ctx->AecMaxIntegrationTime);

    pIMX334Ctx->AecGainIncrement = IMX334_MIN_GAIN_STEP;

    //reflects the state of the sensor registers, must equal default settings
    pIMX334Ctx->AecCurGain = pIMX334Ctx->AecMinGain;
    pIMX334Ctx->AecCurIntegrationTime = 0.0f;
    pIMX334Ctx->OldGain = 0;
    pIMX334Ctx->OldIntegrationTime = 0;

    TRACE(IMX334_INFO, "%s%s: (exit)\n", __func__,
          pIMX334Ctx->isAfpsRun ? "(AFPS)" : "");

    return (result);
}

#ifdef SUBDEV_V4L2
RESULT IMX334_Private_SetFormat(IsiSensorHandle_t handle, int width, int height, bool hdrEnable)
{
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    TRACE(IMX334_INFO, "%s: %d %d %d\n", __func__, width, height, hdrEnable);

    ioctl(pIMX334Ctx->subdev, VVSENSORIOC_S_HDR_MODE, &hdrEnable);

    struct v4l2_subdev_format format;
    format.format.width = width;
    format.format.height = height;
    format.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    format.pad = 0;
    int rc = ioctl(pIMX334Ctx->subdev, VIDIOC_SUBDEV_S_FMT, &format);
    return rc == 0 ? RET_SUCCESS : RET_FAILURE;
}
#endif

static RESULT IMX334_IsiSetupSensorIss
    (IsiSensorHandle_t handle, const IsiSensorConfig_t * pConfig) {
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    if (!pIMX334Ctx) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pConfig) {
        TRACE(IMX334_ERROR,
              "%s: Invalid configuration (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    if (pIMX334Ctx->Streaming != BOOL_FALSE) {
        return RET_WRONG_STATE;
    }

    memcpy(&pIMX334Ctx->Config, pConfig, sizeof(IsiSensorConfig_t));

    /* 1.) SW reset of image sensor (via I2C register interface)  be careful, bits 6..0 are reserved, reset bit is not sticky */
    TRACE(IMX334_DEBUG, "%s: IMX334 System-Reset executed\n", __func__);
    osSleep(100);

    result = IMX334_AecSetModeParameters(pIMX334Ctx, pConfig);
    if (result != RET_SUCCESS) {
        TRACE(IMX334_ERROR, "%s: SetupOutputWindow failed.\n",
              __func__);
        return (result);
    }
#ifdef SUBDEV_V4L2
     IMX334_Private_SetFormat(pIMX334Ctx,
                            pIMX334Ctx->SensorMode.width,
                            pIMX334Ctx->SensorMode.height,
                            pIMX334Ctx->SensorMode.hdr_mode);
#endif

    pIMX334Ctx->Configured = BOOL_TRUE;
    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);
    return 0;
}
/*
static RESULT IMX334_IsiGetSupportResolutionIss
    (IsiSensorHandle_t handle, IsiResolutionArry_t * pResolutinArry) {
    RESULT result = RET_SUCCESS;

    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    if (handle == NULL) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    pResolutinArry->count =
        sizeof(SensorDrvSupportResoluton) / sizeof(uint32_t);
    memcpy(pResolutinArry->ResolutionArry, SensorDrvSupportResoluton,
           sizeof(SensorDrvSupportResoluton));
    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);

    return (result);
}

static RESULT IMX334_IsiChangeSensorResolutionIss
    (IsiSensorHandle_t handle,
     uint32_t Resolution, uint8_t * pNumberOfFramesToSkip) {
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    float hdr_ratio[2] = { 1.0f, 1.0f };

    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    if (!pIMX334Ctx)
        return (RET_WRONG_HANDLE);

    if (!pNumberOfFramesToSkip)
        return (RET_NULL_POINTER);

    if ((pIMX334Ctx->Configured != BOOL_TRUE)
        || (pIMX334Ctx->Streaming != BOOL_FALSE))
        return RET_WRONG_STATE;

    IsiSensorCaps_t Caps;
    result = IMX334_IsiGetCapsIss(handle, &Caps);
    if (RET_SUCCESS != result)
        return result;

    if ((Resolution & Caps.Resolution) == 0)
        return RET_OUTOFRANGE;

    if (Resolution == pIMX334Ctx->Config.Resolution) {
        // well, no need to worry
        *pNumberOfFramesToSkip = 0;
    } else {
        // change resolution
        char *szResName = NULL;
        result = IsiGetResolutionName(Resolution, &szResName);
        TRACE(IMX334_DEBUG, "%s: NewRes=0x%08x (%s)\n", __func__,
              Resolution, szResName);

        // update resolution in copy of config in context
        pIMX334Ctx->Config.Resolution = Resolution;

        // tell sensor about that

        // remember old exposure values
        float OldGain = pIMX334Ctx->AecCurGain;
        float OldIntegrationTime = pIMX334Ctx->AecCurIntegrationTime;

        // update limits & stuff (reset current & old settings)
        result =
            IMX334_AecSetModeParameters(pIMX334Ctx,
                        &pIMX334Ctx->Config);
        if (result != RET_SUCCESS) {
            TRACE(IMX334_ERROR,
                  "%s: AecSetModeParameters failed.\n", __func__);
            return (result);
        }
        // restore old exposure values (at least within new exposure values' limits)
        uint8_t NumberOfFramesToSkip;
        float DummySetGain;
        float DummySetIntegrationTime;
        result = IMX334_IsiExposureControlIss(handle, OldGain,
                         OldIntegrationTime,
                         &NumberOfFramesToSkip,
                         &DummySetGain,
                         &DummySetIntegrationTime,
                         hdr_ratio);
        if (result != RET_SUCCESS) {
            TRACE(IMX334_ERROR,
                  "%s: IMX334_IsiExposureControlIss failed.\n",
                  __func__);
            return (result);
        }
        // return number of frames that aren't exposed correctly
        *pNumberOfFramesToSkip = NumberOfFramesToSkip + 1;
    }

    TRACE(IMX334_INFO, "%s (exit)\n", __func__);
    return (result);
}
*/

static RESULT IMX334_IsiChangeSensorResolutionIss(IsiSensorHandle_t handle, uint16_t width, uint16_t height)
{
    RESULT result = RET_SUCCESS;

    return (result);
#if 0
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    float hdr_ratio[2] = { 1.0f, 1.0f };

    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    if (!pIMX334Ctx)
        return (RET_WRONG_HANDLE);

    if ((pIMX334Ctx->Configured != BOOL_TRUE)
        || (pIMX334Ctx->Streaming != BOOL_FALSE))
        return RET_WRONG_STATE;

    IsiSensorCaps_t Caps;
    result = IMX334_IsiGetCapsIss(handle, &Caps);
    if (RET_SUCCESS != result)
        return result;

    if (Caps.Resolution.width != width || Caps.Resolution.height != height)
        return RET_OUTOFRANGE;


    if (width == pIMX334Ctx->Config.Resolution.width
        && height == pIMX334Ctx->Config.Resolution.height) {
        // well, no need to worry
    } else {
        // change resolution
        char *szResName = NULL;
        result = IsiGetResolutionName(Resolution, &szResName);
        TRACE(IMX334_DEBUG, "%s: NewRes=0x%08x (%s)\n", __func__,
              Resolution, szResName);

        // update resolution in copy of config in context
        pIMX334Ctx->Config.Resolution = Resolution;

        // tell sensor about that

        // remember old exposure values
        float OldGain = pIMX334Ctx->AecCurGain;
        float OldIntegrationTime = pIMX334Ctx->AecCurIntegrationTime;

        // update limits & stuff (reset current & old settings)
        result =
            IMX334_AecSetModeParameters(pIMX334Ctx,
                        &pIMX334Ctx->Config);
        if (result != RET_SUCCESS) {
            TRACE(IMX334_ERROR,
                  "%s: AecSetModeParameters failed.\n", __func__);
            return (result);
        }
        // restore old exposure values (at least within new exposure values' limits)
        uint8_t NumberOfFramesToSkip;
        float DummySetGain;
        float DummySetIntegrationTime;
        result = IMX334_IsiExposureControlIss(handle, OldGain,
                         OldIntegrationTime,
                         &NumberOfFramesToSkip,
                         &DummySetGain,
                         &DummySetIntegrationTime,
                         hdr_ratio);
        if (result != RET_SUCCESS) {
            TRACE(IMX334_ERROR,
                  "%s: IMX334_IsiExposureControlIss failed.\n",
                  __func__);
            return (result);
        }
        // return number of frames that aren't exposed correctly
        *pNumberOfFramesToSkip = NumberOfFramesToSkip + 1;
    }

    TRACE(IMX334_INFO, "%s (exit)\n", __func__);
    return (result);
#endif
}

static RESULT IMX334_IsiSensorSetStreamingIss
    (IsiSensorHandle_t handle, bool_t on) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    if (pIMX334Ctx == NULL || pIMX334Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;

    if (pIMX334Ctx->Configured != BOOL_TRUE)
        return RET_WRONG_STATE;

    if (on) {
        ret |= IMX334_IsiRegisterWriteIss(handle, 0x3000, 0x00);
        ret |= IMX334_IsiRegisterWriteIss(handle, 0x3002, 0x00);
    } else {
        ret |= IMX334_IsiRegisterWriteIss(handle, 0x3000, 0x01);
        ret |= IMX334_IsiRegisterWriteIss(handle, 0x3002, 0x01);
    }

    if (ret != 0) {
        return (RET_FAILURE);
    }

    pIMX334Ctx->Streaming = on;

    TRACE(IMX334_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT IMX334_IsiCheckSensorConnectionIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    uint32_t correct_id = 0x9012;
    uint32_t sensor_id = 0;

    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    if (pIMX334Ctx == NULL || pIMX334Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;

    if (pIMX334Ctx->KernelDriverFlag) {
        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_RESERVE_ID,
              &correct_id);
        if (ret != 0) {
            TRACE(IMX334_ERROR,
                  "%s: Read Sensor correct ID Error! \n", __func__);
            return (RET_FAILURE);
        }

        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_CHIP_ID,
              &sensor_id);
        if (ret != 0) {
            TRACE(IMX334_ERROR,
                  "%s: Read Sensor chip ID Error! \n", __func__);
            return (RET_FAILURE);
        }
    } else {
        result = IMX334_IsiGetSensorRevisionIss(handle, &sensor_id);
        if (result != RET_SUCCESS) {
            TRACE(IMX334_ERROR, "%s: Read Sensor ID Error! \n",
                  __func__);
            return (RET_FAILURE);
        }
    }

    if (correct_id != sensor_id) {
        TRACE(IMX334_ERROR, "%s:ChipID =0x%x sensor_id=%x error! \n",
              __func__, correct_id, sensor_id);
        return (RET_FAILURE);
    }

    TRACE(IMX334_INFO,
          "%s ChipID = 0x%08x, sensor_id = 0x%08x, success! \n", __func__,
          correct_id, sensor_id);
    TRACE(IMX334_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT IMX334_IsiGetSensorRevisionIss
    (IsiSensorHandle_t handle, uint32_t * p_value) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    uint32_t reg_val;
    uint32_t sensor_id;

    TRACE(IMX334_INFO, "%s (enter)\n", __func__);

    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    if (pIMX334Ctx == NULL || pIMX334Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;

    if (!p_value)
        return (RET_NULL_POINTER);

    if (pIMX334Ctx->KernelDriverFlag) {
        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_CHIP_ID,
              &sensor_id);
        if (ret != 0) {
            TRACE(IMX334_ERROR, "%s: Read Sensor ID Error! \n",
                  __func__);
            return (RET_FAILURE);
        }
    } else {
        reg_val = 0;
        result = IMX334_IsiRegisterReadIss(handle, 0x3a04, &reg_val);
        sensor_id = (reg_val & 0xff) << 8;

        reg_val = 0;
        result |= IMX334_IsiRegisterReadIss(handle, 0x3a05, &reg_val);
        sensor_id |= (reg_val & 0xff);

    }

    *p_value = sensor_id;
    TRACE(IMX334_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT IMX334_IsiGetGainLimitsIss
    (IsiSensorHandle_t handle, float *pMinGain, float *pMaxGain) {
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    if (pIMX334Ctx == NULL) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pMinGain == NULL) || (pMaxGain == NULL)) {
        TRACE(IMX334_ERROR, "%s: NULL pointer received!!\n");
        return (RET_NULL_POINTER);
    }

    *pMinGain = pIMX334Ctx->AecMinGain;
    *pMaxGain = pIMX334Ctx->AecMaxGain;

    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);
    return (result);
}

static RESULT IMX334_IsiGetIntegrationTimeLimitsIss
    (IsiSensorHandle_t handle,
     float *pMinIntegrationTime, float *pMaxIntegrationTime) {
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);
    if (pIMX334Ctx == NULL) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pMinIntegrationTime == NULL) || (pMaxIntegrationTime == NULL)) {
        TRACE(IMX334_ERROR, "%s: NULL pointer received!!\n");
        return (RET_NULL_POINTER);
    }

    *pMinIntegrationTime = pIMX334Ctx->AecMinIntegrationTime;
    *pMaxIntegrationTime = pIMX334Ctx->AecMaxIntegrationTime;

    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);
    return (result);
}

RESULT IMX334_IsiGetGainIss(IsiSensorHandle_t handle, float *pSetGain) {
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    if (pIMX334Ctx == NULL) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pSetGain == NULL) {
        return (RET_NULL_POINTER);
    }

    *pSetGain = pIMX334Ctx->AecCurGain;

    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX334_IsiGetSEF1GainIss(IsiSensorHandle_t handle, float *pSetGain) {
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    if (pIMX334Ctx == NULL) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pSetGain == NULL) {
        return (RET_NULL_POINTER);
    }

    *pSetGain = pIMX334Ctx->AecCurGainSEF1;

    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);

    return (result);
}

RESULT IMX334_IsiGetGainIncrementIss(IsiSensorHandle_t handle, float *pIncr) {
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    if (pIMX334Ctx == NULL) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pIncr == NULL)
        return (RET_NULL_POINTER);

    *pIncr = pIMX334Ctx->AecGainIncrement;

    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);

    return (result);
}

RESULT IMX334_IsiSetGainIss
    (IsiSensorHandle_t handle,
     float NewGain, float *pSetGain, float *hdr_ratio) {

    RESULT result = RET_SUCCESS;
    return result;
    int32_t ret = 0;

    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    if (pIMX334Ctx == NULL || pIMX334Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;

    if (pIMX334Ctx->KernelDriverFlag) {
        uint32_t SensorGain = 0;
        SensorGain = NewGain * pIMX334Ctx->gain_accuracy;

    #ifdef SUBDEV_CHAR
        if (pIMX334Ctx->enableHdr == true) {
            uint32_t SensorHdrRatio = (uint32_t)*hdr_ratio;
            ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_HDR_RADIO, &SensorHdrRatio);
        }
    #endif
        ret |= ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_GAIN, &SensorGain);
        if (ret != 0) {
            TRACE(IMX334_ERROR, "%s: set sensor gain error\n",
                  __func__);
            return RET_FAILURE;
        }
    } else {
		uint32_t Gain = 0;
		Gain = (uint32_t)(20*log10(NewGain)*(10/3)) ;
                result = IMX334_IsiRegisterWriteIss(handle, 0x3001, 0x01);
                result =IMX334_IsiRegisterWriteIss(handle, 0x30e8,(Gain & 0x00ff));
                result =IMX334_IsiRegisterWriteIss(handle, 0x30e9,(Gain & 0x0700)>>8);
                result = IMX334_IsiRegisterWriteIss(handle, 0x3001, 0x00);
		pIMX334Ctx->OldGain = NewGain;
        }


    pIMX334Ctx->AecCurGain = ((float)(NewGain));

    *pSetGain = pIMX334Ctx->AecCurGain;
    TRACE(IMX334_DEBUG, "%s: g=%f\n", __func__, *pSetGain);
    return (result);
}
RESULT IMX334_IsiSetSEF1GainIss(IsiSensorHandle_t handle,float NewIntegrationTime,float NewGain, float *pSetGain, float *hdr_ratio) {

    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;

    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    if (!pIMX334Ctx) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetGain || !hdr_ratio)
        return (RET_NULL_POINTER);

    if (pIMX334Ctx->KernelDriverFlag) {
        uint32_t SensorGain = 0;
        SensorGain = NewGain * pIMX334Ctx->gain_accuracy;
        ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_VSGAIN, &SensorGain);
    } else {
	    uint32_t Gain = 0;
            Gain = (uint32_t)(20*log10(NewGain)*(10/3)) ;
            result = IMX334_IsiRegisterWriteIss(handle, 0x3001, 0x01);
            result =IMX334_IsiRegisterWriteIss(handle, 0x30EA, (Gain & 0x00FF));
            result =IMX334_IsiRegisterWriteIss(handle, 0x30EB, (Gain & 0x0700)>>8);
	    result = IMX334_IsiRegisterWriteIss(handle, 0x3001, 0x00);
            pIMX334Ctx->OldGainSEF1 = NewGain;
    }

    pIMX334Ctx->AecCurGainSEF1 = NewGain;
    *pSetGain = pIMX334Ctx->AecCurGainSEF1;

    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX334_IsiSetBayerPattern(IsiSensorHandle_t handle, uint8_t pattern)
{

    RESULT result = RET_SUCCESS;
    uint8_t h_shift = 0, v_shift = 0;
    uint32_t val_h = 0, val_l = 0;
    uint16_t val = 0;
    uint8_t Start_p = 0;

    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    if (pIMX334Ctx == NULL || pIMX334Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    // pattern 0:B 1:GB 2:GR 3:R
    result = IMX334_IsiSensorSetStreamingIss(handle, 0);
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

    IMX334_IsiRegisterReadIss(handle, 0x30a0, &val_h);
    IMX334_IsiRegisterReadIss(handle, 0x30a1, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + h_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    IMX334_IsiRegisterWriteIss(handle, 0x30a0, (uint8_t)val_h);
    IMX334_IsiRegisterWriteIss(handle, 0x30a1, (uint8_t)val_l);

    IMX334_IsiRegisterReadIss(handle, 0x30a2, &val_h);
    IMX334_IsiRegisterReadIss(handle, 0x30a3, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + v_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    IMX334_IsiRegisterWriteIss(handle, 0x30a2, (uint8_t)val_h);
    IMX334_IsiRegisterWriteIss(handle, 0x30a3, (uint8_t)val_l);

    IMX334_IsiRegisterReadIss(handle, 0x30a4, &val_h);
    IMX334_IsiRegisterReadIss(handle, 0x30a5, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + h_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    IMX334_IsiRegisterWriteIss(handle, 0x30a4, (uint8_t)val_h);
    IMX334_IsiRegisterWriteIss(handle, 0x30a5, (uint8_t)val_l);

    IMX334_IsiRegisterReadIss(handle, 0x30a6, &val_h);
    IMX334_IsiRegisterReadIss(handle, 0x30a7, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + v_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    IMX334_IsiRegisterWriteIss(handle, 0x30a6, (uint8_t)val_h);
    IMX334_IsiRegisterWriteIss(handle, 0x30a7, (uint8_t)val_l);

    pIMX334Ctx->pattern = 3;
    result = IMX334_IsiSensorSetStreamingIss(handle, pIMX334Ctx->Streaming);

    return (result);
}

RESULT IMX334_IsiGetIntegrationTimeIss
    (IsiSensorHandle_t handle, float *pSetIntegrationTime)
{
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    if (!pIMX334Ctx) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetIntegrationTime)
        return (RET_NULL_POINTER);
    *pSetIntegrationTime = pIMX334Ctx->AecCurIntegrationTime;
    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX334_IsiGetSEF1IntegrationTimeIss
    (IsiSensorHandle_t handle, float *pSetIntegrationTime)
{
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    if (!pIMX334Ctx) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    if (!pSetIntegrationTime)
        return (RET_NULL_POINTER);

    *pSetIntegrationTime = pIMX334Ctx->AecCurIntegrationTimeSEF1;
    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX334_IsiGetIntegrationTimeIncrementIss
    (IsiSensorHandle_t handle, float *pIncr)
{
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    if (!pIMX334Ctx) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pIncr)
        return (RET_NULL_POINTER);

    //_smallest_ increment the sensor/driver can handle (e.g. used for sliders in the application)
    *pIncr = pIMX334Ctx->AecIntegrationTimeIncrement;
    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX334_IsiSetIntegrationTimeIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float *pSetIntegrationTime,
     uint8_t * pNumberOfFramesToSkip, float *hdr_ratio)
{
    RESULT result = RET_SUCCESS;

    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;

    uint32_t exp = 0;

    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    if (!pIMX334Ctx) {
        TRACE(IMX334_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetIntegrationTime || !pNumberOfFramesToSkip) {
        TRACE(IMX334_ERROR, "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    exp = NewIntegrationTime / pIMX334Ctx->one_line_exp_time;

    TRACE(IMX334_DEBUG, "%s: set AEC_PK_EXPO=0x%05x\n", __func__, exp);

    if (NewIntegrationTime != pIMX334Ctx->OldIntegrationTime) {
        if (pIMX334Ctx->KernelDriverFlag) {
            ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_EXP, &exp);
        } else {

            exp = 2200 - exp +1;
            exp = exp > 5?exp:5;
            exp = exp <IMX334_VMAX - 1 ? exp:IMX334_VMAX - 1;
            result = IMX334_IsiRegisterWriteIss(handle, 0x3001, 0x01);
            result = IMX334_IsiRegisterWriteIss(handle, 0x3058, (exp & 0x0000FF));
            result = IMX334_IsiRegisterWriteIss(handle, 0x3059, (exp & 0x00FF00)>>8);
            result = IMX334_IsiRegisterWriteIss(handle, 0x305a, (exp & 0x070000)>>16);
            result = IMX334_IsiRegisterWriteIss(handle, 0x3001, 0x00);
        }

        pIMX334Ctx->OldIntegrationTime = NewIntegrationTime ;
        pIMX334Ctx->AecCurIntegrationTime = NewIntegrationTime;

        *pNumberOfFramesToSkip = 1U;
    } else {
        *pNumberOfFramesToSkip = 0U;    //no frame skip
    }

    *pSetIntegrationTime = pIMX334Ctx->AecCurIntegrationTime;
    TRACE(IMX334_DEBUG, "%s: Ti=%f\n", __func__, *pSetIntegrationTime);
    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX334_IsiSetSEF1IntegrationTimeIss(IsiSensorHandle_t handle, float NewIntegrationTime,float *pSetIntegrationTimeSEF1, uint8_t * pNumberOfFramesToSkip, float *hdr_ratio)
{
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;
    RESULT result = RET_SUCCESS;
    uint32_t exp = 0;

    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    if (!pIMX334Ctx) {
        TRACE(IMX334_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetIntegrationTimeSEF1 || !pNumberOfFramesToSkip) {
        TRACE(IMX334_ERROR,"%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }
    TRACE(IMX334_INFO,"%s:  maxIntegrationTime-=%f minIntegrationTime = %f\n", __func__,
          pIMX334Ctx->AecMaxIntegrationTime, pIMX334Ctx->AecMinIntegrationTime);


    exp = NewIntegrationTime / pIMX334Ctx->one_line_exp_time;

    if (NewIntegrationTime != pIMX334Ctx->OldIntegrationTimeSEF1) {
        if (pIMX334Ctx->KernelDriverFlag) {
            ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_VSEXP, &exp);
        } else {
            exp = 2200- exp +1;
	    exp = exp > 5 ? exp : 5;
            exp = exp < IMX334_VMAX - 1 ? exp : IMX334_VMAX - 1;
	    result = IMX334_IsiRegisterWriteIss(handle, 0x3001, 0x01);
	    result = IMX334_IsiRegisterWriteIss(handle, 0x305c,(exp & 0x0000ff));
            result = IMX334_IsiRegisterWriteIss(handle, 0x305D,(exp & 0x00ff00)>>8);
            result = IMX334_IsiRegisterWriteIss(handle, 0x305e,(exp & 0x070000)>>16);
	    result = IMX334_IsiRegisterWriteIss(handle, 0x3001, 0x00);
        }

        pIMX334Ctx->OldIntegrationTimeSEF1 = NewIntegrationTime;
        pIMX334Ctx->AecCurIntegrationTimeSEF1 = NewIntegrationTime;
        *pNumberOfFramesToSkip = 1U;
    } else {
        *pNumberOfFramesToSkip = 0U;
    }

    *pSetIntegrationTimeSEF1 = pIMX334Ctx->AecCurIntegrationTimeSEF1;

    TRACE(IMX334_DEBUG, "%s: NewIntegrationTime=%f\n", __func__,
          NewIntegrationTime);
    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX334_IsiExposureControlIss(IsiSensorHandle_t handle,float NewGain,float NewIntegrationTime,
 				    uint8_t * pNumberOfFramesToSkip,float *pSetGain, float *pSetIntegrationTime, float *hdr_ratio)
{
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    return result;
    if (pIMX334Ctx == NULL) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pNumberOfFramesToSkip == NULL) || (pSetGain == NULL)
        || (pSetIntegrationTime == NULL)) {
        TRACE(IMX334_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }


    if (pIMX334Ctx->enableHdr)
    {
        result = IMX334_IsiSetSEF1IntegrationTimeIss(handle, NewIntegrationTime,pSetIntegrationTime,pNumberOfFramesToSkip,hdr_ratio);
        result = IMX334_IsiSetSEF1GainIss(handle, NewIntegrationTime, NewGain,pSetGain, hdr_ratio);
    }
    result = IMX334_IsiSetIntegrationTimeIss(handle, NewIntegrationTime,pSetIntegrationTime,pNumberOfFramesToSkip, hdr_ratio);
    result =  IMX334_IsiSetGainIss(handle, NewGain,  pSetGain,  hdr_ratio);
    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);

    return result;
}

RESULT IMX334_IsiGetCurrentExposureIss
    (IsiSensorHandle_t handle, float *pSetGain, float *pSetIntegrationTime, float *hdr_ratio) {
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    if (pIMX334Ctx == NULL) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pSetGain == NULL) || (pSetIntegrationTime == NULL))
        return (RET_NULL_POINTER);

    *pSetGain = pIMX334Ctx->AecCurGain;
    *pSetIntegrationTime = pIMX334Ctx->AecCurIntegrationTime;
    *hdr_ratio = pIMX334Ctx->CurHdrRatio;

    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX334_IsiGetResolutionIss
    (IsiSensorHandle_t handle, uint16_t *pwidth, uint16_t *pheight) {
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    if (pIMX334Ctx == NULL) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    *pwidth = pIMX334Ctx->Config.Resolution.width;
    *pheight = pIMX334Ctx->Config.Resolution.height;
    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX334_IsiGetSensorFpsIss(IsiSensorHandle_t handle, uint32_t * pfps)
{
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    if (pIMX334Ctx == NULL) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;

    if (pIMX334Ctx->KernelDriverFlag) {
        ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_FPS, pfps);
        pIMX334Ctx->CurrFps = *pfps;
    }

    *pfps = pIMX334Ctx->CurrFps;

    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT IMX334_IsiSetSensorFpsIss(IsiSensorHandle_t handle, uint32_t fps)
{
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);
    return 0;
    if (pIMX334Ctx == NULL) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;

    if (fps > pIMX334Ctx->MaxFps) {
        TRACE(IMX334_ERROR,
              "%s: set fps(%d) out of range, correct to %d (%d, %d)\n",
              __func__, fps, pIMX334Ctx->MaxFps, pIMX334Ctx->MinFps,
              pIMX334Ctx->MaxFps);
        fps = pIMX334Ctx->MaxFps;
    }
    if (fps < pIMX334Ctx->MinFps) {
        TRACE(IMX334_ERROR,
              "%s: set fps(%d) out of range, correct to %d (%d, %d)\n",
              __func__, fps, pIMX334Ctx->MinFps, pIMX334Ctx->MinFps,
              pIMX334Ctx->MaxFps);
        fps = pIMX334Ctx->MinFps;
    }
    if (pIMX334Ctx->KernelDriverFlag) {
        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fps);
        if (ret != 0) {
            TRACE(IMX334_ERROR, "%s: set sensor fps=%d error\n",
                  __func__);
            return (RET_FAILURE);
        }
#ifdef SUBDEV_CHAR
        struct vvcam_ae_info_s ae_info;
        ret =
            ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_AE_INFO, &ae_info);
        if (ret != 0) {
            TRACE(IMX334_ERROR, "%s:sensor get ae info error!\n",
                  __func__);
            return (RET_FAILURE);
        }
        pIMX334Ctx->one_line_exp_time =
            (float)ae_info.one_line_exp_time_ns / 1000000000;
        pIMX334Ctx->MaxIntegrationLine = ae_info.max_integration_time;
        pIMX334Ctx->AecMaxIntegrationTime =
            pIMX334Ctx->MaxIntegrationLine *
            pIMX334Ctx->one_line_exp_time;
#endif
    } else {
        uint16_t FrameLengthLines;
        FrameLengthLines =
            pIMX334Ctx->FrameLengthLines * pIMX334Ctx->MaxFps / fps;
        result =
            IMX334_IsiRegisterWriteIss(handle, 0x30b2,
                           (FrameLengthLines >> 8) & 0xff);
        result |=
            IMX334_IsiRegisterWriteIss(handle, 0x30b3,
                           FrameLengthLines & 0xff);
        if (result != RET_SUCCESS) {
            TRACE(IMX334_ERROR,
                  "%s: Invalid sensor handle (NULL pointer detected)\n",
                  __func__);
            return (RET_FAILURE);
        }
        pIMX334Ctx->CurrFps = fps;
        pIMX334Ctx->CurFrameLengthLines = FrameLengthLines;
        pIMX334Ctx->MaxIntegrationLine =
            pIMX334Ctx->CurFrameLengthLines - 3;
        pIMX334Ctx->AecMaxIntegrationTime =
            pIMX334Ctx->MaxIntegrationLine *
            pIMX334Ctx->one_line_exp_time;
    }

    TRACE(IMX334_INFO, "%s: set sensor fps = %d\n", __func__,
          pIMX334Ctx->CurrFps);

    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);
    return (result);
}

/*
RESULT IMX334_IsiGetAfpsInfoIss(IsiSensorHandle_t handle,
                uint32_t Resolution, IsiAfpsInfo_t * pAfpsInfo)
{
    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    if (pIMX334Ctx == NULL) {
        TRACE(IMX334_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pAfpsInfo == NULL)
        return (RET_NULL_POINTER);

    // set current resolution data in info struct
    pAfpsInfo->AecMaxGain = pIMX334Ctx->AecMaxGain;
    pAfpsInfo->AecMinGain = pIMX334Ctx->AecMinGain;
    pAfpsInfo->AecMaxIntTime = pIMX334Ctx->AecMinIntegrationTime;
    pAfpsInfo->AecMaxIntTime = pIMX334Ctx->AecMaxIntegrationTime;
    pAfpsInfo->AecSlowestResolution = pIMX334Ctx->Resolution;
    pAfpsInfo->CurrResolution = pIMX334Ctx->Resolution;
    pAfpsInfo->CurrMaxIntTime = pIMX334Ctx->AecMaxIntegrationTime;
    pAfpsInfo->CurrMinIntTime = pIMX334Ctx->AecMinIntegrationTime;
    pAfpsInfo->Stage[0].MaxIntTime = pIMX334Ctx->AecMaxIntegrationTime;
    pAfpsInfo->Stage[0].Resolution = pIMX334Ctx->Resolution;

    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);
    return (result);
}
*/

static RESULT IMX334_IsiActivateTestPattern(IsiSensorHandle_t handle,
                        const bool_t enable)
{
    RESULT result = RET_SUCCESS;

    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    if (pIMX334Ctx == NULL || pIMX334Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    if (pIMX334Ctx->Configured != BOOL_TRUE)
        return RET_WRONG_STATE;

    if (BOOL_TRUE == enable) {
        result = IMX334_IsiRegisterWriteIss(handle, 0x3253, 0x80);
    } else {
        result = IMX334_IsiRegisterWriteIss(handle, 0x3253, 0x00);
    }
    pIMX334Ctx->TestPattern = enable;

    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);

    return (result);
}

/*
static RESULT IMX334_IsiEnableHdr(IsiSensorHandle_t handle, const bool_t enable)
{
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    TRACE(IMX334_INFO, "%s: (enter)\n", __func__);

    IMX334_Context_t *pIMX334Ctx = (IMX334_Context_t *) handle;
    if (pIMX334Ctx == NULL || pIMX334Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

#ifdef SUBDEV_CHAR
    HalContext_t *pHalCtx = (HalContext_t *) pIMX334Ctx->IsiCtx.HalHandle;
    result = IMX334_IsiSensorSetStreamingIss(handle, 0);

    if (pIMX334Ctx->KernelDriverFlag) {
        uint32_t hdr_mode;
        if (enable == 0) {
            hdr_mode = SENSOR_MODE_LINEAR;
        } else {
            hdr_mode = SENSOR_MODE_HDR_STITCH;
        }
        ret =
            ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_HDR_MODE,
              &hdr_mode);
    } else {
        result |=
            IMX334_IsiRegisterWriteIss(handle, 0x3190,
                           enable ? 0x05 : 0x08);
    }
    result |=
        IMX334_IsiSensorSetStreamingIss(handle, pIMX334Ctx->Streaming);
#endif

    if (result != 0 || ret != 0) {
        TRACE(IMX334_ERROR, "%s: change hdr status error\n", __func__);
        return RET_FAILURE;
    }

    pIMX334Ctx->enableHdr = enable;
    TRACE(IMX334_INFO, "%s: (exit)\n", __func__);
    return RET_SUCCESS;
}
*/

RESULT IMX334_IsiGetSensorIss(IsiSensor_t *pIsiSensor)
{
    RESULT result = RET_SUCCESS;
    TRACE( IMX334_INFO, "%s (enter)\n", __func__);

    if ( pIsiSensor != NULL ) {
        pIsiSensor->pszName                         = SensorName;
        pIsiSensor->pIsiCreateSensorIss             = IMX334_IsiCreateSensorIss;

        pIsiSensor->pIsiInitSensorIss               = IMX334_IsiInitSensorIss;
        pIsiSensor->pIsiGetSensorModeIss            = IMX334_IsiGetSensorModeIss;
        pIsiSensor->pIsiResetSensorIss              = IMX334_IsiResetSensorIss;
        pIsiSensor->pIsiReleaseSensorIss            = IMX334_IsiReleaseSensorIss;
        pIsiSensor->pIsiGetCapsIss                  = IMX334_IsiGetCapsIss;
        pIsiSensor->pIsiSetupSensorIss              = IMX334_IsiSetupSensorIss;
        pIsiSensor->pIsiChangeSensorResolutionIss   = IMX334_IsiChangeSensorResolutionIss;
        pIsiSensor->pIsiSensorSetStreamingIss       = IMX334_IsiSensorSetStreamingIss;
        pIsiSensor->pIsiSensorSetPowerIss           = IMX334_IsiSensorSetPowerIss;
        pIsiSensor->pIsiCheckSensorConnectionIss    = IMX334_IsiCheckSensorConnectionIss;
        pIsiSensor->pIsiGetSensorRevisionIss        = IMX334_IsiGetSensorRevisionIss;
        pIsiSensor->pIsiRegisterReadIss             = IMX334_IsiRegisterReadIss;
        pIsiSensor->pIsiRegisterWriteIss            = IMX334_IsiRegisterWriteIss;

        /* AEC functions */
        pIsiSensor->pIsiExposureControlIss          = IMX334_IsiExposureControlIss;
        pIsiSensor->pIsiGetGainLimitsIss            = IMX334_IsiGetGainLimitsIss;
        pIsiSensor->pIsiGetIntegrationTimeLimitsIss = IMX334_IsiGetIntegrationTimeLimitsIss;
        pIsiSensor->pIsiGetCurrentExposureIss       = IMX334_IsiGetCurrentExposureIss;
        pIsiSensor->pIsiGetVSGainIss                    = IMX334_IsiGetSEF1GainIss;
        pIsiSensor->pIsiGetGainIss                      = IMX334_IsiGetGainIss;
        pIsiSensor->pIsiGetGainIncrementIss             = IMX334_IsiGetGainIncrementIss;
        pIsiSensor->pIsiSetGainIss                      = IMX334_IsiSetGainIss;
        pIsiSensor->pIsiGetIntegrationTimeIss           = IMX334_IsiGetIntegrationTimeIss;
        pIsiSensor->pIsiGetVSIntegrationTimeIss         = IMX334_IsiGetSEF1IntegrationTimeIss;
        pIsiSensor->pIsiGetIntegrationTimeIncrementIss  = IMX334_IsiGetIntegrationTimeIncrementIss;
        pIsiSensor->pIsiSetIntegrationTimeIss           = IMX334_IsiSetIntegrationTimeIss;
        pIsiSensor->pIsiQuerySensorIss                  = IMX334_IsiQuerySensorIss;
        pIsiSensor->pIsiGetResolutionIss                = IMX334_IsiGetResolutionIss;
        pIsiSensor->pIsiGetSensorFpsIss                 = IMX334_IsiGetSensorFpsIss;
        pIsiSensor->pIsiSetSensorFpsIss                 = IMX334_IsiSetSensorFpsIss;

        /* AWB specific functions */

        /* Testpattern */
        pIsiSensor->pIsiActivateTestPattern         = IMX334_IsiActivateTestPattern;
        pIsiSensor->pIsiSetBayerPattern             = IMX334_IsiSetBayerPattern;

    } else {
        result = RET_NULL_POINTER;
    }

    TRACE( IMX334_INFO, "%s (exit)\n", __func__);
    return ( result );
}

/*****************************************************************************
* each sensor driver need declare this struct for isi load
*****************************************************************************/
IsiCamDrvConfig_t IMX334_IsiCamDrvConfig = {
    0,
    IMX334_IsiQuerySensorSupportIss,
    IMX334_IsiGetSensorIss,
    {
     SensorName,            /**< IsiSensor_t.pszName */
     0,            /**< IsiSensor_t.pIsiInitIss>*/
     0,              /**< IsiSensor_t.pIsiResetSensorIss>*/
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
     0,            /**< IsiSensor_t.pIsiEnableHdr */
     0,            /**< IsiSensor_t.pIsiSetBayerPattern */
     }
};
