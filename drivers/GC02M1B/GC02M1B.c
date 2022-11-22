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
#include "GC02M1B_priv.h"
#include "gc02m1b.h"

CREATE_TRACER( GC02M1B_INFO , "GC02M1B: ", INFO,    0);
CREATE_TRACER( GC02M1B_WARN , "GC02M1B: ", WARNING, 0);
CREATE_TRACER( GC02M1B_ERROR, "GC02M1B: ", ERROR,   1);
CREATE_TRACER( GC02M1B_DEBUG,     "GC02M1B: ", INFO, 0);
CREATE_TRACER( GC02M1B_REG_INFO , "GC02M1B: ", INFO, 0);
CREATE_TRACER( GC02M1B_REG_DEBUG, "GC02M1B: ", INFO, 0);

#ifdef SUBDEV_V4L2
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#undef TRACE
#define TRACE(x, ...)
#endif

#define GC02M1B_MIN_GAIN_STEP    ( 1.0f/16.0f )  /**< min gain step size used by GUI (hardware min = 1/16; 1/16..32/16 depending on actual gain ) */
#define GC02M1B_MAX_GAIN_AEC     ( 32.0f )       /**< max. gain used by the AEC (arbitrarily chosen, hardware limit = 62.0, driver limit = 32.0 ) */
#define GC02M1B_VS_MAX_INTEGRATION_TIME (0.0018)

/*****************************************************************************
 *Sensor Info
*****************************************************************************/
static const char SensorName[16] = "GC02M1B";

static struct vvcam_mode_info pgc02m1b_mode_info[] = {
    {
        .index     = 0,
        .width     = 1600,
        .height    = 1200,
        .fps       = 30,
        .hdr_mode  = SENSOR_MODE_LINEAR, 
        .bit_width = 10,
        .bayer_pattern = BAYER_BGGR,
        .mipi_phy_freq = 672, //mbps
        .mipi_line_num = 1,
        .preg_data = (void *)"gc02m1b sensor liner mode 1600*1200@30",
    },
};

static RESULT GC02M1B_IsiRegisterWriteIss(IsiSensorHandle_t handle, const uint32_t address, const uint32_t value);

static RESULT GC02M1B_IsiSensorSetPowerIss(IsiSensorHandle_t handle, bool_t on) {
    RESULT result = RET_SUCCESS;

    int ret = 0;
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL || pGC02M1BCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    TRACE(GC02M1B_INFO, "%s (enter)\n", __func__);

    int32_t enable = on;
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_POWER, &enable);
    if (ret != 0) {
        // to do
        //TRACE(GC02M1B_ERROR, "%s: sensor set power error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(GC02M1B_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT GC02M1B_IsiResetSensorIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;
    int ret = 0;

    TRACE(GC02M1B_INFO, "%s (enter)\n", __func__);

    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL || pGC02M1BCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_RESET, NULL);
    if (ret != 0) {
        TRACE(GC02M1B_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }

    TRACE(GC02M1B_INFO, "%s (exit)\n", __func__);
    return (result);
}

#ifdef SUBDEV_CHAR
static RESULT GC02M1B_IsiSensorSetClkIss(IsiSensorHandle_t handle, uint32_t clk) {
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;

    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL || pGC02M1BCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    TRACE(GC02M1B_INFO, "%s (enter)\n", __func__);

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_CLK, &clk);
    if (ret != 0) {
        // to do
        //TRACE(GC02M1B_ERROR, "%s: sensor set clk error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(GC02M1B_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT GC02M1B_IsiSensorGetClkIss
    (IsiSensorHandle_t handle, uint32_t * pclk) {
    RESULT result = RET_SUCCESS;
    int ret = 0;

    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL || pGC02M1BCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    TRACE(GC02M1B_INFO, "%s (enter)\n", __func__);

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_CLK, pclk);
    if (ret != 0) {
        // to do
        //TRACE(GC02M1B_ERROR, "%s: sensor get clk error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(GC02M1B_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT GC02M1B_IsiConfigSensorSCCBIss(IsiSensorHandle_t handle)
{
    return RET_SUCCESS;
}
#endif

static RESULT GC02M1B_IsiRegisterReadIss
    (IsiSensorHandle_t handle, const uint32_t address, uint32_t * p_value) {
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    TRACE(GC02M1B_INFO, "%s (enter)\n", __func__);

    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL || pGC02M1BCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    struct vvcam_sccb_data sccb_data;
    sccb_data.addr = address;
    sccb_data.data = 0;
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_READ_REG, &sccb_data);
    if (ret != 0) {
        TRACE(GC02M1B_ERROR, "%s: read sensor register error!\n",
              __func__);
        return (RET_FAILURE);
    }

    *p_value = sccb_data.data;

    TRACE(GC02M1B_INFO, "%s (exit) result = %d\n", __func__, result);
    return (result);
}

static RESULT GC02M1B_IsiRegisterWriteIss
    (IsiSensorHandle_t handle, const uint32_t address, const uint32_t value) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    TRACE(GC02M1B_INFO, "%s (enter)\n", __func__);

    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL || pGC02M1BCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    struct vvcam_sccb_data sccb_data;
    sccb_data.addr = address;
    sccb_data.data = value;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_WRITE_REG, &sccb_data);
    if (ret != 0) {
        TRACE(GC02M1B_ERROR, "%s: write sensor register error!\n",
              __func__);
        return (RET_FAILURE);
    }

    TRACE(GC02M1B_INFO, "%s (exit) result = %d\n", __func__, result);
    return (result);
}

static RESULT GC02M1B_IsiQuerySensorSupportIss(HalHandle_t  HalHandle, vvcam_mode_info_array_t *pSensorSupportInfo)
{
    TRACE(GC02M1B_DEBUG, "enter %s", __func__);
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
        TRACE(GC02M1B_ERROR, "%s: sensor kernel query error! Use isi query\n",__func__);
        psensor_mode_info_arry->count = sizeof(pgc02m1b_mode_info) / sizeof(struct vvcam_mode_info);
        memcpy(psensor_mode_info_arry->modes, pgc02m1b_mode_info, sizeof(pgc02m1b_mode_info));
    }
#endif
    psensor_mode_info_arry->count = sizeof(pgc02m1b_mode_info) / sizeof(struct vvcam_mode_info);
    memcpy(psensor_mode_info_arry->modes, pgc02m1b_mode_info, sizeof(pgc02m1b_mode_info));
    
    TRACE(GC02M1B_DEBUG, "%s-%s-%d: cnt=%d\n", __FILE__, __func__, __LINE__, psensor_mode_info_arry->count);

    return RET_SUCCESS;
}

static  RESULT GC02M1B_IsiQuerySensorIss(IsiSensorHandle_t handle, vvcam_mode_info_array_t *pSensorInfo)
{
    RESULT result = RET_SUCCESS;
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL || pGC02M1BCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;
    GC02M1B_IsiQuerySensorSupportIss(pHalCtx,pSensorInfo);

    return result;
}

static RESULT GC02M1B_IsiGetSensorModeIss(IsiSensorHandle_t handle,void *mode)
{
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL) {
        return (RET_WRONG_HANDLE);
    }
    memcpy(mode,&(pGC02M1BCtx->SensorMode), sizeof(pGC02M1BCtx->SensorMode));

    return ( RET_SUCCESS );
}

static RESULT GC02M1B_IsiCreateSensorIss(IsiSensorInstanceConfig_t * pConfig) {
    RESULT result = RET_SUCCESS;
    GC02M1B_Context_t *pGC02M1BCtx;

    TRACE(GC02M1B_INFO, "%s (enter)\n", __func__);

    if (!pConfig || !pConfig->pSensor)
        return (RET_NULL_POINTER);

    pGC02M1BCtx = (GC02M1B_Context_t *) malloc(sizeof(GC02M1B_Context_t));
    if (!pGC02M1BCtx) {
        TRACE(GC02M1B_ERROR, "%s: Can't allocate gc02m1b context\n",
              __func__);
        return (RET_OUTOFMEM);
    }

    MEMSET(pGC02M1BCtx, 0, sizeof(GC02M1B_Context_t));

    result = HalAddRef(pConfig->HalHandle);
    if (result != RET_SUCCESS) {
        free(pGC02M1BCtx);
        return (result);
    }

    pGC02M1BCtx->IsiCtx.HalHandle = pConfig->HalHandle;
    pGC02M1BCtx->IsiCtx.pSensor = pConfig->pSensor;
    pGC02M1BCtx->GroupHold = BOOL_FALSE;
    pGC02M1BCtx->OldGain = 0;
    pGC02M1BCtx->OldIntegrationTime = 0;
    pGC02M1BCtx->Configured = BOOL_FALSE;
    pGC02M1BCtx->Streaming = BOOL_FALSE;
    pGC02M1BCtx->TestPattern = BOOL_FALSE;
    pGC02M1BCtx->isAfpsRun = BOOL_FALSE;
    pGC02M1BCtx->SensorMode.index = pConfig->SensorModeIndex;
    pConfig->hSensor = (IsiSensorHandle_t) pGC02M1BCtx;
#ifdef SUBDEV_CHAR
    struct vvcam_mode_info *SensorDefaultMode = NULL;
    for (int i=0; i < sizeof(pgc02m1b_mode_info)/ sizeof(struct vvcam_mode_info); i++)
    {
        if (pgc02m1b_mode_info[i].index == pGC02M1BCtx->SensorMode.index)
        {
            SensorDefaultMode = &(pgc02m1b_mode_info[i]);
            break;
        }
    }

    if (SensorDefaultMode != NULL)
    {
        strcpy(pGC02M1BCtx->SensorRegCfgFile, get_vi_config_path());
        switch(SensorDefaultMode->index)
        {
            case 0:
                strcat(pGC02M1BCtx->SensorRegCfgFile,
                    "GC02M1B_mipi1lane_1600x1200@30_mayi.txt");
                break;
            default:
                break;
        }

        if (access(pGC02M1BCtx->SensorRegCfgFile, F_OK) == 0) {
            pGC02M1BCtx->KernelDriverFlag = 0;
            memcpy(&(pGC02M1BCtx->SensorMode),SensorDefaultMode,sizeof(struct vvcam_mode_info));
        } else {
            pGC02M1BCtx->KernelDriverFlag = 1;
        }
    }else
    {
        pGC02M1BCtx->KernelDriverFlag = 1;
    }

    result = GC02M1B_IsiSensorSetPowerIss(pGC02M1BCtx, BOOL_TRUE);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    uint32_t SensorClkIn = 0;
    if (pGC02M1BCtx->KernelDriverFlag) {
        result = GC02M1B_IsiSensorGetClkIss(pGC02M1BCtx, &SensorClkIn);
        RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);
    }

    result = GC02M1B_IsiSensorSetClkIss(pGC02M1BCtx, SensorClkIn);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    result = GC02M1B_IsiResetSensorIss(pGC02M1BCtx);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    pGC02M1BCtx->pattern = ISI_BPAT_BGBGGRGR;

    if (!pGC02M1BCtx->KernelDriverFlag) {
        result = GC02M1B_IsiConfigSensorSCCBIss(pGC02M1BCtx);
        RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);
    }
#endif

    TRACE(GC02M1B_INFO, "%s (exit pConfig->hSensor = %p)\n", __func__, pConfig->hSensor);
    return (result);
}

static RESULT GC02M1B_IsiGetRegCfgIss(const char *registerFileName,
                     struct vvcam_sccb_array *arry)
{
    if (NULL == registerFileName) {
        TRACE(GC02M1B_ERROR, "%s:registerFileName is NULL\n", __func__);
        return (RET_NULL_POINTER);
    }
#ifdef SUBDEV_CHAR
    FILE *fp = NULL;
    fp = fopen(registerFileName, "rb");
    if (!fp) {
        TRACE(GC02M1B_ERROR, "%s:load register file  %s error!\n",
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
        TRACE(GC02M1B_ERROR, "%s:malloc failed NULL Point!\n", __func__,
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

static RESULT GC02M1B_IsiInitSensorIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;

    int ret = 0;
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;

    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;
    TRACE(GC02M1B_INFO, "%s (enter handle = %p)\n", __func__, handle);

    if (pGC02M1BCtx == NULL) {
        return (RET_WRONG_HANDLE);
    }
	TRACE(GC02M1B_INFO, "%s (pGC02M1BCtx->KernelDriverFlag = %d)\n", __func__, pGC02M1BCtx->KernelDriverFlag);
    if (pGC02M1BCtx->KernelDriverFlag) {
        ;
    } else {
		TRACE(GC02M1B_INFO, "%s (001)\n", __func__);
        struct vvcam_sccb_array arry;
        result = GC02M1B_IsiGetRegCfgIss(pGC02M1BCtx->SensorRegCfgFile, &arry);
        if (result != 0) {
            TRACE(GC02M1B_ERROR,
                  "%s:GC02M1B_IsiGetRegCfgIss error!\n", __func__);
            return (RET_FAILURE);
        }

        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_WRITE_ARRAY, &arry);
        if (ret != 0) {
            TRACE(GC02M1B_ERROR, "%s:Sensor Write Reg arry error!\n",
                  __func__);
            return (RET_FAILURE);
        }
		TRACE(GC02M1B_INFO, "%s (pGC02M1BCtx->SensorMode.index = %d)\n", __func__, pGC02M1BCtx->SensorMode.index);
        switch(pGC02M1BCtx->SensorMode.index)
        {
            case 0:
                pGC02M1BCtx->one_line_exp_time = 0.00002609; // line_time = line_length / pclk =1460/87.6mhz = 0.0000167
                pGC02M1BCtx->FrameLengthLines = 0x4FE; //framelength=1278=0x4FE
                pGC02M1BCtx->CurFrameLengthLines = pGC02M1BCtx->FrameLengthLines;
                pGC02M1BCtx->MaxIntegrationLine = pGC02M1BCtx->CurFrameLengthLines - 16;
                pGC02M1BCtx->MinIntegrationLine = 1;
                pGC02M1BCtx->AecMaxGain = 16;
                pGC02M1BCtx->AecMinGain = 1;
                break;
            default:
                return (RET_FAILURE);
        }
		pGC02M1BCtx->AecIntegrationTimeIncrement = pGC02M1BCtx->one_line_exp_time;
		pGC02M1BCtx->AecMinIntegrationTime =
			pGC02M1BCtx->one_line_exp_time * pGC02M1BCtx->MinIntegrationLine;
		pGC02M1BCtx->AecMaxIntegrationTime =
			pGC02M1BCtx->one_line_exp_time * pGC02M1BCtx->MaxIntegrationLine;


        pGC02M1BCtx->MaxFps  = pGC02M1BCtx->SensorMode.fps;
        pGC02M1BCtx->MinFps  = 1;
        pGC02M1BCtx->CurrFps = pGC02M1BCtx->MaxFps;
    }
    TRACE(GC02M1B_INFO, "%s (pGC02M1BCtx->one_line_exp_time = %f)\n", __func__, pGC02M1BCtx->one_line_exp_time);
    TRACE(GC02M1B_INFO, "%s (pGC02M1BCtx->MinIntegrationLine = %d, pGC02M1BCtx->MaxIntegrationLine = %d)\n", __func__, pGC02M1BCtx->MinIntegrationLine, pGC02M1BCtx->MaxIntegrationLine);
    return (result);
}

static RESULT GC02M1B_IsiReleaseSensorIss(IsiSensorHandle_t handle) {
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(GC02M1B_INFO, "%s (enter)\n", __func__);

    if (pGC02M1BCtx == NULL)
        return (RET_WRONG_HANDLE);

    (void)GC02M1B_IsiSensorSetStreamingIss(pGC02M1BCtx, BOOL_FALSE);
    (void)GC02M1B_IsiSensorSetPowerIss(pGC02M1BCtx, BOOL_FALSE);
    (void)HalDelRef(pGC02M1BCtx->IsiCtx.HalHandle);

    MEMSET(pGC02M1BCtx, 0, sizeof(GC02M1B_Context_t));
    free(pGC02M1BCtx);
    TRACE(GC02M1B_INFO, "%s (exit)\n", __func__);
    return (result);
}

struct gc02m1b_fmt {
    int width;
    int height;
    int fps;
};

static RESULT GC02M1B_IsiSetupSensorIss
    (IsiSensorHandle_t handle, const IsiSensorConfig_t * pConfig) {

    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    RESULT result = RET_SUCCESS;

    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (!pGC02M1BCtx) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pConfig) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid configuration (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    if (pGC02M1BCtx->Streaming != BOOL_FALSE) {
        return RET_WRONG_STATE;
    }

    memcpy(&pGC02M1BCtx->Config, pConfig, sizeof(IsiSensorConfig_t));

    /* 1.) SW reset of image sensor (via I2C register interface)  be careful, bits 6..0 are reserved, reset bit is not sticky */
    TRACE(GC02M1B_DEBUG, "%s: GC02M1B System-Reset executed\n", __func__);
    osSleep(100);

    //GC02M1B_AecSetModeParameters not defined yet as of 2021/8/9.
    //result = GC02M1B_AecSetModeParameters(pGC02M1BCtx, pConfig);
    //if (result != RET_SUCCESS) {
    //    TRACE(GC02M1B_ERROR, "%s: SetupOutputWindow failed.\n",
    //          __func__);
    //    return (result);
    //}
#if 1
    struct gc02m1b_fmt fmt;
    fmt.width = pConfig->Resolution.width;
    fmt.height = pConfig->Resolution.height;

    ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fmt);//result = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fmt);
#endif
    pGC02M1BCtx->Configured = BOOL_TRUE;
    TRACE(GC02M1B_INFO, "%s: (exit) ret=0x%x \n", __func__, result);
    return result;
}

static RESULT GC02M1B_IsiChangeSensorResolutionIss(IsiSensorHandle_t handle, uint16_t width, uint16_t height) {
    RESULT result = RET_SUCCESS;
#if 0
    struct gc02m1b_fmt fmt;
    fmt.width = width;
    fmt.height = height;

    int ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fmt);
#endif
    TRACE(GC02M1B_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT GC02M1B_IsiSensorSetStreamingIss
    (IsiSensorHandle_t handle, bool_t on) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    TRACE(GC02M1B_INFO, "%s (enter)\n", __func__);

    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL || pGC02M1BCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    if (pGC02M1BCtx->Configured != BOOL_TRUE)
        return RET_WRONG_STATE;

    int32_t enable = (uint32_t) on;
    ret = GC02M1B_IsiRegisterWriteIss(handle, 0xfe, 0x00);
    if (ret != 0) {
        return (RET_FAILURE);
    }

    if (on == true) {
        ret = GC02M1B_IsiRegisterWriteIss(handle, 0x3e, 0x90);
    } else {
        ret = GC02M1B_IsiRegisterWriteIss(handle, 0x3e, 0x00);
    }

    if (ret != 0) {
        return (RET_FAILURE);
    }

    pGC02M1BCtx->Streaming = on;

    TRACE(GC02M1B_INFO, "%s (exit)\n", __func__);
    return (result);
}

static int32_t sensor_get_chip_id(IsiSensorHandle_t handle, uint32_t *chip_id)
{
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    int32_t chip_id_high = 0;
    int32_t chip_id_low = 0;

    ret = GC02M1B_IsiRegisterReadIss(handle, 0xf0, &chip_id_high);
    if (ret != 0) {
        TRACE(GC02M1B_ERROR,
            "%s: Read Sensor correct ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    ret = GC02M1B_IsiRegisterReadIss(handle, 0xf1, &chip_id_low);
    if (ret != 0) {
        TRACE(GC02M1B_ERROR,
            "%s: Read Sensor correct ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    *chip_id = ((chip_id_high & 0xff)<<8) | (chip_id_low & 0xff);

    return 0;
}

static RESULT GC02M1B_IsiCheckSensorConnectionIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    uint32_t correct_id = 0x02e0;
    uint32_t sensor_id = 0;

    TRACE(GC02M1B_INFO, "%s (enter)\n", __func__);

    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL || pGC02M1BCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    ret = sensor_get_chip_id(handle, &sensor_id);
    if (ret != 0) {
        TRACE(GC02M1B_ERROR,
            "%s: Read Sensor chip ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    if (correct_id != sensor_id) {
        TRACE(GC02M1B_ERROR, "%s:ChipID =0x%x sensor_id=%x error! \n",
              __func__, correct_id, sensor_id);
        return (RET_FAILURE);
    }

    printf("%s ChipID = 0x%08x, sensor_id = 0x%08x, success! \n", __func__,
          correct_id, sensor_id);
    TRACE(GC02M1B_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT GC02M1B_IsiGetSensorRevisionIss
    (IsiSensorHandle_t handle, uint32_t * p_value) {
    RESULT result = RET_SUCCESS;
    TRACE(GC02M1B_INFO, "%s (enter)\n", __func__);

    *p_value = 0X5690;
    TRACE(GC02M1B_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT GC02M1B_IsiGetGainLimitsIss
    (IsiSensorHandle_t handle, float *pMinGain, float *pMaxGain) {
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    /*TODO*/

    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (pGC02M1BCtx == NULL) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pMinGain == NULL) || (pMaxGain == NULL)) {
        TRACE(GC02M1B_ERROR, "%s: NULL pointer received!!\n");
        return (RET_NULL_POINTER);
    }

    *pMinGain = pGC02M1BCtx->AecMinGain;
    *pMaxGain = pGC02M1BCtx->AecMaxGain;

    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);
    return (result);
}

static RESULT GC02M1B_IsiGetIntegrationTimeLimitsIss
    (IsiSensorHandle_t handle,
     float *pMinIntegrationTime, float *pMaxIntegrationTime) {
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    /*TODO*/

    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);
    if (pGC02M1BCtx == NULL) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pMinIntegrationTime == NULL) || (pMaxIntegrationTime == NULL)) {
        TRACE(GC02M1B_ERROR, "%s: NULL pointer received!!\n");
        return (RET_NULL_POINTER);
    }

    *pMinIntegrationTime = pGC02M1BCtx->AecMinIntegrationTime;
    *pMaxIntegrationTime = pGC02M1BCtx->AecMaxIntegrationTime;

    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC02M1B_IsiGetGainIss(IsiSensorHandle_t handle, float *pSetGain) {
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (pGC02M1BCtx == NULL) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pSetGain == NULL) {
        return (RET_NULL_POINTER);
    }

    *pSetGain = pGC02M1BCtx->AecCurGain;

    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC02M1B_IsiGetLongGainIss(IsiSensorHandle_t handle, float *gain)
{
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;

    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (pGC02M1BCtx == NULL) {
        TRACE(GC02M1B_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }

    if (gain == NULL) {
        return (RET_NULL_POINTER);
    }

    *gain = pGC02M1BCtx->AecCurLongGain;

    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);

    return (RET_SUCCESS);
}

RESULT GC02M1B_IsiGetVSGainIss(IsiSensorHandle_t handle, float *pSetGain) {
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (pGC02M1BCtx == NULL) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pSetGain == NULL) {
        return (RET_NULL_POINTER);
    }

    *pSetGain = pGC02M1BCtx->AecCurVSGain;

    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);

    return (result);
}

RESULT GC02M1B_IsiGetGainIncrementIss(IsiSensorHandle_t handle, float *pIncr) {
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (pGC02M1BCtx == NULL) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pIncr == NULL)
        return (RET_NULL_POINTER);

    *pIncr = pGC02M1BCtx->AecGainIncrement;

    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);

    return (result);
}

uint32_t gainLevelTable[17] = {
								 64,
								 96,
								127,
								157,
								197,
								226,
								259,					
								287,
								318,
								356,			
								391,
								419,
								450,
								480,
								513,
								646,
								0xffffffff,
};

RESULT GC02M1B_IsiSetGainIss
    (IsiSensorHandle_t handle,
     float NewGain, float *pSetGain, float *hdr_ratio) {

    RESULT result = RET_SUCCESS;
    int32_t ret = 0;


    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL || pGC02M1BCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;
    //GC02M1B specific
    int Analog_Index;
    uint32_t tol_dig_gain = 0, SensorGain;	    
    int total;	
    total = sizeof(gainLevelTable) / sizeof(uint32_t);
	
    SensorGain = NewGain * 64;
    if (SensorGain < 64) {
        SensorGain = 64;
    }

    for(Analog_Index = 0; Analog_Index < total; Analog_Index++)
    {
        if((gainLevelTable[Analog_Index] <= SensorGain)&&(SensorGain < gainLevelTable[Analog_Index+1]))
            break;
    }	    

    tol_dig_gain = SensorGain*1024/gainLevelTable[Analog_Index];	 

    ret = GC02M1B_IsiRegisterWriteIss(handle, 0xfe, 0x00);
    if (ret != 0) {
        return (RET_FAILURE);
    }
    ret = GC02M1B_IsiRegisterWriteIss(handle, 0xb6, Analog_Index);
    if (ret != 0) {
        return (RET_FAILURE);
    }

    ret = GC02M1B_IsiRegisterWriteIss(handle, 0xb1,(tol_dig_gain>>8));
    if (ret != 0) {
        return (RET_FAILURE);
    }
    ret = GC02M1B_IsiRegisterWriteIss(handle, 0xb2, (tol_dig_gain&0xff));
    if (ret != 0) {
        return (RET_FAILURE);
    }

    volatile int32_t reg;
    TRACE(GC02M1B_DEBUG, "%s NewGain=%f SensorGain=%d Analog_Index=%d,gainLevelTable[Analog_Index]=%d,tol_dig_gain=%u b1=0x%x b2=0x%x\n",__func__, 
        NewGain, SensorGain, Analog_Index, gainLevelTable[Analog_Index], tol_dig_gain, (tol_dig_gain>>8), (tol_dig_gain&0xff));
    TRACE(GC02M1B_DEBUG, "%s 0xb6 write=0x%x,0xb1 write 0x%x,0xb2 write 0x%x\n",__func__, Analog_Index,(tol_dig_gain>>8), (tol_dig_gain&0xff));
    GC02M1B_IsiRegisterReadIss(handle, 0xb6, &reg);
    TRACE(GC02M1B_DEBUG, "%s 0xb6 read 0x0%x\n",__func__, reg);
    GC02M1B_IsiRegisterReadIss(handle, 0xb1, &reg);
    TRACE(GC02M1B_DEBUG, "%s 0xb1 read 0x0%x\n",__func__, reg);
    GC02M1B_IsiRegisterReadIss(handle, 0xb2, &reg);
    TRACE(GC02M1B_DEBUG, "%s 0xb2 read 0x0%x\n",__func__, reg);

    pGC02M1BCtx->AecCurGain = ((float)(NewGain));
    *pSetGain = pGC02M1BCtx->AecCurGain;
    TRACE(GC02M1B_DEBUG, "%s: g=%f\n", __func__, *pSetGain);
    return (result);
}

RESULT GC02M1B_IsiSetLongGainIss(IsiSensorHandle_t handle, float gain)
{
    int ret = 0;
    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;

    if (!pGC02M1BCtx || !pGC02M1BCtx->IsiCtx.HalHandle)
    {
        TRACE(GC02M1B_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }

    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    uint32_t SensorGain = 0;
    SensorGain = gain * pGC02M1BCtx->gain_accuracy;
    if (pGC02M1BCtx->LastLongGain != SensorGain)
    {

        /*TODO*/
#if 0
        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_LONG_GAIN, &SensorGain);
        if (ret != 0)
        {
            return (RET_FAILURE);
            TRACE(GC02M1B_ERROR,"%s: set long gain failed\n");

        }
#endif
        pGC02M1BCtx->LastLongGain = SensorGain;
        pGC02M1BCtx->AecCurLongGain = gain;
    }

    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT GC02M1B_IsiSetVSGainIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float NewGain, float *pSetGain, float *hdr_ratio) {
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;
    RESULT result = RET_SUCCESS;
#if 0
    float Gain = 0.0f;

    uint32_t ucGain = 0U;
    uint32_t again = 0U;
#endif

    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (!pGC02M1BCtx) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetGain || !hdr_ratio)
        return (RET_NULL_POINTER);

    uint32_t SensorGain = 0;
    SensorGain = NewGain * pGC02M1BCtx->gain_accuracy;

    /*TODO*/
    //ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_VSGAIN, &SensorGain);

    pGC02M1BCtx->AecCurVSGain = NewGain;
    *pSetGain = pGC02M1BCtx->AecCurGain;
    TRACE(GC02M1B_DEBUG, "%s: g=%f\n", __func__, *pSetGain);
    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC02M1B_IsiSetBayerPattern(IsiSensorHandle_t handle, uint8_t pattern)
{

    RESULT result = RET_SUCCESS;
#if 0
    uint8_t h_shift = 0, v_shift = 0;
    uint32_t val_h = 0, val_l = 0;
    uint16_t val = 0;
    uint8_t Start_p = 0;
    bool_t streaming_status;
    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL || pGC02M1BCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    // pattern 0:B 1:GB 2:GR 3:R
    streaming_status = pGC02M1BCtx->Streaming;
    result = GC02M1B_IsiSensorSetStreamingIss(handle, 0);
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

    GC02M1B_IsiRegisterReadIss(handle, 0x30a0, &val_h);
    GC02M1B_IsiRegisterReadIss(handle, 0x30a1, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + h_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    GC02M1B_IsiRegisterWriteIss(handle, 0x30a0, (uint8_t)val_h);
    GC02M1B_IsiRegisterWriteIss(handle, 0x30a1, (uint8_t)val_l);

    GC02M1B_IsiRegisterReadIss(handle, 0x30a2, &val_h);
    GC02M1B_IsiRegisterReadIss(handle, 0x30a3, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + v_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    GC02M1B_IsiRegisterWriteIss(handle, 0x30a2, (uint8_t)val_h);
    GC02M1B_IsiRegisterWriteIss(handle, 0x30a3, (uint8_t)val_l);

    GC02M1B_IsiRegisterReadIss(handle, 0x30a4, &val_h);
    GC02M1B_IsiRegisterReadIss(handle, 0x30a5, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + h_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    GC02M1B_IsiRegisterWriteIss(handle, 0x30a4, (uint8_t)val_h);
    GC02M1B_IsiRegisterWriteIss(handle, 0x30a5, (uint8_t)val_l);

    GC02M1B_IsiRegisterReadIss(handle, 0x30a6, &val_h);
    GC02M1B_IsiRegisterReadIss(handle, 0x30a7, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + v_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    GC02M1B_IsiRegisterWriteIss(handle, 0x30a6, (uint8_t)val_h);
    GC02M1B_IsiRegisterWriteIss(handle, 0x30a7, (uint8_t)val_l);

    pGC02M1BCtx->pattern = pattern;
    result = GC02M1B_IsiSensorSetStreamingIss(handle, streaming_status);
#endif

    return (result);
}

RESULT GC02M1B_IsiGetIntegrationTimeIss
    (IsiSensorHandle_t handle, float *pSetIntegrationTime)
{
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (!pGC02M1BCtx) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetIntegrationTime)
        return (RET_NULL_POINTER);
    *pSetIntegrationTime = pGC02M1BCtx->AecCurIntegrationTime;
    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC02M1B_IsiGetLongIntegrationTimeIss(IsiSensorHandle_t handle, float *pIntegrationTime)
{
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (!pGC02M1BCtx) {
        TRACE(GC02M1B_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }
    if (!pIntegrationTime)
        return (RET_NULL_POINTER);

    pGC02M1BCtx->AecCurLongIntegrationTime =  pGC02M1BCtx->AecCurIntegrationTime;

    *pIntegrationTime = pGC02M1BCtx->AecCurLongIntegrationTime;
    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT GC02M1B_IsiGetVSIntegrationTimeIss
    (IsiSensorHandle_t handle, float *pSetIntegrationTime)
{
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (!pGC02M1BCtx) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    if (!pSetIntegrationTime)
        return (RET_NULL_POINTER);

    *pSetIntegrationTime = pGC02M1BCtx->AecCurVSIntegrationTime;
    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC02M1B_IsiGetIntegrationTimeIncrementIss
    (IsiSensorHandle_t handle, float *pIncr)
{
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (!pGC02M1BCtx) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pIncr)
        return (RET_NULL_POINTER);

    //_smallest_ increment the sensor/driver can handle (e.g. used for sliders in the application)
    *pIncr = pGC02M1BCtx->AecIntegrationTimeIncrement;
    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC02M1B_IsiSetIntegrationTimeIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float *pSetIntegrationTime,
     uint8_t * pNumberOfFramesToSkip, float *hdr_ratio)
{
    RESULT result = RET_SUCCESS;

    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    uint32_t exp_line = 0;
    uint32_t exp_line_old = 0;
    int ret = 0;

    TRACE(GC02M1B_INFO, "%s: (enter handle = %p)\n", __func__, handle);

    if (!pGC02M1BCtx) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetIntegrationTime || !pNumberOfFramesToSkip) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }
    exp_line = NewIntegrationTime / pGC02M1BCtx->one_line_exp_time;
    exp_line_old = exp_line;
    exp_line =
        MIN(pGC02M1BCtx->MaxIntegrationLine,
        MAX(pGC02M1BCtx->MinIntegrationLine, exp_line));

    TRACE(GC02M1B_DEBUG, "%s: set AEC_PK_EXPO=0x%05x min_exp_line = %d, max_exp_line = %d\n", __func__, exp_line, pGC02M1BCtx->MinIntegrationLine, pGC02M1BCtx->MaxIntegrationLine);

    if (exp_line != pGC02M1BCtx->OldIntegrationTime) {

        /*TODO*/
        //ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_EXP, &exp_line);
        pGC02M1BCtx->OldIntegrationTime = exp_line;    // remember current integration time
        pGC02M1BCtx->AecCurIntegrationTime =
            exp_line * pGC02M1BCtx->one_line_exp_time;

        *pNumberOfFramesToSkip = 1U;    //skip 1 frame
    } else {
        *pNumberOfFramesToSkip = 0U;    //no frame skip
    }

    if (NewIntegrationTime > pGC02M1BCtx->MaxIntegrationLine * pGC02M1BCtx->one_line_exp_time)
        NewIntegrationTime = pGC02M1BCtx->MaxIntegrationLine * pGC02M1BCtx->one_line_exp_time;
    
    // GC02M1B specific
     int vts = exp_line + 16;

    ret = GC02M1B_IsiRegisterWriteIss(handle, 0xfe, 0x00);
    if (ret != 0) {
        return (RET_FAILURE);
    }
    ret = GC02M1B_IsiRegisterWriteIss(handle, 0x41, (vts>>8)&0xff);
    if (ret != 0) {
        return (RET_FAILURE);
    }
    ret = GC02M1B_IsiRegisterWriteIss(handle, 0x42, (vts&0xff));
    if (ret != 0) {
        return (RET_FAILURE);
    }
    ret = GC02M1B_IsiRegisterWriteIss(handle, 0x03, (exp_line>>8));
    if (ret != 0) {
        return (RET_FAILURE);
    }
    ret = GC02M1B_IsiRegisterWriteIss(handle, 0x04, (exp_line&0xff));
    if (ret != 0) {
        return (RET_FAILURE);
    }

    volatile int32_t reg;
    TRACE(GC02M1B_DEBUG, "%s exp_line = %fs / %fs = %d vts=%d\n",__func__, NewIntegrationTime,  pGC02M1BCtx->one_line_exp_time, exp_line, vts);
    //TRACE(GC02M1B_DEBUG, "%s cal_shutter=%d,Dgain_ratio=%u\n", __func__, cal_shutter, Dgain_ratio);
    TRACE(GC02M1B_DEBUG, "%s 0x41 write 0x%x, 0x42 write 0x%x\n", __func__, (vts>>8)&0xff, (vts&0xff));
    TRACE(GC02M1B_DEBUG, "%s 0x03 write 0x%x, 0x04 write 0x%x\n", __func__, (exp_line>>8), (exp_line&0xff));
    GC02M1B_IsiRegisterReadIss(handle, 0x41, &reg);
    TRACE(GC02M1B_DEBUG, "%s 0x41 read 0x0%x\n",__func__, reg);
    GC02M1B_IsiRegisterReadIss(handle, 0x42, &reg);
    TRACE(GC02M1B_DEBUG, "%s 0x42 read 0x0%x\n",__func__, reg);
    GC02M1B_IsiRegisterReadIss(handle, 0x03, &reg);
    TRACE(GC02M1B_DEBUG, "%s 0x03 read 0x0%x\n",__func__, reg);
    GC02M1B_IsiRegisterReadIss(handle, 0x04, &reg);
    TRACE(GC02M1B_DEBUG, "%s 0x04 read 0x0%x\n",__func__, reg);

    if (exp_line_old != exp_line) {
        *pSetIntegrationTime = pGC02M1BCtx->AecCurIntegrationTime;
    } else {
        *pSetIntegrationTime = NewIntegrationTime;
    }

    TRACE(GC02M1B_DEBUG, "%s: Ti=%f\n", __func__, *pSetIntegrationTime);
    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC02M1B_IsiSetLongIntegrationTimeIss(IsiSensorHandle_t handle,float IntegrationTime)
{
    int ret;
    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (!handle || !pGC02M1BCtx->IsiCtx.HalHandle)
    {
        TRACE(GC02M1B_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    uint32_t exp_line = 0;
    exp_line = IntegrationTime / pGC02M1BCtx->one_line_exp_time;
    exp_line = MIN(pGC02M1BCtx->MaxIntegrationLine, MAX(pGC02M1BCtx->MinIntegrationLine, exp_line));

    if (exp_line != pGC02M1BCtx->LastLongExpLine)
    {
        if (pGC02M1BCtx->KernelDriverFlag)
        {
            ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_LONG_EXP, &exp_line);
            if (ret != 0)
            {
                TRACE(GC02M1B_ERROR,"%s: set long gain failed\n");
                return RET_FAILURE;
            }
        }

        pGC02M1BCtx->LastLongExpLine = exp_line;
        pGC02M1BCtx->AecCurLongIntegrationTime =  pGC02M1BCtx->LastLongExpLine*pGC02M1BCtx->one_line_exp_time;
    }


    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT GC02M1B_IsiSetVSIntegrationTimeIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float *pSetVSIntegrationTime,
     uint8_t * pNumberOfFramesToSkip, float *hdr_ratio)
{
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    uint32_t exp_line = 0;

    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (!pGC02M1BCtx) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetVSIntegrationTime || !pNumberOfFramesToSkip) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    TRACE(GC02M1B_INFO,
          "%s:  maxIntegrationTime-=%f minIntegrationTime = %f\n", __func__,
          pGC02M1BCtx->AecMaxIntegrationTime,
          pGC02M1BCtx->AecMinIntegrationTime);


    exp_line = NewIntegrationTime / pGC02M1BCtx->one_line_exp_time;
    exp_line =
        MIN(pGC02M1BCtx->MaxIntegrationLine,
        MAX(pGC02M1BCtx->MinIntegrationLine, exp_line));

    if (exp_line != pGC02M1BCtx->OldVsIntegrationTime) {
    /*TODO*/
    //    ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_VSEXP, &exp_line);
    } else if (1){

        pGC02M1BCtx->OldVsIntegrationTime = exp_line;
        pGC02M1BCtx->AecCurVSIntegrationTime = exp_line * pGC02M1BCtx->one_line_exp_time;    //remember current integration time
        *pNumberOfFramesToSkip = 1U;    //skip 1 frame
    } else {
        *pNumberOfFramesToSkip = 0U;    //no frame skip
    }

    *pSetVSIntegrationTime = pGC02M1BCtx->AecCurVSIntegrationTime;

    TRACE(GC02M1B_DEBUG, "%s: NewIntegrationTime=%f\n", __func__,
          NewIntegrationTime);
    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC02M1B_IsiExposureControlIss
    (IsiSensorHandle_t handle,
     float NewGain,
     float NewIntegrationTime,
     uint8_t * pNumberOfFramesToSkip,
     float *pSetGain, float *pSetIntegrationTime, float *hdr_ratio)
{
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    int TmpGain;
    /*TODO*/

    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (pGC02M1BCtx == NULL) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pNumberOfFramesToSkip == NULL) || (pSetGain == NULL)
        || (pSetIntegrationTime == NULL)) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    TRACE(GC02M1B_ERROR, "%s: g=%f, Ti=%f\n", __func__, NewGain,
          NewIntegrationTime);

    result = GC02M1B_IsiSetIntegrationTimeIss(handle, NewIntegrationTime,
                        pSetIntegrationTime,
                        pNumberOfFramesToSkip, hdr_ratio);
    result = GC02M1B_IsiSetGainIss(handle, NewGain, pSetGain, hdr_ratio);

    pGC02M1BCtx->CurHdrRatio = *hdr_ratio;

    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);

    return result;
}

RESULT GC02M1B_IsiGetCurrentExposureIss
    (IsiSensorHandle_t handle, float *pSetGain, float *pSetIntegrationTime, float *hdr_ratio) {
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (pGC02M1BCtx == NULL) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pSetGain == NULL) || (pSetIntegrationTime == NULL))
        return (RET_NULL_POINTER);

    *pSetGain = pGC02M1BCtx->AecCurGain;
    *pSetIntegrationTime = pGC02M1BCtx->AecCurIntegrationTime;
    *hdr_ratio = pGC02M1BCtx->CurHdrRatio;

    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC02M1B_IsiGetResolutionIss(IsiSensorHandle_t handle, uint16_t *pwidth, uint16_t *pheight) {
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (pGC02M1BCtx == NULL) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    *pwidth = pGC02M1BCtx->SensorMode.width;
    *pheight =  pGC02M1BCtx->SensorMode.height;

    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC02M1B_IsiGetSensorFpsIss(IsiSensorHandle_t handle, uint32_t * pfps)
{
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    RESULT result = RET_SUCCESS;


    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (pGC02M1BCtx == NULL) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    if (pGC02M1BCtx->KernelDriverFlag) {
       /*TODO*/
        ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_FPS, pfps);
        pGC02M1BCtx->CurrFps = *pfps;
    }

    *pfps = pGC02M1BCtx->CurrFps;

    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC02M1B_IsiSetSensorFpsIss(IsiSensorHandle_t handle, uint32_t fps)
{
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    if (pGC02M1BCtx == NULL) {
        TRACE(GC02M1B_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    if (fps > pGC02M1BCtx->MaxFps) {
        TRACE(GC02M1B_ERROR,
              "%s: set fps(%d) out of range, correct to %d (%d, %d)\n",
              __func__, fps, pGC02M1BCtx->MaxFps, pGC02M1BCtx->MinFps,
              pGC02M1BCtx->MaxFps);
        fps = pGC02M1BCtx->MaxFps;
    }
    if (fps < pGC02M1BCtx->MinFps) {
        TRACE(GC02M1B_ERROR,
              "%s: set fps(%d) out of range, correct to %d (%d, %d)\n",
              __func__, fps, pGC02M1BCtx->MinFps, pGC02M1BCtx->MinFps,
              pGC02M1BCtx->MaxFps);
        fps = pGC02M1BCtx->MinFps;
    }
    if (pGC02M1BCtx->KernelDriverFlag) {
        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fps);
        if (ret != 0) {
            TRACE(GC02M1B_ERROR, "%s: set sensor fps=%d error\n",
                  __func__);
            return (RET_FAILURE);
        }

        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_SENSOR_MODE, &(pGC02M1BCtx->SensorMode));
        {
            pGC02M1BCtx->MaxIntegrationLine = pGC02M1BCtx->SensorMode.ae_info.max_integration_time;
            pGC02M1BCtx->AecMaxIntegrationTime = pGC02M1BCtx->MaxIntegrationLine * pGC02M1BCtx->one_line_exp_time;
        }
#ifdef SUBDEV_CHAR
        struct vvcam_ae_info_s ae_info;
        ret =
            ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_AE_INFO, &ae_info);
        if (ret != 0) {
            TRACE(GC02M1B_ERROR, "%s:sensor get ae info error!\n",
                  __func__);
            return (RET_FAILURE);
        }
        pGC02M1BCtx->one_line_exp_time =
            (float)ae_info.one_line_exp_time_ns / 1000000000;
        pGC02M1BCtx->MaxIntegrationLine = ae_info.max_integration_time;
        pGC02M1BCtx->AecMaxIntegrationTime =
            pGC02M1BCtx->MaxIntegrationLine *
            pGC02M1BCtx->one_line_exp_time;
#endif
    }

    TRACE(GC02M1B_INFO, "%s: set sensor fps = %d\n", __func__,
          pGC02M1BCtx->CurrFps);

    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);
    return (result);
}

static RESULT GC02M1B_IsiActivateTestPattern(IsiSensorHandle_t handle,
                        const bool_t enable)
{
    RESULT result = RET_SUCCESS;

    TRACE(GC02M1B_INFO, "%s: (enter)\n", __func__);

    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL || pGC02M1BCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    if (pGC02M1BCtx->Configured != BOOL_TRUE)
        return RET_WRONG_STATE;

    if (BOOL_TRUE == enable) {
        //result = GC02M1B_IsiRegisterWriteIss(handle, 0x3253, 0x80);
    } else {
        //result = GC02M1B_IsiRegisterWriteIss(handle, 0x3253, 0x00);
    }
    pGC02M1BCtx->TestPattern = enable;

    TRACE(GC02M1B_INFO, "%s: (exit)\n", __func__);

    return (result);
}

static RESULT GC02M1B_IsiSensorSetBlcIss(IsiSensorHandle_t handle, sensor_blc_t * pblc)
{
    int32_t ret = 0;
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL || pGC02M1BCtx->IsiCtx.HalHandle == NULL) {
        return RET_WRONG_HANDLE;
    }

    if (pblc == NULL)
        return RET_NULL_POINTER;

    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_BLC, pblc);
    if (ret != 0)
    {
         TRACE(GC02M1B_ERROR, "%s: set wb error\n", __func__);
    }

    return RET_SUCCESS;
}

static RESULT GC02M1B_IsiSensorSetWBIss(IsiSensorHandle_t handle, sensor_white_balance_t * pwb)
{
    int32_t ret = 0;
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL || pGC02M1BCtx->IsiCtx.HalHandle == NULL) {
        return RET_WRONG_HANDLE;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    if (pwb == NULL)
        return RET_NULL_POINTER;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_WB, pwb);
    if (ret != 0)
    {
         TRACE(GC02M1B_ERROR, "%s: set wb error\n", __func__);
    }

    return RET_SUCCESS;
}

static RESULT GC02M1B_IsiGetSensorAWBModeIss(IsiSensorHandle_t  handle, IsiSensorAwbMode_t *pawbmode)
{
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL || pGC02M1BCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    if (pGC02M1BCtx->SensorMode.hdr_mode == SENSOR_MODE_HDR_NATIVE){
        *pawbmode = ISI_SENSOR_AWB_MODE_SENSOR;
    }else{
        *pawbmode = ISI_SENSOR_AWB_MODE_NORMAL;
    }
    return RET_SUCCESS;
}

static RESULT GC02M1B_IsiSensorGetExpandCurveIss(IsiSensorHandle_t handle, sensor_expand_curve_t * pexpand_curve)
{
    int32_t ret = 0;
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;
    if (pGC02M1BCtx == NULL || pGC02M1BCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pGC02M1BCtx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_EXPAND_CURVE, pexpand_curve);
    if (ret != 0)
    {
        TRACE(GC02M1B_ERROR, "%s: get  expand cure error\n", __func__);
        return RET_FAILURE;
    }

    return RET_SUCCESS;
}

static RESULT GC02M1B_IsiGetCapsIss(IsiSensorHandle_t handle,
                         IsiSensorCaps_t * pIsiSensorCaps)
{
    GC02M1B_Context_t *pGC02M1BCtx = (GC02M1B_Context_t *) handle;

    RESULT result = RET_SUCCESS;

    TRACE(GC02M1B_INFO, "%s (enter)\n", __func__);

    if (pGC02M1BCtx == NULL) {
        return (RET_WRONG_HANDLE);
    }

    if (pIsiSensorCaps == NULL) {
        return (RET_NULL_POINTER);
    }

    pIsiSensorCaps->BusWidth = pGC02M1BCtx->SensorMode.bit_width;
    pIsiSensorCaps->Mode = ISI_MODE_BAYER;
    pIsiSensorCaps->FieldSelection = ISI_FIELDSEL_BOTH;
    pIsiSensorCaps->YCSequence = ISI_YCSEQ_YCBYCR;
    pIsiSensorCaps->Conv422 = ISI_CONV422_NOCOSITED;
    pIsiSensorCaps->BPat = pGC02M1BCtx->SensorMode.bayer_pattern;
    pIsiSensorCaps->HPol = ISI_HPOL_REFPOS;
    pIsiSensorCaps->VPol = ISI_VPOL_NEG;
    pIsiSensorCaps->Edge = ISI_EDGE_RISING;
    pIsiSensorCaps->Resolution.width = pGC02M1BCtx->SensorMode.width;
    pIsiSensorCaps->Resolution.height = pGC02M1BCtx->SensorMode.height;
    pIsiSensorCaps->SmiaMode = ISI_SMIA_OFF;
    pIsiSensorCaps->MipiLanes = ISI_MIPI_2LANES;

    if (pIsiSensorCaps->BusWidth == 10) {
        pIsiSensorCaps->MipiMode      = ISI_MIPI_MODE_RAW_10;
    }else if (pIsiSensorCaps->BusWidth == 12){
        pIsiSensorCaps->MipiMode      = ISI_MIPI_MODE_RAW_12;
    }else{
        pIsiSensorCaps->MipiMode      = ISI_MIPI_OFF;
    }

    TRACE(GC02M1B_INFO, "%s (exit)\n", __func__);
    return result;
}

RESULT GC02M1B_IsiGetSensorIss(IsiSensor_t *pIsiSensor)
{
    RESULT result = RET_SUCCESS;
    TRACE( GC02M1B_INFO, "%s (enter)\n", __func__);

    if ( pIsiSensor != NULL ) {
        pIsiSensor->pszName                         = SensorName;
        pIsiSensor->pIsiCreateSensorIss             = GC02M1B_IsiCreateSensorIss;

        pIsiSensor->pIsiInitSensorIss               = GC02M1B_IsiInitSensorIss;
        pIsiSensor->pIsiGetSensorModeIss            = GC02M1B_IsiGetSensorModeIss;
        pIsiSensor->pIsiResetSensorIss              = GC02M1B_IsiResetSensorIss;
        pIsiSensor->pIsiReleaseSensorIss            = GC02M1B_IsiReleaseSensorIss;
        pIsiSensor->pIsiGetCapsIss                  = GC02M1B_IsiGetCapsIss;
        pIsiSensor->pIsiSetupSensorIss              = GC02M1B_IsiSetupSensorIss;
        pIsiSensor->pIsiChangeSensorResolutionIss   = GC02M1B_IsiChangeSensorResolutionIss;
        pIsiSensor->pIsiSensorSetStreamingIss       = GC02M1B_IsiSensorSetStreamingIss;
        pIsiSensor->pIsiSensorSetPowerIss           = GC02M1B_IsiSensorSetPowerIss;
        pIsiSensor->pIsiCheckSensorConnectionIss    = GC02M1B_IsiCheckSensorConnectionIss;
        pIsiSensor->pIsiGetSensorRevisionIss        = GC02M1B_IsiGetSensorRevisionIss;
        pIsiSensor->pIsiRegisterReadIss             = GC02M1B_IsiRegisterReadIss;
        pIsiSensor->pIsiRegisterWriteIss            = GC02M1B_IsiRegisterWriteIss;

        /* AEC functions */
        pIsiSensor->pIsiExposureControlIss          = GC02M1B_IsiExposureControlIss;
        pIsiSensor->pIsiGetGainLimitsIss            = GC02M1B_IsiGetGainLimitsIss;
        pIsiSensor->pIsiGetIntegrationTimeLimitsIss = GC02M1B_IsiGetIntegrationTimeLimitsIss;
        pIsiSensor->pIsiGetCurrentExposureIss       = GC02M1B_IsiGetCurrentExposureIss;
        pIsiSensor->pIsiGetVSGainIss                    = GC02M1B_IsiGetVSGainIss;
        pIsiSensor->pIsiGetGainIss                      = GC02M1B_IsiGetGainIss;
        pIsiSensor->pIsiGetLongGainIss                  = GC02M1B_IsiGetLongGainIss;
        pIsiSensor->pIsiGetGainIncrementIss             = GC02M1B_IsiGetGainIncrementIss;
        pIsiSensor->pIsiSetGainIss                      = GC02M1B_IsiSetGainIss;
        pIsiSensor->pIsiGetIntegrationTimeIss           = GC02M1B_IsiGetIntegrationTimeIss;
        pIsiSensor->pIsiGetVSIntegrationTimeIss         = GC02M1B_IsiGetVSIntegrationTimeIss;
        pIsiSensor->pIsiGetLongIntegrationTimeIss       = GC02M1B_IsiGetLongIntegrationTimeIss;
        pIsiSensor->pIsiGetIntegrationTimeIncrementIss  = GC02M1B_IsiGetIntegrationTimeIncrementIss;
        pIsiSensor->pIsiSetIntegrationTimeIss           = GC02M1B_IsiSetIntegrationTimeIss;
        pIsiSensor->pIsiQuerySensorIss                  = GC02M1B_IsiQuerySensorIss;
        pIsiSensor->pIsiGetResolutionIss                = GC02M1B_IsiGetResolutionIss;
        pIsiSensor->pIsiGetSensorFpsIss                 = GC02M1B_IsiGetSensorFpsIss;
        pIsiSensor->pIsiSetSensorFpsIss                 = GC02M1B_IsiSetSensorFpsIss;
        pIsiSensor->pIsiSensorGetExpandCurveIss         = GC02M1B_IsiSensorGetExpandCurveIss;

        /* AWB specific functions */

        /* Testpattern */
        pIsiSensor->pIsiActivateTestPattern         = GC02M1B_IsiActivateTestPattern;
        pIsiSensor->pIsiSetBayerPattern             = GC02M1B_IsiSetBayerPattern;

        pIsiSensor->pIsiSensorSetBlcIss             = GC02M1B_IsiSensorSetBlcIss;
        pIsiSensor->pIsiSensorSetWBIss              = GC02M1B_IsiSensorSetWBIss;
        pIsiSensor->pIsiGetSensorAWBModeIss         = GC02M1B_IsiGetSensorAWBModeIss;

    } else {
        result = RET_NULL_POINTER;
    }

    TRACE( GC02M1B_INFO, "%s (exit)\n", __func__);
    return ( result );
}

/*****************************************************************************
* each sensor driver need declare this struct for isi load
*****************************************************************************/
IsiCamDrvConfig_t GC02M1B_IsiCamDrvConfig = {
    0,
    GC02M1B_IsiQuerySensorSupportIss,
    GC02M1B_IsiGetSensorIss,
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
