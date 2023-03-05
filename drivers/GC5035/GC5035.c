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
#include "GC5035_priv.h"
#include "gc5035.h"

CREATE_TRACER( GC5035_INFO , "GC5035: ", INFO,    0);
CREATE_TRACER( GC5035_WARN , "GC5035: ", WARNING, 0);
CREATE_TRACER( GC5035_ERROR, "GC5035: ", ERROR,   0);
CREATE_TRACER( GC5035_DEBUG,     "GC5035: ", INFO, 0);
CREATE_TRACER( GC5035_REG_INFO , "GC5035: ", INFO, 0);
CREATE_TRACER( GC5035_REG_DEBUG, "GC5035: ", INFO, 0);

#ifdef SUBDEV_V4L2
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#undef TRACE
#define TRACE(x, ...)
#endif

#define GC5035_MIN_GAIN_STEP    ( 1.0f/16.0f )  /**< min gain step size used by GUI (hardware min = 1/16; 1/16..32/16 depending on actual gain ) */
#define GC5035_MAX_GAIN_AEC     ( 32.0f )       /**< max. gain used by the AEC (arbitrarily chosen, hardware limit = 62.0, driver limit = 32.0 ) */
#define GC5035_VS_MAX_INTEGRATION_TIME (0.0018)

/*****************************************************************************
 *Sensor Info
*****************************************************************************/
static const char SensorName[16] = "GC5035";

static struct vvcam_mode_info pgc5035_mode_info[] = {
    {
        .index     = 0,
        .width     = 640,
        .height    = 480,
        .fps       = 30,
        .hdr_mode  = SENSOR_MODE_LINEAR, // SENSOR_MODE_LINEAR
        .bit_width = 10,
        .bayer_pattern = BAYER_GRBG,
        .mipi_phy_freq = 438, //mbps
        .mipi_line_num = 2,
        .config_file_3a = "GC5035_640x480_raw10",  //3aconfig_GC5035_640x480_raw10.json
        .preg_data = (void *)"gc5035 sensor liner mode 640*480@30",
    },
    {
        .index     = 1,
        .width     = 1920,
        .height    = 1080,
        .fps       = 30,
        .hdr_mode  = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_RGGB,
        .mipi_phy_freq = 876, //mbps
        .mipi_line_num = 2,
        .config_file_3a = "GC5035_1920x1080_raw10",  //3aconfig_GC5035_1920x1080_raw10.json
        .preg_data = (void *)"gc5035 sensor liner mode 1920*1080@30",
    },
    {
        .index     = 2,
        .width     = 2592,
        .height    = 1944,
        .fps       = 30,
        .hdr_mode  = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_RGGB,
        .mipi_phy_freq = 876, //mbps
        .mipi_line_num = 2,
        .config_file_3a = "GC5035_2592x1944_raw10",  //3aconfig_GC5035_2592x1944_raw10.json		
        .preg_data = (void *)"gc5035 sensor liner mode 2592*1944@30",
    },
    {
        .index     = 3,
        .width     = 1296,
        .height    = 972,
        .fps       = 30,
        .hdr_mode  = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_RGGB,
        .mipi_phy_freq = 438,
        .mipi_line_num = 2,  
        .config_file_3a = "GC5035_1296x972_raw10",  //3aconfig_GC5035_1296x972_raw10.json		
        .preg_data = (void *)"gc5035 sensor liner mode 1296*972@30",
    },
    {
        .index     = 4,
        .width = 1280,
        .height = 720,
        .fps      = 30,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_RGGB,
        .mipi_phy_freq = 438, //COULD BE WRONG
        .mipi_line_num = 2,   
        .config_file_3a = "GC5035_1280x720_raw10",  //3aconfig_GC5035_1280x720_raw10.json		
        .preg_data = (void *)"gc5035 sensor liner mode 1280*720@30",
    },
#if 0
    {
        .index     = 5,
        .width = 1280,
        .height = 720,
        .fps      = 60,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_RGGB,
        .mipi_phy_freq = 438, //COULD BE WRONG
        .mipi_line_num = 2,    
        .config_file_3a = "GC5035_1280x720_raw10",  //3aconfig_GC5035_1280x720_raw10.json		
        .preg_data = (void *)"gc5035 sensor liner mode 1280*720@60",
    }
#endif
};

static RESULT GC5035_IsiRegisterWriteIss(IsiSensorHandle_t handle, const uint32_t address, const uint32_t value);

static RESULT GC5035_IsiSensorSetPowerIss(IsiSensorHandle_t handle, bool_t on) {
    RESULT result = RET_SUCCESS;

    int ret = 0;
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL || pGC5035Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    TRACE(GC5035_INFO, "%s (enter)\n", __func__);

    int32_t enable = on;
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_POWER, &enable);
    if (ret != 0) {
        // to do
        //TRACE(GC5035_ERROR, "%s: sensor set power error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(GC5035_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT GC5035_IsiResetSensorIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;
    int ret = 0;

    TRACE(GC5035_INFO, "%s (enter)\n", __func__);

    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL || pGC5035Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_RESET, NULL);
    if (ret != 0) {
        TRACE(GC5035_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }

    TRACE(GC5035_INFO, "%s (exit)\n", __func__);
    return (result);
}

#ifdef SUBDEV_CHAR
static RESULT GC5035_IsiSensorSetClkIss(IsiSensorHandle_t handle, uint32_t clk) {
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;

    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL || pGC5035Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    TRACE(GC5035_INFO, "%s (enter)\n", __func__);

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_CLK, &clk);
    if (ret != 0) {
        // to do
        //TRACE(GC5035_ERROR, "%s: sensor set clk error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(GC5035_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT GC5035_IsiSensorGetClkIss
    (IsiSensorHandle_t handle, uint32_t * pclk) {
    RESULT result = RET_SUCCESS;
    int ret = 0;

    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL || pGC5035Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    TRACE(GC5035_INFO, "%s (enter)\n", __func__);

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_CLK, pclk);
    if (ret != 0) {
        // to do
        //TRACE(GC5035_ERROR, "%s: sensor get clk error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(GC5035_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT GC5035_IsiConfigSensorSCCBIss(IsiSensorHandle_t handle)
{
    return RET_SUCCESS;
}
#endif

static RESULT GC5035_IsiRegisterReadIss
    (IsiSensorHandle_t handle, const uint32_t address, uint32_t * p_value) {
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    TRACE(GC5035_INFO, "%s (enter)\n", __func__);

    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL || pGC5035Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    struct vvcam_sccb_data sccb_data;
    sccb_data.addr = address;
    sccb_data.data = 0;
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_READ_REG, &sccb_data);
    if (ret != 0) {
        TRACE(GC5035_ERROR, "%s: read sensor register error!\n",
              __func__);
        return (RET_FAILURE);
    }

    *p_value = sccb_data.data;

    TRACE(GC5035_INFO, "%s (exit) result = %d\n", __func__, result);
    return (result);
}

static RESULT GC5035_IsiRegisterWriteIss
    (IsiSensorHandle_t handle, const uint32_t address, const uint32_t value) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    TRACE(GC5035_INFO, "%s (enter)\n", __func__);

    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL || pGC5035Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    struct vvcam_sccb_data sccb_data;
    sccb_data.addr = address;
    sccb_data.data = value;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_WRITE_REG, &sccb_data);
    if (ret != 0) {
        TRACE(GC5035_ERROR, "%s: write sensor register error!\n",
              __func__);
        return (RET_FAILURE);
    }

    TRACE(GC5035_INFO, "%s (exit) result = %d\n", __func__, result);
    return (result);
}

static RESULT GC5035_IsiQuerySensorSupportIss(HalHandle_t  HalHandle, vvcam_mode_info_array_t *pSensorSupportInfo)
{
    TRACE(GC5035_DEBUG, "enter %s", __func__);
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
        TRACE(GC5035_ERROR, "%s: sensor kernel query error! Use isi query\n",__func__);
        psensor_mode_info_arry->count = sizeof(pgc5035_mode_info) / sizeof(struct vvcam_mode_info);
        memcpy(psensor_mode_info_arry->modes, pgc5035_mode_info, sizeof(pgc5035_mode_info));
    }
#endif
    psensor_mode_info_arry->count = sizeof(pgc5035_mode_info) / sizeof(struct vvcam_mode_info);
    memcpy(psensor_mode_info_arry->modes, pgc5035_mode_info, sizeof(pgc5035_mode_info));
    
    TRACE(GC5035_DEBUG, "%s-%s-%d: cnt=%d\n", __FILE__, __func__, __LINE__, psensor_mode_info_arry->count);

    return RET_SUCCESS;
}

static  RESULT GC5035_IsiQuerySensorIss(IsiSensorHandle_t handle, vvcam_mode_info_array_t *pSensorInfo)
{
    RESULT result = RET_SUCCESS;
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL || pGC5035Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;
    GC5035_IsiQuerySensorSupportIss(pHalCtx,pSensorInfo);

    return result;
}

static RESULT GC5035_IsiGetSensorModeIss(IsiSensorHandle_t handle,void *mode)
{
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL) {
        return (RET_WRONG_HANDLE);
    }
    memcpy(mode,&(pGC5035Ctx->SensorMode), sizeof(pGC5035Ctx->SensorMode));

    return ( RET_SUCCESS );
}

static RESULT GC5035_IsiCreateSensorIss(IsiSensorInstanceConfig_t * pConfig) {
    RESULT result = RET_SUCCESS;
    GC5035_Context_t *pGC5035Ctx;

    TRACE(GC5035_INFO, "%s (enter)\n", __func__);

    if (!pConfig || !pConfig->pSensor)
        return (RET_NULL_POINTER);

    pGC5035Ctx = (GC5035_Context_t *) malloc(sizeof(GC5035_Context_t));
    if (!pGC5035Ctx) {
        TRACE(GC5035_ERROR, "%s: Can't allocate gc5035 context\n",
              __func__);
        return (RET_OUTOFMEM);
    }

    MEMSET(pGC5035Ctx, 0, sizeof(GC5035_Context_t));

    result = HalAddRef(pConfig->HalHandle);
    if (result != RET_SUCCESS) {
        free(pGC5035Ctx);
        return (result);
    }

    pGC5035Ctx->IsiCtx.HalHandle = pConfig->HalHandle;
    pGC5035Ctx->IsiCtx.pSensor = pConfig->pSensor;
    pGC5035Ctx->GroupHold = BOOL_FALSE;
    pGC5035Ctx->OldGain = 0;
    pGC5035Ctx->OldIntegrationTime = 0;
    pGC5035Ctx->Configured = BOOL_FALSE;
    pGC5035Ctx->Streaming = BOOL_FALSE;
    pGC5035Ctx->TestPattern = BOOL_FALSE;
    pGC5035Ctx->isAfpsRun = BOOL_FALSE;
    pGC5035Ctx->SensorMode.index = pConfig->SensorModeIndex;
    pConfig->hSensor = (IsiSensorHandle_t) pGC5035Ctx;
#ifdef SUBDEV_CHAR
    struct vvcam_mode_info *SensorDefaultMode = NULL;
    for (int i=0; i < sizeof(pgc5035_mode_info)/ sizeof(struct vvcam_mode_info); i++)
    {
        if (pgc5035_mode_info[i].index == pGC5035Ctx->SensorMode.index)
        {
            SensorDefaultMode = &(pgc5035_mode_info[i]);
            break;
        }
    }

    if (SensorDefaultMode != NULL)
    {
        strcpy(pGC5035Ctx->SensorRegCfgFile, get_vi_config_path());
        switch(SensorDefaultMode->index)
        {
            case 0:
                strcat(pGC5035Ctx->SensorRegCfgFile,
                    "GC5035_mipi2lane_640x480@30_gc.txt");
                break;
            case 1:
                strcat(pGC5035Ctx->SensorRegCfgFile,
                    "GC5035_mipi2lane_1920x1080@30_gc.txt");
                break;
            case 2: 
                strcat(pGC5035Ctx->SensorRegCfgFile,
                    "GC5035_mipi2lane_2592x1944@30_gc.txt");
                break;
            case 3: 
                strcat(pGC5035Ctx->SensorRegCfgFile,
                    "GC5035_mipi2lane_1296x972@30_mayi.txt");
                break;
            case 4: //720p@30fps
                strcat(pGC5035Ctx->SensorRegCfgFile,
                    "GC5035_mipi2lane_1280x720@30_gc.txt");
                break;
            case 5: //720p@60fps
                strcat(pGC5035Ctx->SensorRegCfgFile,
                    "GC5035_mipi2lane_1280x720@60_mayi.txt");
                break;
            default:
                break;
        }

        if (access(pGC5035Ctx->SensorRegCfgFile, F_OK) == 0) {
            pGC5035Ctx->KernelDriverFlag = 0;
            memcpy(&(pGC5035Ctx->SensorMode),SensorDefaultMode,sizeof(struct vvcam_mode_info));
        } else {
            pGC5035Ctx->KernelDriverFlag = 1;
        }
    }else
    {
        pGC5035Ctx->KernelDriverFlag = 1;
    }

    result = GC5035_IsiSensorSetPowerIss(pGC5035Ctx, BOOL_TRUE);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    uint32_t SensorClkIn = 0;
    if (pGC5035Ctx->KernelDriverFlag) {
        result = GC5035_IsiSensorGetClkIss(pGC5035Ctx, &SensorClkIn);
        RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);
    }

    result = GC5035_IsiSensorSetClkIss(pGC5035Ctx, SensorClkIn);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    result = GC5035_IsiResetSensorIss(pGC5035Ctx);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    pGC5035Ctx->pattern = ISI_BPAT_BGBGGRGR;

    if (!pGC5035Ctx->KernelDriverFlag) {
        result = GC5035_IsiConfigSensorSCCBIss(pGC5035Ctx);
        RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);
    }
#endif

    TRACE(GC5035_INFO, "%s (exit pConfig->hSensor = %p)\n", __func__, pConfig->hSensor);
    return (result);
}

static RESULT GC5035_IsiGetRegCfgIss(const char *registerFileName,
                     struct vvcam_sccb_array *arry)
{
    if (NULL == registerFileName) {
        TRACE(GC5035_ERROR, "%s:registerFileName is NULL\n", __func__);
        return (RET_NULL_POINTER);
    }
#ifdef SUBDEV_CHAR
    FILE *fp = NULL;
    fp = fopen(registerFileName, "rb");
    if (!fp) {
        TRACE(GC5035_ERROR, "%s:load register file  %s error!\n",
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
        TRACE(GC5035_ERROR, "%s:malloc failed NULL Point!\n", __func__,
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

static RESULT GC5035_IsiInitSensorIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;

    int ret = 0;
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;

    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;
    TRACE(GC5035_INFO, "%s (enter handle = %p)\n", __func__, handle);

    if (pGC5035Ctx == NULL) {
        return (RET_WRONG_HANDLE);
    }
	TRACE(GC5035_INFO, "%s (pGC5035Ctx->KernelDriverFlag = %d)\n", __func__, pGC5035Ctx->KernelDriverFlag);
    if (pGC5035Ctx->KernelDriverFlag) {
        ;
    } else {
		TRACE(GC5035_INFO, "%s (001)\n", __func__);
        struct vvcam_sccb_array arry;
        result = GC5035_IsiGetRegCfgIss(pGC5035Ctx->SensorRegCfgFile, &arry);
        if (result != 0) {
            TRACE(GC5035_ERROR,
                  "%s:GC5035_IsiGetRegCfgIss error!\n", __func__);
            return (RET_FAILURE);
        }

        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_WRITE_ARRAY, &arry);
        if (ret != 0) {
            TRACE(GC5035_ERROR, "%s:Sensor Write Reg arry error!\n",
                  __func__);
            return (RET_FAILURE);
        }
		TRACE(GC5035_INFO, "%s (pGC5035Ctx->SensorMode.index = %d)\n", __func__, pGC5035Ctx->SensorMode.index);
        switch(pGC5035Ctx->SensorMode.index)
        {
            case 0: // 480p
                pGC5035Ctx->one_line_exp_time = 0.0000167; // line_time = line_length / pclk =1460/87.6mhz = 0.0000167
                pGC5035Ctx->FrameLengthLines = 0x7cc; //framelength=1996=0x7cc
                pGC5035Ctx->CurFrameLengthLines = pGC5035Ctx->FrameLengthLines;
                pGC5035Ctx->MaxIntegrationLine = pGC5035Ctx->CurFrameLengthLines - 8;
                pGC5035Ctx->MinIntegrationLine = 1;
                pGC5035Ctx->AecMaxGain = 16;
                pGC5035Ctx->AecMinGain = 1;
                break;
            case 1: // 1080p
                pGC5035Ctx->one_line_exp_time = 0.0000167; // line_time = line_length / pclk =2920/175.2mhz = 0.00001667
                pGC5035Ctx->FrameLengthLines = 0x7D8; //framelength=2008=0x7D8
                pGC5035Ctx->CurFrameLengthLines = pGC5035Ctx->FrameLengthLines;
                pGC5035Ctx->MaxIntegrationLine = pGC5035Ctx->CurFrameLengthLines - 8;
                pGC5035Ctx->MinIntegrationLine = 1;
                pGC5035Ctx->AecMaxGain = 16;
                pGC5035Ctx->AecMinGain = 1;
                break;
            case 2: // full size
                pGC5035Ctx->one_line_exp_time = 0.0000167; // line_time = line_length / pclk =2920/175.2mhz = 0.00001667
                pGC5035Ctx->FrameLengthLines = 0x7D8; //framelength=2008=0x7D8
                pGC5035Ctx->CurFrameLengthLines = pGC5035Ctx->FrameLengthLines;
                pGC5035Ctx->MaxIntegrationLine = pGC5035Ctx->CurFrameLengthLines - 8;
                pGC5035Ctx->MinIntegrationLine = 1;
                pGC5035Ctx->AecMaxGain = 16;
                pGC5035Ctx->AecMinGain = 1;
                break;
            case 3: // 1296x972
                pGC5035Ctx->one_line_exp_time = 0.0000167; // line_time = line_length / pclk =2920/175.2mhz = 0.00001667
                pGC5035Ctx->FrameLengthLines = 0x7D8; //framelength=2008=0x7D8
                pGC5035Ctx->CurFrameLengthLines = pGC5035Ctx->FrameLengthLines;
                pGC5035Ctx->MaxIntegrationLine = pGC5035Ctx->CurFrameLengthLines - 8;
                pGC5035Ctx->MinIntegrationLine = 1;
                pGC5035Ctx->AecMaxGain = 16;
                pGC5035Ctx->AecMinGain = 1;
                break;
            case 4: // 720p@30fps
            case 5: // 720p@60fps
                pGC5035Ctx->one_line_exp_time = 0.0000167; // line_time = line_length / pclk =2920/175.2mhz = 0.00001667
                pGC5035Ctx->FrameLengthLines = 0x7D8; //framelength=2008=0x7D8
                pGC5035Ctx->CurFrameLengthLines = pGC5035Ctx->FrameLengthLines;
                pGC5035Ctx->MaxIntegrationLine = pGC5035Ctx->CurFrameLengthLines - 8;
                pGC5035Ctx->MinIntegrationLine = 1;
                pGC5035Ctx->AecMaxGain = 16;
                pGC5035Ctx->AecMinGain = 1;
                break;
            default:
                return (RET_FAILURE);
        }
		pGC5035Ctx->AecIntegrationTimeIncrement = pGC5035Ctx->one_line_exp_time;
		pGC5035Ctx->AecMinIntegrationTime =
			pGC5035Ctx->one_line_exp_time * pGC5035Ctx->MinIntegrationLine;
		pGC5035Ctx->AecMaxIntegrationTime =
			pGC5035Ctx->one_line_exp_time * pGC5035Ctx->MaxIntegrationLine;


        pGC5035Ctx->MaxFps  = pGC5035Ctx->SensorMode.fps;
        pGC5035Ctx->MinFps  = 1;
        pGC5035Ctx->CurrFps = pGC5035Ctx->MaxFps;
    }
    TRACE(GC5035_INFO, "%s (pGC5035Ctx->one_line_exp_time = %f)\n", __func__, pGC5035Ctx->one_line_exp_time);
    TRACE(GC5035_INFO, "%s (pGC5035Ctx->MinIntegrationLine = %d, pGC5035Ctx->MaxIntegrationLine = %d)\n", __func__, pGC5035Ctx->MinIntegrationLine, pGC5035Ctx->MaxIntegrationLine);
    return (result);
}

static RESULT GC5035_IsiReleaseSensorIss(IsiSensorHandle_t handle) {
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(GC5035_INFO, "%s (enter)\n", __func__);

    if (pGC5035Ctx == NULL)
        return (RET_WRONG_HANDLE);

    (void)GC5035_IsiSensorSetStreamingIss(pGC5035Ctx, BOOL_FALSE);
    (void)GC5035_IsiSensorSetPowerIss(pGC5035Ctx, BOOL_FALSE);
    (void)HalDelRef(pGC5035Ctx->IsiCtx.HalHandle);

    MEMSET(pGC5035Ctx, 0, sizeof(GC5035_Context_t));
    free(pGC5035Ctx);
    TRACE(GC5035_INFO, "%s (exit)\n", __func__);
    return (result);
}

struct gc5035_fmt {
    int width;
    int height;
    int fps;
};

static RESULT GC5035_IsiSetupSensorIss
    (IsiSensorHandle_t handle, const IsiSensorConfig_t * pConfig) {

    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    RESULT result = RET_SUCCESS;

    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (!pGC5035Ctx) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pConfig) {
        TRACE(GC5035_ERROR,
              "%s: Invalid configuration (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    if (pGC5035Ctx->Streaming != BOOL_FALSE) {
        return RET_WRONG_STATE;
    }

    memcpy(&pGC5035Ctx->Config, pConfig, sizeof(IsiSensorConfig_t));

    /* 1.) SW reset of image sensor (via I2C register interface)  be careful, bits 6..0 are reserved, reset bit is not sticky */
    TRACE(GC5035_DEBUG, "%s: GC5035 System-Reset executed\n", __func__);
    osSleep(100);

    //GC5035_AecSetModeParameters not defined yet as of 2021/8/9.
    //result = GC5035_AecSetModeParameters(pGC5035Ctx, pConfig);
    //if (result != RET_SUCCESS) {
    //    TRACE(GC5035_ERROR, "%s: SetupOutputWindow failed.\n",
    //          __func__);
    //    return (result);
    //}
#if 1
    struct gc5035_fmt fmt;
    fmt.width = pConfig->Resolution.width;
    fmt.height = pConfig->Resolution.height;

    ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fmt);//result = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fmt);
#endif
    pGC5035Ctx->Configured = BOOL_TRUE;
    TRACE(GC5035_INFO, "%s: (exit) ret=0x%x \n", __func__, result);
    return result;
}

static RESULT GC5035_IsiChangeSensorResolutionIss(IsiSensorHandle_t handle, uint16_t width, uint16_t height) {
    RESULT result = RET_SUCCESS;
#if 0
    struct gc5035_fmt fmt;
    fmt.width = width;
    fmt.height = height;

    int ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fmt);
#endif
    TRACE(GC5035_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT GC5035_IsiSensorSetStreamingIss
    (IsiSensorHandle_t handle, bool_t on) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    TRACE(GC5035_INFO, "%s (enter)\n", __func__);

    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL || pGC5035Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    if (pGC5035Ctx->Configured != BOOL_TRUE)
        return RET_WRONG_STATE;

    int32_t enable = (uint32_t) on;
    ret = GC5035_IsiRegisterWriteIss(handle, 0xfe, 0x00);
    if (ret != 0) {
        return (RET_FAILURE);
    }

    if (on == true) {
        ret = GC5035_IsiRegisterWriteIss(handle, 0x3e, 0x91);
    } else {
        ret = GC5035_IsiRegisterWriteIss(handle, 0x3e, 0x01);
    }

    if (ret != 0) {
        return (RET_FAILURE);
    }

    pGC5035Ctx->Streaming = on;

    TRACE(GC5035_INFO, "%s (exit)\n", __func__);
    return (result);
}

static int32_t sensor_get_chip_id(IsiSensorHandle_t handle, uint32_t *chip_id)
{
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    int32_t chip_id_high = 0;
    int32_t chip_id_low = 0;

    ret = GC5035_IsiRegisterReadIss(handle, 0xf0, &chip_id_high);
    if (ret != 0) {
        TRACE(GC5035_ERROR,
            "%s: Read Sensor correct ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    ret = GC5035_IsiRegisterReadIss(handle, 0xf1, &chip_id_low);
    if (ret != 0) {
        TRACE(GC5035_ERROR,
            "%s: Read Sensor correct ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    *chip_id = ((chip_id_high & 0xff)<<8) | (chip_id_low & 0xff);

    return 0;
}

static RESULT GC5035_IsiCheckSensorConnectionIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    uint32_t correct_id = 0x5035;
    uint32_t sensor_id = 0;

    TRACE(GC5035_INFO, "%s (enter)\n", __func__);

    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL || pGC5035Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    ret = sensor_get_chip_id(handle, &sensor_id);
    if (ret != 0) {
        TRACE(GC5035_ERROR,
            "%s: Read Sensor chip ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    if (correct_id != sensor_id) {
        TRACE(GC5035_ERROR, "%s:ChipID =0x%x sensor_id=%x error! \n",
              __func__, correct_id, sensor_id);
        return (RET_FAILURE);
    }

    TRACE(GC5035_INFO,
          "%s ChipID = 0x%08x, sensor_id = 0x%08x, success! \n", __func__,
          correct_id, sensor_id);
    TRACE(GC5035_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT GC5035_IsiGetSensorRevisionIss
    (IsiSensorHandle_t handle, uint32_t * p_value) {
    RESULT result = RET_SUCCESS;
    TRACE(GC5035_INFO, "%s (enter)\n", __func__);

    *p_value = 0X5690;
    TRACE(GC5035_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT GC5035_IsiGetGainLimitsIss
    (IsiSensorHandle_t handle, float *pMinGain, float *pMaxGain) {
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    /*TODO*/

    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (pGC5035Ctx == NULL) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pMinGain == NULL) || (pMaxGain == NULL)) {
        TRACE(GC5035_ERROR, "%s: NULL pointer received!!\n");
        return (RET_NULL_POINTER);
    }

    *pMinGain = pGC5035Ctx->AecMinGain;
    *pMaxGain = pGC5035Ctx->AecMaxGain;

    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);
    return (result);
}

static RESULT GC5035_IsiGetIntegrationTimeLimitsIss
    (IsiSensorHandle_t handle,
     float *pMinIntegrationTime, float *pMaxIntegrationTime) {
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    /*TODO*/

    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);
    if (pGC5035Ctx == NULL) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pMinIntegrationTime == NULL) || (pMaxIntegrationTime == NULL)) {
        TRACE(GC5035_ERROR, "%s: NULL pointer received!!\n");
        return (RET_NULL_POINTER);
    }

    *pMinIntegrationTime = pGC5035Ctx->AecMinIntegrationTime;
    *pMaxIntegrationTime = pGC5035Ctx->AecMaxIntegrationTime;

    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC5035_IsiGetGainIss(IsiSensorHandle_t handle, float *pSetGain) {
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (pGC5035Ctx == NULL) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pSetGain == NULL) {
        return (RET_NULL_POINTER);
    }

    *pSetGain = pGC5035Ctx->AecCurGain;

    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC5035_IsiGetLongGainIss(IsiSensorHandle_t handle, float *gain)
{
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;

    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (pGC5035Ctx == NULL) {
        TRACE(GC5035_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }

    if (gain == NULL) {
        return (RET_NULL_POINTER);
    }

    *gain = pGC5035Ctx->AecCurLongGain;

    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);

    return (RET_SUCCESS);
}

RESULT GC5035_IsiGetVSGainIss(IsiSensorHandle_t handle, float *pSetGain) {
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (pGC5035Ctx == NULL) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pSetGain == NULL) {
        return (RET_NULL_POINTER);
    }

    *pSetGain = pGC5035Ctx->AecCurVSGain;

    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);

    return (result);
}

RESULT GC5035_IsiGetGainIncrementIss(IsiSensorHandle_t handle, float *pIncr) {
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (pGC5035Ctx == NULL) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pIncr == NULL)
        return (RET_NULL_POINTER);

    *pIncr = pGC5035Ctx->AecGainIncrement;

    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);

    return (result);
}

uint16_t GC5035_AGC_Param[17][2] = {
	{  256,  0 },
	{  302,  1 },
	{  358,  2 },
	{  425,  3 },
	{  502,  8 },
	{  599,  9 },
	{  717, 10 },
	{  845, 11 },
	{ 998,  12 },
	{ 1203, 13 },
	{ 1434, 14 },
	{ 1710, 15 },
	{ 1997, 16 },
	{ 2355, 17 },
	{ 2816, 18 },
	{ 3318, 19 },
	{ 3994, 20 },
};
static uint32_t Dgain_ratio = 256;

RESULT GC5035_IsiSetGainIss
    (IsiSensorHandle_t handle,
     float NewGain, float *pSetGain, float *hdr_ratio) {

    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    uint32_t temp_gain;
    uint16_t gain_index;

    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL || pGC5035Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    if (NewGain >= 16) { // More than 16 will not take effect
        NewGain = 16;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;
    //GC5035 specific
    uint32_t SensorGain = 0;
    SensorGain = NewGain * 256; // pGC5035Ctx->gain_accuracy;
	if (SensorGain < 256) {     //min gain=1x
		SensorGain = 256;
        NewGain = 1;
    } else if (SensorGain > 16*256)  {   //max gain=16x
        SensorGain = 16*256;
        NewGain = 16;
    }
	for (gain_index = 16; gain_index >= 0; gain_index--) {
		if (SensorGain >= GC5035_AGC_Param[gain_index][0])
			break;
    }
    ret = GC5035_IsiRegisterWriteIss(handle, 0xfe, 0x00);
    if (ret != 0) {
        return (RET_FAILURE);
    }
    ret = GC5035_IsiRegisterWriteIss(handle, 0xb6, GC5035_AGC_Param[gain_index][1]);
    if (ret != 0) {
        return (RET_FAILURE);
    }
	temp_gain = SensorGain*Dgain_ratio / GC5035_AGC_Param[gain_index][0];
    ret = GC5035_IsiRegisterWriteIss(handle, 0xb1, (temp_gain >> 8) & 0x0f);
    if (ret != 0) {
        return (RET_FAILURE);
    }
    ret = GC5035_IsiRegisterWriteIss(handle, 0xb2, temp_gain & 0xfc);
    if (ret != 0) {
        return (RET_FAILURE);
    }

    volatile int32_t reg;
    TRACE(GC5035_DEBUG, "%s gain_index=%d,[gain_index][0]=%d,Dgain_ratio=%u\n",__func__, gain_index, GC5035_AGC_Param[gain_index][0], Dgain_ratio);
    TRACE(GC5035_DEBUG, "%s temp_gain=0x%x,0xb1 write 0x%x,0xb2 write 0x%x\n",__func__, temp_gain, (temp_gain >> 8) & 0x0f, temp_gain & 0xfc);
    GC5035_IsiRegisterReadIss(handle, 0xb6, &reg);
    TRACE(GC5035_DEBUG, "%s 0xb6 read 0x0%x\n",__func__, reg);
    GC5035_IsiRegisterReadIss(handle, 0xb1, &reg);
    TRACE(GC5035_DEBUG, "%s 0xb1 read 0x0%x\n",__func__, reg);
    GC5035_IsiRegisterReadIss(handle, 0xb2, &reg);
    TRACE(GC5035_DEBUG, "%s 0xb2 read 0x0%x\n",__func__, reg);

    pGC5035Ctx->AecCurGain = ((float)(NewGain));
    *pSetGain = pGC5035Ctx->AecCurGain;
    TRACE(GC5035_DEBUG, "%s: g=%f\n", __func__, *pSetGain);
    return (result);
}

RESULT GC5035_IsiSetLongGainIss(IsiSensorHandle_t handle, float gain)
{
    int ret = 0;
    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;

    if (!pGC5035Ctx || !pGC5035Ctx->IsiCtx.HalHandle)
    {
        TRACE(GC5035_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }

    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    uint32_t SensorGain = 0;
    SensorGain = gain * pGC5035Ctx->gain_accuracy;
    if (pGC5035Ctx->LastLongGain != SensorGain)
    {

        /*TODO*/
#if 0
        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_LONG_GAIN, &SensorGain);
        if (ret != 0)
        {
            return (RET_FAILURE);
            TRACE(GC5035_ERROR,"%s: set long gain failed\n");

        }
#endif
        pGC5035Ctx->LastLongGain = SensorGain;
        pGC5035Ctx->AecCurLongGain = gain;
    }

    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT GC5035_IsiSetVSGainIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float NewGain, float *pSetGain, float *hdr_ratio) {
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;
    RESULT result = RET_SUCCESS;
#if 0
    float Gain = 0.0f;

    uint32_t ucGain = 0U;
    uint32_t again = 0U;
#endif

    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (!pGC5035Ctx) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetGain || !hdr_ratio)
        return (RET_NULL_POINTER);

    uint32_t SensorGain = 0;
    SensorGain = NewGain * pGC5035Ctx->gain_accuracy;

    /*TODO*/
    //ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_VSGAIN, &SensorGain);

    pGC5035Ctx->AecCurVSGain = NewGain;
    *pSetGain = pGC5035Ctx->AecCurGain;
    TRACE(GC5035_DEBUG, "%s: g=%f\n", __func__, *pSetGain);
    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC5035_IsiSetBayerPattern(IsiSensorHandle_t handle, uint8_t pattern)
{

    RESULT result = RET_SUCCESS;
#if 0
    uint8_t h_shift = 0, v_shift = 0;
    uint32_t val_h = 0, val_l = 0;
    uint16_t val = 0;
    uint8_t Start_p = 0;
    bool_t streaming_status;
    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL || pGC5035Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    // pattern 0:B 1:GB 2:GR 3:R
    streaming_status = pGC5035Ctx->Streaming;
    result = GC5035_IsiSensorSetStreamingIss(handle, 0);
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

    GC5035_IsiRegisterReadIss(handle, 0x30a0, &val_h);
    GC5035_IsiRegisterReadIss(handle, 0x30a1, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + h_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    GC5035_IsiRegisterWriteIss(handle, 0x30a0, (uint8_t)val_h);
    GC5035_IsiRegisterWriteIss(handle, 0x30a1, (uint8_t)val_l);

    GC5035_IsiRegisterReadIss(handle, 0x30a2, &val_h);
    GC5035_IsiRegisterReadIss(handle, 0x30a3, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + v_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    GC5035_IsiRegisterWriteIss(handle, 0x30a2, (uint8_t)val_h);
    GC5035_IsiRegisterWriteIss(handle, 0x30a3, (uint8_t)val_l);

    GC5035_IsiRegisterReadIss(handle, 0x30a4, &val_h);
    GC5035_IsiRegisterReadIss(handle, 0x30a5, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + h_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    GC5035_IsiRegisterWriteIss(handle, 0x30a4, (uint8_t)val_h);
    GC5035_IsiRegisterWriteIss(handle, 0x30a5, (uint8_t)val_l);

    GC5035_IsiRegisterReadIss(handle, 0x30a6, &val_h);
    GC5035_IsiRegisterReadIss(handle, 0x30a7, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + v_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    GC5035_IsiRegisterWriteIss(handle, 0x30a6, (uint8_t)val_h);
    GC5035_IsiRegisterWriteIss(handle, 0x30a7, (uint8_t)val_l);

    pGC5035Ctx->pattern = pattern;
    result = GC5035_IsiSensorSetStreamingIss(handle, streaming_status);
#endif

    return (result);
}

RESULT GC5035_IsiGetIntegrationTimeIss
    (IsiSensorHandle_t handle, float *pSetIntegrationTime)
{
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (!pGC5035Ctx) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetIntegrationTime)
        return (RET_NULL_POINTER);
    *pSetIntegrationTime = pGC5035Ctx->AecCurIntegrationTime;
    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC5035_IsiGetLongIntegrationTimeIss(IsiSensorHandle_t handle, float *pIntegrationTime)
{
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (!pGC5035Ctx) {
        TRACE(GC5035_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }
    if (!pIntegrationTime)
        return (RET_NULL_POINTER);

    pGC5035Ctx->AecCurLongIntegrationTime =  pGC5035Ctx->AecCurIntegrationTime;

    *pIntegrationTime = pGC5035Ctx->AecCurLongIntegrationTime;
    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT GC5035_IsiGetVSIntegrationTimeIss
    (IsiSensorHandle_t handle, float *pSetIntegrationTime)
{
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (!pGC5035Ctx) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    if (!pSetIntegrationTime)
        return (RET_NULL_POINTER);

    *pSetIntegrationTime = pGC5035Ctx->AecCurVSIntegrationTime;
    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC5035_IsiGetIntegrationTimeIncrementIss
    (IsiSensorHandle_t handle, float *pIncr)
{
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (!pGC5035Ctx) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pIncr)
        return (RET_NULL_POINTER);

    //_smallest_ increment the sensor/driver can handle (e.g. used for sliders in the application)
    *pIncr = pGC5035Ctx->AecIntegrationTimeIncrement;
    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC5035_IsiSetIntegrationTimeIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float *pSetIntegrationTime,
     uint8_t * pNumberOfFramesToSkip, float *hdr_ratio)
{
    RESULT result = RET_SUCCESS;

    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    uint32_t exp_line = 0;
    uint32_t cal_shutter = 0;
    uint32_t exp_line_old = 0;
    int ret = 0;

    TRACE(GC5035_INFO, "%s: (enter handle = %p)\n", __func__, handle);

    if (!pGC5035Ctx) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetIntegrationTime || !pNumberOfFramesToSkip) {
        TRACE(GC5035_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }
    exp_line = NewIntegrationTime / pGC5035Ctx->one_line_exp_time;
    exp_line_old = exp_line;
    exp_line =
        MIN(pGC5035Ctx->MaxIntegrationLine,
        MAX(pGC5035Ctx->MinIntegrationLine, exp_line));

    TRACE(GC5035_DEBUG, "%s: set AEC_PK_EXPO=0x%05x min_exp_line = %d, max_exp_line = %d\n", __func__, exp_line, pGC5035Ctx->MinIntegrationLine, pGC5035Ctx->MaxIntegrationLine);

    if (exp_line != pGC5035Ctx->OldIntegrationTime) {

        /*TODO*/
        //ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_EXP, &exp_line);
        pGC5035Ctx->OldIntegrationTime = exp_line;    // remember current integration time
        pGC5035Ctx->AecCurIntegrationTime =
            exp_line * pGC5035Ctx->one_line_exp_time;

        *pNumberOfFramesToSkip = 1U;    //skip 1 frame
    } else {
        *pNumberOfFramesToSkip = 0U;    //no frame skip
    }

    if (NewIntegrationTime > pGC5035Ctx->MaxIntegrationLine * pGC5035Ctx->one_line_exp_time)
        NewIntegrationTime = pGC5035Ctx->MaxIntegrationLine * pGC5035Ctx->one_line_exp_time;
    
    // GC5035 specific
    cal_shutter = exp_line >> 2;
    cal_shutter = cal_shutter << 2;//保证为4的整数倍
    if (cal_shutter != 0) {
        Dgain_ratio = 256 * exp_line / cal_shutter;
    }
    ret = GC5035_IsiRegisterWriteIss(handle, 0xfe, 0x00);
    if (ret != 0) {
        return (RET_FAILURE);
    }
    ret = GC5035_IsiRegisterWriteIss(handle, 0x03, (cal_shutter >> 8) & 0x3F);
    if (ret != 0) {
        return (RET_FAILURE);
    }
    ret = GC5035_IsiRegisterWriteIss(handle, 0x04, cal_shutter & 0xFF);
    if (ret != 0) {
        return (RET_FAILURE);
    }

    volatile int32_t reg;
    TRACE(GC5035_DEBUG, "%s exp_line = %fs / %fs = %d\n",__func__, NewIntegrationTime,  pGC5035Ctx->one_line_exp_time, exp_line);
    TRACE(GC5035_DEBUG, "%s cal_shutter=%d,Dgain_ratio=%u\n", __func__, cal_shutter, Dgain_ratio);
    TRACE(GC5035_DEBUG, "%s 0x03 write 0x%x, 0x04 write 0x%x\n", __func__, (cal_shutter >> 8) & 0x3F, cal_shutter & 0xFF);
    GC5035_IsiRegisterReadIss(handle, 0x03, &reg);
    TRACE(GC5035_DEBUG, "%s 0x03 read 0x0%x\n",__func__, reg);
    GC5035_IsiRegisterReadIss(handle, 0x04, &reg);
    TRACE(GC5035_DEBUG, "%s 0x04 read 0x0%x\n",__func__, reg);

    if (exp_line_old != exp_line) {
        *pSetIntegrationTime = pGC5035Ctx->AecCurIntegrationTime;
    } else {
        *pSetIntegrationTime = NewIntegrationTime;
    }

    TRACE(GC5035_DEBUG, "%s: Ti=%f\n", __func__, *pSetIntegrationTime);
    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC5035_IsiSetLongIntegrationTimeIss(IsiSensorHandle_t handle,float IntegrationTime)
{
    int ret;
    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (!handle || !pGC5035Ctx->IsiCtx.HalHandle)
    {
        TRACE(GC5035_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    uint32_t exp_line = 0;
    exp_line = IntegrationTime / pGC5035Ctx->one_line_exp_time;
    exp_line = MIN(pGC5035Ctx->MaxIntegrationLine, MAX(pGC5035Ctx->MinIntegrationLine, exp_line));

    if (exp_line != pGC5035Ctx->LastLongExpLine)
    {
        if (pGC5035Ctx->KernelDriverFlag)
        {
            ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_LONG_EXP, &exp_line);
            if (ret != 0)
            {
                TRACE(GC5035_ERROR,"%s: set long gain failed\n");
                return RET_FAILURE;
            }
        }

        pGC5035Ctx->LastLongExpLine = exp_line;
        pGC5035Ctx->AecCurLongIntegrationTime =  pGC5035Ctx->LastLongExpLine*pGC5035Ctx->one_line_exp_time;
    }


    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT GC5035_IsiSetVSIntegrationTimeIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float *pSetVSIntegrationTime,
     uint8_t * pNumberOfFramesToSkip, float *hdr_ratio)
{
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    uint32_t exp_line = 0;

    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (!pGC5035Ctx) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetVSIntegrationTime || !pNumberOfFramesToSkip) {
        TRACE(GC5035_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    TRACE(GC5035_INFO,
          "%s:  maxIntegrationTime-=%f minIntegrationTime = %f\n", __func__,
          pGC5035Ctx->AecMaxIntegrationTime,
          pGC5035Ctx->AecMinIntegrationTime);


    exp_line = NewIntegrationTime / pGC5035Ctx->one_line_exp_time;
    exp_line =
        MIN(pGC5035Ctx->MaxIntegrationLine,
        MAX(pGC5035Ctx->MinIntegrationLine, exp_line));

    if (exp_line != pGC5035Ctx->OldVsIntegrationTime) {
    /*TODO*/
    //    ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_VSEXP, &exp_line);
    } else if (1){

        pGC5035Ctx->OldVsIntegrationTime = exp_line;
        pGC5035Ctx->AecCurVSIntegrationTime = exp_line * pGC5035Ctx->one_line_exp_time;    //remember current integration time
        *pNumberOfFramesToSkip = 1U;    //skip 1 frame
    } else {
        *pNumberOfFramesToSkip = 0U;    //no frame skip
    }

    *pSetVSIntegrationTime = pGC5035Ctx->AecCurVSIntegrationTime;

    TRACE(GC5035_DEBUG, "%s: NewIntegrationTime=%f\n", __func__,
          NewIntegrationTime);
    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC5035_IsiExposureControlIss
    (IsiSensorHandle_t handle,
     float NewGain,
     float NewIntegrationTime,
     uint8_t * pNumberOfFramesToSkip,
     float *pSetGain, float *pSetIntegrationTime, float *hdr_ratio)
{
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    int TmpGain;
    /*TODO*/

    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (pGC5035Ctx == NULL) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pNumberOfFramesToSkip == NULL) || (pSetGain == NULL)
        || (pSetIntegrationTime == NULL)) {
        TRACE(GC5035_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    TRACE(GC5035_ERROR, "%s: g=%f, Ti=%f\n", __func__, NewGain,
          NewIntegrationTime);

    result = GC5035_IsiSetIntegrationTimeIss(handle, NewIntegrationTime,
                        pSetIntegrationTime,
                        pNumberOfFramesToSkip, hdr_ratio);
    result = GC5035_IsiSetGainIss(handle, NewGain, pSetGain, hdr_ratio);

    pGC5035Ctx->CurHdrRatio = *hdr_ratio;

    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);

    return result;
}

RESULT GC5035_IsiGetCurrentExposureIss
    (IsiSensorHandle_t handle, float *pSetGain, float *pSetIntegrationTime, float *hdr_ratio) {
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (pGC5035Ctx == NULL) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pSetGain == NULL) || (pSetIntegrationTime == NULL))
        return (RET_NULL_POINTER);

    *pSetGain = pGC5035Ctx->AecCurGain;
    *pSetIntegrationTime = pGC5035Ctx->AecCurIntegrationTime;
    *hdr_ratio = pGC5035Ctx->CurHdrRatio;

    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC5035_IsiGetResolutionIss(IsiSensorHandle_t handle, uint16_t *pwidth, uint16_t *pheight) {
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (pGC5035Ctx == NULL) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    *pwidth = pGC5035Ctx->SensorMode.width;
    *pheight =  pGC5035Ctx->SensorMode.height;

    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC5035_IsiGetSensorFpsIss(IsiSensorHandle_t handle, uint32_t * pfps)
{
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    RESULT result = RET_SUCCESS;


    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (pGC5035Ctx == NULL) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    if (pGC5035Ctx->KernelDriverFlag) {
       /*TODO*/
        ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_FPS, pfps);
        pGC5035Ctx->CurrFps = *pfps;
    }

    *pfps = pGC5035Ctx->CurrFps;

    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT GC5035_IsiSetSensorFpsIss(IsiSensorHandle_t handle, uint32_t fps)
{
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    if (pGC5035Ctx == NULL) {
        TRACE(GC5035_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    if (fps > pGC5035Ctx->MaxFps) {
        TRACE(GC5035_ERROR,
              "%s: set fps(%d) out of range, correct to %d (%d, %d)\n",
              __func__, fps, pGC5035Ctx->MaxFps, pGC5035Ctx->MinFps,
              pGC5035Ctx->MaxFps);
        fps = pGC5035Ctx->MaxFps;
    }
    if (fps < pGC5035Ctx->MinFps) {
        TRACE(GC5035_ERROR,
              "%s: set fps(%d) out of range, correct to %d (%d, %d)\n",
              __func__, fps, pGC5035Ctx->MinFps, pGC5035Ctx->MinFps,
              pGC5035Ctx->MaxFps);
        fps = pGC5035Ctx->MinFps;
    }
    if (pGC5035Ctx->KernelDriverFlag) {
        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fps);
        if (ret != 0) {
            TRACE(GC5035_ERROR, "%s: set sensor fps=%d error\n",
                  __func__);
            return (RET_FAILURE);
        }

        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_SENSOR_MODE, &(pGC5035Ctx->SensorMode));
        {
            pGC5035Ctx->MaxIntegrationLine = pGC5035Ctx->SensorMode.ae_info.max_integration_time;
            pGC5035Ctx->AecMaxIntegrationTime = pGC5035Ctx->MaxIntegrationLine * pGC5035Ctx->one_line_exp_time;
        }
#ifdef SUBDEV_CHAR
        struct vvcam_ae_info_s ae_info;
        ret =
            ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_AE_INFO, &ae_info);
        if (ret != 0) {
            TRACE(GC5035_ERROR, "%s:sensor get ae info error!\n",
                  __func__);
            return (RET_FAILURE);
        }
        pGC5035Ctx->one_line_exp_time =
            (float)ae_info.one_line_exp_time_ns / 1000000000;
        pGC5035Ctx->MaxIntegrationLine = ae_info.max_integration_time;
        pGC5035Ctx->AecMaxIntegrationTime =
            pGC5035Ctx->MaxIntegrationLine *
            pGC5035Ctx->one_line_exp_time;
#endif
    }

    TRACE(GC5035_INFO, "%s: set sensor fps = %d\n", __func__,
          pGC5035Ctx->CurrFps);

    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);
    return (result);
}

static RESULT GC5035_IsiActivateTestPattern(IsiSensorHandle_t handle,
                        const bool_t enable)
{
    RESULT result = RET_SUCCESS;

    TRACE(GC5035_INFO, "%s: (enter)\n", __func__);

    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL || pGC5035Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    if (pGC5035Ctx->Configured != BOOL_TRUE)
        return RET_WRONG_STATE;

    if (BOOL_TRUE == enable) {
        //result = GC5035_IsiRegisterWriteIss(handle, 0x3253, 0x80);
    } else {
        //result = GC5035_IsiRegisterWriteIss(handle, 0x3253, 0x00);
    }
    pGC5035Ctx->TestPattern = enable;

    TRACE(GC5035_INFO, "%s: (exit)\n", __func__);

    return (result);
}

static RESULT GC5035_IsiSensorSetBlcIss(IsiSensorHandle_t handle, sensor_blc_t * pblc)
{
    int32_t ret = 0;
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL || pGC5035Ctx->IsiCtx.HalHandle == NULL) {
        return RET_WRONG_HANDLE;
    }

    if (pblc == NULL)
        return RET_NULL_POINTER;

    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_BLC, pblc);
    if (ret != 0)
    {
         TRACE(GC5035_ERROR, "%s: set wb error\n", __func__);
    }

    return RET_SUCCESS;
}

static RESULT GC5035_IsiSensorSetWBIss(IsiSensorHandle_t handle, sensor_white_balance_t * pwb)
{
    int32_t ret = 0;
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL || pGC5035Ctx->IsiCtx.HalHandle == NULL) {
        return RET_WRONG_HANDLE;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    if (pwb == NULL)
        return RET_NULL_POINTER;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_WB, pwb);
    if (ret != 0)
    {
         TRACE(GC5035_ERROR, "%s: set wb error\n", __func__);
    }

    return RET_SUCCESS;
}

static RESULT GC5035_IsiGetSensorAWBModeIss(IsiSensorHandle_t  handle, IsiSensorAwbMode_t *pawbmode)
{
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL || pGC5035Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    if (pGC5035Ctx->SensorMode.hdr_mode == SENSOR_MODE_HDR_NATIVE){
        *pawbmode = ISI_SENSOR_AWB_MODE_SENSOR;
    }else{
        *pawbmode = ISI_SENSOR_AWB_MODE_NORMAL;
    }
    return RET_SUCCESS;
}

static RESULT GC5035_IsiSensorGetExpandCurveIss(IsiSensorHandle_t handle, sensor_expand_curve_t * pexpand_curve)
{
    int32_t ret = 0;
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;
    if (pGC5035Ctx == NULL || pGC5035Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pGC5035Ctx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_EXPAND_CURVE, pexpand_curve);
    if (ret != 0)
    {
        TRACE(GC5035_ERROR, "%s: get  expand cure error\n", __func__);
        return RET_FAILURE;
    }

    return RET_SUCCESS;
}

static RESULT GC5035_IsiGetCapsIss(IsiSensorHandle_t handle,
                         IsiSensorCaps_t * pIsiSensorCaps)
{
    GC5035_Context_t *pGC5035Ctx = (GC5035_Context_t *) handle;

    RESULT result = RET_SUCCESS;

    TRACE(GC5035_INFO, "%s (enter)\n", __func__);

    if (pGC5035Ctx == NULL) {
        return (RET_WRONG_HANDLE);
    }

    if (pIsiSensorCaps == NULL) {
        return (RET_NULL_POINTER);
    }

    pIsiSensorCaps->BusWidth = pGC5035Ctx->SensorMode.bit_width;
    pIsiSensorCaps->Mode = ISI_MODE_BAYER;
    pIsiSensorCaps->FieldSelection = ISI_FIELDSEL_BOTH;
    pIsiSensorCaps->YCSequence = ISI_YCSEQ_YCBYCR;
    pIsiSensorCaps->Conv422 = ISI_CONV422_NOCOSITED;
    pIsiSensorCaps->BPat = pGC5035Ctx->SensorMode.bayer_pattern;
    pIsiSensorCaps->HPol = ISI_HPOL_REFPOS;
    pIsiSensorCaps->VPol = ISI_VPOL_NEG;
    pIsiSensorCaps->Edge = ISI_EDGE_RISING;
    pIsiSensorCaps->Resolution.width = pGC5035Ctx->SensorMode.width;
    pIsiSensorCaps->Resolution.height = pGC5035Ctx->SensorMode.height;
    pIsiSensorCaps->SmiaMode = ISI_SMIA_OFF;
    pIsiSensorCaps->MipiLanes = ISI_MIPI_2LANES;

    if (pIsiSensorCaps->BusWidth == 10) {
        pIsiSensorCaps->MipiMode      = ISI_MIPI_MODE_RAW_10;
    }else if (pIsiSensorCaps->BusWidth == 12){
        pIsiSensorCaps->MipiMode      = ISI_MIPI_MODE_RAW_12;
    }else{
        pIsiSensorCaps->MipiMode      = ISI_MIPI_OFF;
    }

    TRACE(GC5035_INFO, "%s (exit)\n", __func__);
    return result;
}

RESULT GC5035_IsiGetSensorIss(IsiSensor_t *pIsiSensor)
{
    RESULT result = RET_SUCCESS;
    TRACE( GC5035_INFO, "%s (enter)\n", __func__);

    if ( pIsiSensor != NULL ) {
        pIsiSensor->pszName                         = SensorName;
        pIsiSensor->pIsiCreateSensorIss             = GC5035_IsiCreateSensorIss;

        pIsiSensor->pIsiInitSensorIss               = GC5035_IsiInitSensorIss;
        pIsiSensor->pIsiGetSensorModeIss            = GC5035_IsiGetSensorModeIss;
        pIsiSensor->pIsiResetSensorIss              = GC5035_IsiResetSensorIss;
        pIsiSensor->pIsiReleaseSensorIss            = GC5035_IsiReleaseSensorIss;
        pIsiSensor->pIsiGetCapsIss                  = GC5035_IsiGetCapsIss;
        pIsiSensor->pIsiSetupSensorIss              = GC5035_IsiSetupSensorIss;
        pIsiSensor->pIsiChangeSensorResolutionIss   = GC5035_IsiChangeSensorResolutionIss;
        pIsiSensor->pIsiSensorSetStreamingIss       = GC5035_IsiSensorSetStreamingIss;
        pIsiSensor->pIsiSensorSetPowerIss           = GC5035_IsiSensorSetPowerIss;
        pIsiSensor->pIsiCheckSensorConnectionIss    = GC5035_IsiCheckSensorConnectionIss;
        pIsiSensor->pIsiGetSensorRevisionIss        = GC5035_IsiGetSensorRevisionIss;
        pIsiSensor->pIsiRegisterReadIss             = GC5035_IsiRegisterReadIss;
        pIsiSensor->pIsiRegisterWriteIss            = GC5035_IsiRegisterWriteIss;

        /* AEC functions */
        pIsiSensor->pIsiExposureControlIss          = GC5035_IsiExposureControlIss;
        pIsiSensor->pIsiGetGainLimitsIss            = GC5035_IsiGetGainLimitsIss;
        pIsiSensor->pIsiGetIntegrationTimeLimitsIss = GC5035_IsiGetIntegrationTimeLimitsIss;
        pIsiSensor->pIsiGetCurrentExposureIss       = GC5035_IsiGetCurrentExposureIss;
        pIsiSensor->pIsiGetVSGainIss                    = GC5035_IsiGetVSGainIss;
        pIsiSensor->pIsiGetGainIss                      = GC5035_IsiGetGainIss;
        pIsiSensor->pIsiGetLongGainIss                  = GC5035_IsiGetLongGainIss;
        pIsiSensor->pIsiGetGainIncrementIss             = GC5035_IsiGetGainIncrementIss;
        pIsiSensor->pIsiSetGainIss                      = GC5035_IsiSetGainIss;
        pIsiSensor->pIsiGetIntegrationTimeIss           = GC5035_IsiGetIntegrationTimeIss;
        pIsiSensor->pIsiGetVSIntegrationTimeIss         = GC5035_IsiGetVSIntegrationTimeIss;
        pIsiSensor->pIsiGetLongIntegrationTimeIss       = GC5035_IsiGetLongIntegrationTimeIss;
        pIsiSensor->pIsiGetIntegrationTimeIncrementIss  = GC5035_IsiGetIntegrationTimeIncrementIss;
        pIsiSensor->pIsiSetIntegrationTimeIss           = GC5035_IsiSetIntegrationTimeIss;
        pIsiSensor->pIsiQuerySensorIss                  = GC5035_IsiQuerySensorIss;
        pIsiSensor->pIsiGetResolutionIss                = GC5035_IsiGetResolutionIss;
        pIsiSensor->pIsiGetSensorFpsIss                 = GC5035_IsiGetSensorFpsIss;
        pIsiSensor->pIsiSetSensorFpsIss                 = GC5035_IsiSetSensorFpsIss;
        pIsiSensor->pIsiSensorGetExpandCurveIss         = GC5035_IsiSensorGetExpandCurveIss;

        /* AWB specific functions */

        /* Testpattern */
        pIsiSensor->pIsiActivateTestPattern         = GC5035_IsiActivateTestPattern;
        pIsiSensor->pIsiSetBayerPattern             = GC5035_IsiSetBayerPattern;

        pIsiSensor->pIsiSensorSetBlcIss             = GC5035_IsiSensorSetBlcIss;
        pIsiSensor->pIsiSensorSetWBIss              = GC5035_IsiSensorSetWBIss;
        pIsiSensor->pIsiGetSensorAWBModeIss         = GC5035_IsiGetSensorAWBModeIss;

    } else {
        result = RET_NULL_POINTER;
    }

    TRACE( GC5035_INFO, "%s (exit)\n", __func__);
    return ( result );
}

/*****************************************************************************
* each sensor driver need declare this struct for isi load
*****************************************************************************/
IsiCamDrvConfig_t GC5035_IsiCamDrvConfig = {
    0,
    GC5035_IsiQuerySensorSupportIss,
    GC5035_IsiGetSensorIss,
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
