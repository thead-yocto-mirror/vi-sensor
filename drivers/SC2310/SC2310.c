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
#include "SC2310_priv.h"
#include "sc2310.h"

CREATE_TRACER( SC2310_INFO , "SC2310: ", INFO,    0);
CREATE_TRACER( SC2310_WARN , "SC2310: ", WARNING, 1);
CREATE_TRACER( SC2310_ERROR, "SC2310: ", ERROR,   1);
CREATE_TRACER( SC2310_DEBUG,     "SC2310: ", INFO, 1);
CREATE_TRACER( SC2310_REG_INFO , "SC2310: ", INFO, 1);
CREATE_TRACER( SC2310_REG_DEBUG, "SC2310: ", INFO, 1);

#ifdef SUBDEV_V4L2
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#undef TRACE
#define TRACE(x, ...)
#endif

#define SC2310_MIN_GAIN_STEP    ( 1.0f )  /**< min gain step size used by GUI (hardware min = 1/16; 1/16..32/16 depending on actual gain ) */
#define SC2310_MAX_GAIN_AEC     ( 35.0f )       /**< max. gain used by the AEC (arbitrarily chosen, hardware limit = 62.0, driver limit = 32.0 ) */
#define SC2310_VS_MAX_INTEGRATION_TIME (0.0018)

/*****************************************************************************
 *Sensor Info
*****************************************************************************/
static const char SensorName[16] = "SC2310";

static struct vvcam_mode_info psc2310_mode_info[] = {
    {
        .index     = 0,
        .width     = 640,
        .height    = 480,
        .fps       = 26,
        .hdr_mode  = SENSOR_MODE_LINEAR,
        .bit_width = 12,
        .bayer_pattern = BAYER_BGGR, //BAYER_RGGB, //BAYER_BGGR, //BAYER_GBRG, //, // BAYER_GRBG,
        .mipi_phy_freq = 395, //mbps
        .mipi_line_num = 2,
        .config_file_3a = NULL,
        .preg_data = (void *)"sc2310 sensor liner mode, raw12, img resolution is 640*480",
    },
    {
        .index     = 1,
        .width = 1920,
        .height = 1088,
        .fps      = 26,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 12,
        .bayer_pattern = BAYER_BGGR, //BAYER_RGGB, //BAYER_BGGR, //, //,  // BAYER_GRBG,
        .mipi_phy_freq = 395, //mbps
        .mipi_line_num = 2,
        .config_file_3a = "SC2310_1920x1088_raw12", //3aconfig_SC2310_1920x1088_raw12.json
        .preg_data = (void *)"sc2310 sensor liner mode, raw12, img resolution is 1920*1088",
    },
    {
        .index     = 2,
        .width     = 1920,
        .height    = 1080,
        .fps       = 30,
        .hdr_mode  = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_BGGR,
        .mipi_phy_freq = 371, //mbps
        .mipi_line_num = 2,
        .config_file_3a = NULL,
        .preg_data = (void *)"sc2310 sensor liner mode, raw10, img resolution is 1920*1080",
    },
    {
        .index     = 3,
        .width     = 1440,
        .height    = 1080,
        .fps       = 30,
        .hdr_mode  = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_BGGR,
        .mipi_phy_freq = 320, //mbps
        .mipi_line_num = 2,
        .config_file_3a = "SC2310_1440x1080_raw10",  //3aconfig_SC2310_1440x1080_raw10.json
        .preg_data = (void *)"sc2310 sensor liner mode, raw10, img resolution is 1440*1080",
    },
};

static RESULT SC2310_IsiRegisterWriteIss(IsiSensorHandle_t handle, const uint32_t address, const uint32_t value);

//gain is dst value, set_gain is actual value

typedef struct {
    int ana_reg_val;
    float step;
    float max_val;
} sc2310_gain_map_t;

static sc2310_gain_map_t sc2310_gain_map[] = {
    {0x03, 0.015, 1},
    {0x03, 0.015, 1.984},
    {0x07, 0.031, 2.688},
    {0x023, 0.043, 5.398},
    {0x027, 0.085, 10.795},
    {0x02f, 0.170, 21.590},
    {0x03f, 0.340, 35.360},
};

static int sc2310_set_gain(IsiSensorHandle_t handle, float gain, float *set_gain)
{
//Normal 模式/ HDR 模式下的长曝光数据
#define ANA_GAIN 0x3e08
#define ANA_FINE 0x3e09
//HDR 模式下的短曝光数据
#define ANA_VS_GAIN 0x3e08
#define ANA_VS_FINE 0x3e09

    int ret = 0;
    int i = 0;
    uint32_t ana_gain_val = 0;
    uint32_t ana_fine_val = 0;

    if (gain <= 1.0) {
        ana_gain_val = 0x3;
        ana_fine_val = 0x40;
        *set_gain = 1;
    } else if (gain >= 35.360) {
        ana_gain_val = 0x3f;
        ana_fine_val = 0x7f;
        *set_gain = 35.360;
    } else {
        for(i = 0; i < sizeof(sc2310_gain_map) / sizeof(sc2310_gain_map[0]); i++) {
            if (sc2310_gain_map[i].max_val > gain) {
                ana_gain_val = sc2310_gain_map[i].ana_reg_val;
                ana_fine_val =  0x40 + (gain - sc2310_gain_map[i - 1].max_val) / sc2310_gain_map[i].step;
                *set_gain = sc2310_gain_map[i - 1].max_val + (gain - sc2310_gain_map[i - 1].max_val) / sc2310_gain_map[i].step;
                break;
            }
        }
    }


    ret |= SC2310_IsiRegisterWriteIss(handle, ANA_GAIN, ana_gain_val);
    ret |= SC2310_IsiRegisterWriteIss(handle, ANA_FINE, ana_fine_val);

    if (ret != 0) {
        return -1;
    }

    return  ret;
}


long __sc2310_set_exposure(IsiSensorHandle_t handle, int coarse_itg,
				 int gain, int digitgain, SC2310_EXPOSURE_SETTING_t type)
{

	return 0;
}

static RESULT SC2310_IsiSensorSetPowerIss(IsiSensorHandle_t handle, bool_t on) {
    RESULT result = RET_SUCCESS;

    int ret = 0;
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL || pSC2310Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

    TRACE(SC2310_INFO, "%s (enter)\n", __func__);

    int32_t enable = on;
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_POWER, &enable);
    if (ret != 0) {
        // to do
        //TRACE(SC2310_ERROR, "%s: sensor set power error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(SC2310_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT SC2310_IsiResetSensorIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;
    int ret = 0;


    TRACE(SC2310_INFO, "%s (enter)\n", __func__);

    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL || pSC2310Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_RESET, NULL);
    if (ret != 0) {
        TRACE(SC2310_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }
    sleep(0.01);

    ret = SC2310_IsiRegisterWriteIss(handle, 0x103, 1);
    if (ret != 0) {
        TRACE(SC2310_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }

    sleep(0.01);

    ret = SC2310_IsiRegisterWriteIss(handle, 0x100, 0);
    if (ret != 0) {
        TRACE(SC2310_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }

    TRACE(SC2310_INFO, "%s (exit)\n", __func__);
    return (result);
}

#ifdef SUBDEV_CHAR
static RESULT SC2310_IsiSensorSetClkIss(IsiSensorHandle_t handle, uint32_t clk) {
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;

    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL || pSC2310Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

    TRACE(SC2310_INFO, "%s (enter)\n", __func__);

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_CLK, &clk);
    if (ret != 0) {
        // to do
        //TRACE(SC2310_ERROR, "%s: sensor set clk error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(SC2310_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT SC2310_IsiSensorGetClkIss
    (IsiSensorHandle_t handle, uint32_t * pclk) {
    RESULT result = RET_SUCCESS;
    int ret = 0;

    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL || pSC2310Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

    TRACE(SC2310_INFO, "%s (enter)\n", __func__);

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_CLK, pclk);
    if (ret != 0) {
        // to do
        //TRACE(SC2310_ERROR, "%s: sensor get clk error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(SC2310_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT SC2310_IsiConfigSensorSCCBIss(IsiSensorHandle_t handle)
{
    RESULT result = RET_SUCCESS;
    return RET_SUCCESS;
}
#endif

static RESULT SC2310_IsiRegisterReadIss
    (IsiSensorHandle_t handle, const uint32_t address, uint32_t * p_value) {
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    TRACE(SC2310_INFO, "%s (enter)\n", __func__);

    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL || pSC2310Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

    struct vvcam_sccb_data sccb_data;
    sccb_data.addr = address;
    sccb_data.data = 0;
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_READ_REG, &sccb_data);
    if (ret != 0) {
        TRACE(SC2310_ERROR, "%s: read sensor register error!\n",
              __func__);
        return (RET_FAILURE);
    }

    *p_value = sccb_data.data;

    TRACE(SC2310_INFO, "%s (exit) result = %d\n", __func__, result);
    return (result);
}

static RESULT SC2310_IsiRegisterWriteIss
    (IsiSensorHandle_t handle, const uint32_t address, const uint32_t value) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    TRACE(SC2310_INFO, "%s (enter)\n", __func__);

    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL || pSC2310Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

    struct vvcam_sccb_data sccb_data;
    sccb_data.addr = address;
    sccb_data.data = value;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_WRITE_REG, &sccb_data);
    if (ret != 0) {
        TRACE(SC2310_ERROR, "%s: write sensor register error!\n",
              __func__);
        return (RET_FAILURE);
    }

    TRACE(SC2310_INFO, "%s (exit) result = %d\n", __func__, result);
    return (result);
}

static RESULT SC2310_IsiQuerySensorSupportIss(HalHandle_t  HalHandle, vvcam_mode_info_array_t *pSensorSupportInfo)
{
    //int ret = 0;
    struct vvcam_mode_info_array *psensor_mode_info_arry;

    HalContext_t *pHalCtx = HalHandle;
    if ( pHalCtx == NULL ) {
        return RET_NULL_POINTER;
    }

    psensor_mode_info_arry = pSensorSupportInfo;
    psensor_mode_info_arry->count = sizeof(psc2310_mode_info) / sizeof(struct vvcam_mode_info);
    memcpy(psensor_mode_info_arry->modes, psc2310_mode_info, sizeof(psc2310_mode_info));
    return RET_SUCCESS;
}

static  RESULT SC2310_IsiQuerySensorIss(IsiSensorHandle_t handle, vvcam_mode_info_array_t *pSensorInfo)
{
    RESULT result = RET_SUCCESS;
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL || pSC2310Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;
    SC2310_IsiQuerySensorSupportIss(pHalCtx,pSensorInfo);

    return result;
}

static RESULT SC2310_IsiGetSensorModeIss(IsiSensorHandle_t handle,void *mode)
{
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL) {
        return (RET_WRONG_HANDLE);
    }
    memcpy(mode,&(pSC2310Ctx->SensorMode), sizeof(pSC2310Ctx->SensorMode));

    return ( RET_SUCCESS );
}

static RESULT SC2310_IsiCreateSensorIss(IsiSensorInstanceConfig_t * pConfig) {
    RESULT result = RET_SUCCESS;
    SC2310_Context_t *pSC2310Ctx;

    TRACE(SC2310_INFO, "%s (enter)\n", __func__);

    if (!pConfig || !pConfig->pSensor)
        return (RET_NULL_POINTER);

    pSC2310Ctx = (SC2310_Context_t *) malloc(sizeof(SC2310_Context_t));
    if (!pSC2310Ctx) {
        TRACE(SC2310_ERROR, "%s: Can't allocate sc2310 context\n",
              __func__);
        return (RET_OUTOFMEM);
    }

    MEMSET(pSC2310Ctx, 0, sizeof(SC2310_Context_t));

    result = HalAddRef(pConfig->HalHandle);
    if (result != RET_SUCCESS) {
        free(pSC2310Ctx);
        return (result);
    }

    pSC2310Ctx->IsiCtx.HalHandle = pConfig->HalHandle;
    pSC2310Ctx->IsiCtx.pSensor = pConfig->pSensor;
    pSC2310Ctx->GroupHold = BOOL_FALSE;
    pSC2310Ctx->OldGain = 0;
    pSC2310Ctx->OldIntegrationTime = 0;
    pSC2310Ctx->Configured = BOOL_FALSE;
    pSC2310Ctx->Streaming = BOOL_FALSE;
    pSC2310Ctx->TestPattern = BOOL_FALSE;
    pSC2310Ctx->isAfpsRun = BOOL_FALSE;
    pSC2310Ctx->SensorMode.index = pConfig->SensorModeIndex;
    pConfig->hSensor = (IsiSensorHandle_t)pSC2310Ctx;
#ifdef SUBDEV_CHAR
    struct vvcam_mode_info *SensorDefaultMode = NULL;
    for (int i=0; i < sizeof(psc2310_mode_info)/ sizeof(struct vvcam_mode_info); i++)
    {
        if (psc2310_mode_info[i].index == pSC2310Ctx->SensorMode.index)
        {
            SensorDefaultMode = &(psc2310_mode_info[i]);
            break;
        }
    }

    if (SensorDefaultMode != NULL)
    {
        strcpy(pSC2310Ctx->SensorRegCfgFile, get_vi_config_path());
        switch(SensorDefaultMode->index)
        {
            case 0:
                strcat(pSC2310Ctx->SensorRegCfgFile,
                    "SC2310_mipi2lane_640x480_raw12_30fps_init.txt");
                break;
            case 1:
                strcat(pSC2310Ctx->SensorRegCfgFile,
                    "SC2310_mipi2lane_1920x1088_raw12_30fps_init.txt");
                break;
            case 2:
                strcat(pSC2310Ctx->SensorRegCfgFile,
                    "SC2310_mipi2lane_1920x1080_raw10_30fps_init.txt");
                break;
            case 3:
                strcat(pSC2310Ctx->SensorRegCfgFile,
                    "SC2310_mipi2lane_1440x1080_raw10_30fps_init.txt");
                break;
            default:
                break;
        }

        if (access(pSC2310Ctx->SensorRegCfgFile, F_OK) == 0) {
            pSC2310Ctx->KernelDriverFlag = 0;
            memcpy(&(pSC2310Ctx->SensorMode),SensorDefaultMode,sizeof(struct vvcam_mode_info));
        } else {
            pSC2310Ctx->KernelDriverFlag = 1;
        }
    }else
    {
        pSC2310Ctx->KernelDriverFlag = 1;
    }

    result = SC2310_IsiSensorSetPowerIss(pSC2310Ctx, BOOL_TRUE);

    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    uint32_t SensorClkIn = 0;
    if (pSC2310Ctx->KernelDriverFlag) {
        result = SC2310_IsiSensorGetClkIss(pSC2310Ctx, &SensorClkIn);
        RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);
    }

    result = SC2310_IsiSensorSetClkIss(pSC2310Ctx, SensorClkIn);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    if (!pSC2310Ctx->KernelDriverFlag) {
        result = SC2310_IsiConfigSensorSCCBIss(pSC2310Ctx);
        RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    result = SC2310_IsiResetSensorIss(pSC2310Ctx);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    pSC2310Ctx->pattern = ISI_BPAT_BGBGGRGR;

    }
#endif

    TRACE(SC2310_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT SC2310_IsiGetRegCfgIss(const char *registerFileName,
                     struct vvcam_sccb_array *arry)
{
    if (NULL == registerFileName) {
        TRACE(SC2310_ERROR, "%s:registerFileName is NULL\n", __func__);
        return (RET_NULL_POINTER);
    }
#ifdef SUBDEV_CHAR
    FILE *fp = NULL;
    fp = fopen(registerFileName, "rb");
    if (!fp) {
        TRACE(SC2310_ERROR, "%s:load register file  %s error!\n",
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
        TRACE(SC2310_ERROR, "%s:malloc failed NULL Point!\n", __func__,
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

static RESULT SC2310_IsiInitSensorIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;

    int ret = 0;
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;

    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;
    TRACE(SC2310_INFO, "%s (enter)\n", __func__);

    if (pSC2310Ctx == NULL) {
        return (RET_WRONG_HANDLE);
    }

    if (pSC2310Ctx->KernelDriverFlag) {
        ;
    } else {
        struct vvcam_sccb_array arry;
        result = SC2310_IsiGetRegCfgIss(pSC2310Ctx->SensorRegCfgFile, &arry);
        if (result != 0) {
            TRACE(SC2310_ERROR,
                  "%s:SC2310_IsiGetRegCfgIss error!\n", __func__);
            return (RET_FAILURE);
        }

        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_WRITE_ARRAY, &arry);
        if (ret != 0) {
            TRACE(SC2310_ERROR, "%s:Sensor Write Reg arry error!\n",
                  __func__);
            return (RET_FAILURE);
        }

        switch(pSC2310Ctx->SensorMode.index)
        {
            case 0:
                pSC2310Ctx->one_line_exp_time = 1.0 / (2.0 * 0x465 - 6.0) / 26.0;
                pSC2310Ctx->FrameLengthLines = 2 * 0x465;
                pSC2310Ctx->CurFrameLengthLines = pSC2310Ctx->FrameLengthLines;
                pSC2310Ctx->MaxIntegrationLine = pSC2310Ctx->CurFrameLengthLines - 6;
                pSC2310Ctx->MinIntegrationLine = 3;
                pSC2310Ctx->AecMaxGain = 35;
                pSC2310Ctx->AecMinGain = 1;
                break;
            case 1:
                pSC2310Ctx->one_line_exp_time = 1.0 / (2 * 0x465 - 6.0) / 26.0;
                pSC2310Ctx->FrameLengthLines = 2 * 0x465;
                pSC2310Ctx->CurFrameLengthLines = pSC2310Ctx->FrameLengthLines;
                pSC2310Ctx->MaxIntegrationLine = pSC2310Ctx->CurFrameLengthLines - 6;
                pSC2310Ctx->MinIntegrationLine = 3;
                pSC2310Ctx->AecMaxGain = 35;
                pSC2310Ctx->AecMinGain = 1;
                break;
            case 2:
                pSC2310Ctx->one_line_exp_time = 1.0 / (2 * 0x465 - 6.0) / 30.0;
                pSC2310Ctx->FrameLengthLines = 2 * 0x465;
                pSC2310Ctx->CurFrameLengthLines = pSC2310Ctx->FrameLengthLines;
                pSC2310Ctx->MaxIntegrationLine = pSC2310Ctx->CurFrameLengthLines - 6;
                pSC2310Ctx->MinIntegrationLine = 3;
                pSC2310Ctx->AecMaxGain = 35;
                pSC2310Ctx->AecMinGain = 1;
                break;
            case 3:
                pSC2310Ctx->one_line_exp_time = 1.0 / (2 * 0x465 - 6.0) / 30.0;
                pSC2310Ctx->FrameLengthLines = 2 * 0x465;
                pSC2310Ctx->CurFrameLengthLines = pSC2310Ctx->FrameLengthLines;
                pSC2310Ctx->MaxIntegrationLine = pSC2310Ctx->CurFrameLengthLines - 6;
                pSC2310Ctx->MinIntegrationLine = 3;
                pSC2310Ctx->AecMaxGain = 35;
                pSC2310Ctx->AecMinGain = 1;
                break;

            default:
                return ( RET_NOTAVAILABLE );
        }
        printf("pSC2310Ctx->one_line_exp_time %f\n", pSC2310Ctx->one_line_exp_time);
		pSC2310Ctx->AecIntegrationTimeIncrement = pSC2310Ctx->one_line_exp_time;
		pSC2310Ctx->AecMinIntegrationTime =
			pSC2310Ctx->one_line_exp_time * pSC2310Ctx->MinIntegrationLine;
		pSC2310Ctx->AecMaxIntegrationTime =
			pSC2310Ctx->one_line_exp_time * pSC2310Ctx->FrameLengthLines;


        pSC2310Ctx->MaxFps  = pSC2310Ctx->SensorMode.fps;
        pSC2310Ctx->MinFps  = 1;
        pSC2310Ctx->CurrFps = pSC2310Ctx->MaxFps;
    }

    TRACE(SC2310_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT SC2310_IsiReleaseSensorIss(IsiSensorHandle_t handle) {
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(SC2310_INFO, "%s (enter)\n", __func__);

    if (pSC2310Ctx == NULL)
        return (RET_WRONG_HANDLE);

    (void)SC2310_IsiSensorSetStreamingIss(pSC2310Ctx, BOOL_FALSE);
    (void)SC2310_IsiSensorSetPowerIss(pSC2310Ctx, BOOL_FALSE);
    (void)HalDelRef(pSC2310Ctx->IsiCtx.HalHandle);

    MEMSET(pSC2310Ctx, 0, sizeof(SC2310_Context_t));
    free(pSC2310Ctx);
    TRACE(SC2310_INFO, "%s (exit)\n", __func__);
    return (result);
}

struct sc2310_fmt {
    int width;
    int height;
    int fps;
};

static RESULT SC2310_IsiSetupSensorIss
    (IsiSensorHandle_t handle, const IsiSensorConfig_t * pConfig) {

    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

    RESULT result = RET_SUCCESS;

    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (!pSC2310Ctx) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pConfig) {
        TRACE(SC2310_ERROR,
              "%s: Invalid configuration (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    if (pSC2310Ctx->Streaming != BOOL_FALSE) {
        return RET_WRONG_STATE;
    }

    memcpy(&pSC2310Ctx->Config, pConfig, sizeof(IsiSensorConfig_t));

    pSC2310Ctx->Configured = BOOL_TRUE;
    TRACE(SC2310_INFO, "%s: (exit) ret=0x%x \n", __func__, result);
    return result;
}

static RESULT SC2310_IsiChangeSensorResolutionIss(IsiSensorHandle_t handle, uint16_t width, uint16_t height) {
    RESULT result = RET_SUCCESS;
#if 0
    struct sc2310_fmt fmt;
    fmt.width = width;
    fmt.height = height;

    int ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fmt);
#endif
    TRACE(SC2310_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT SC2310_IsiSensorSetStreamingIss
    (IsiSensorHandle_t handle, bool_t on) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    TRACE(SC2310_INFO, "%s (enter)\n", __func__);

    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL || pSC2310Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

    if (pSC2310Ctx->Configured != BOOL_TRUE)
        return RET_WRONG_STATE;

    if (on == 0) {
        ret = SC2310_IsiRegisterWriteIss(handle, 0x3812, 0);
        if (ret != 0) {
            return (RET_FAILURE);
        }

        ret = SC2310_IsiRegisterWriteIss(handle, 0x100, on);
        if (ret != 0) {
            return (RET_FAILURE);
        }

        ret = SC2310_IsiRegisterWriteIss(handle, 0x3812, 0x30);
        if (ret != 0) {
            return (RET_FAILURE);
        }
    } else {
        ret = SC2310_IsiRegisterWriteIss(handle, 0x100, on);
        if (ret != 0) {
            return (RET_FAILURE);
        }
    }

    pSC2310Ctx->Streaming = on;

    TRACE(SC2310_INFO, "%s (exit)\n", __func__);
    return (result);
}

static int32_t sensor_get_chip_id(IsiSensorHandle_t handle, uint32_t *chip_id)
{
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    int32_t chip_id_high = 0;
    int32_t chip_id_low = 0;

    ret = SC2310_IsiRegisterReadIss(handle, 0x3107, &chip_id_high);
    if (ret != 0) {
        TRACE(SC2310_ERROR,
            "%s: Read Sensor correct ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    ret = SC2310_IsiRegisterReadIss(handle, 0x3108, &chip_id_low);
    if (ret != 0) {
        TRACE(SC2310_ERROR,
            "%s: Read Sensor correct ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    *chip_id = ((chip_id_high & 0xff)<<8) | (chip_id_low & 0xff);
    printf("sc2310 chip id is %d\n", *chip_id);

    return 0;
}

static RESULT SC2310_IsiCheckSensorConnectionIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    uint32_t correct_id = 0x2311;
    uint32_t sensor_id = 0;

    TRACE(SC2310_INFO, "%s (enter)\n", __func__);

    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL || pSC2310Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

    ret = sensor_get_chip_id(handle, &sensor_id);
    if (ret != 0) {
        TRACE(SC2310_ERROR,
            "%s: Read Sensor chip ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    if (correct_id != sensor_id) {
        TRACE(SC2310_ERROR, "%s:ChipID =0x%x sensor_id=%x error! \n",
              __func__, correct_id, sensor_id);
        return (RET_FAILURE);
    }

    TRACE(SC2310_INFO,
          "%s ChipID = 0x%08x, sensor_id = 0x%08x, success! \n", __func__,
          correct_id, sensor_id);
    TRACE(SC2310_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT SC2310_IsiGetSensorRevisionIss
    (IsiSensorHandle_t handle, uint32_t * p_value) {
    RESULT result = RET_SUCCESS;
    TRACE(SC2310_INFO, "%s (enter)\n", __func__);

    *p_value = 0X2311;
    TRACE(SC2310_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT SC2310_IsiGetGainLimitsIss
    (IsiSensorHandle_t handle, float *pMinGain, float *pMaxGain) {
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    /*TODO*/

    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (pSC2310Ctx == NULL) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pMinGain == NULL) || (pMaxGain == NULL)) {
        TRACE(SC2310_ERROR, "%s: NULL pointer received!!\n");
        return (RET_NULL_POINTER);
    }

    *pMinGain = pSC2310Ctx->AecMinGain;
    *pMaxGain = pSC2310Ctx->AecMaxGain;

    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);
    return (result);
}

static RESULT SC2310_IsiGetIntegrationTimeLimitsIss
    (IsiSensorHandle_t handle,
     float *pMinIntegrationTime, float *pMaxIntegrationTime) {
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    /*TODO*/

    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);
    if (pSC2310Ctx == NULL) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pMinIntegrationTime == NULL) || (pMaxIntegrationTime == NULL)) {
        TRACE(SC2310_ERROR, "%s: NULL pointer received!!\n");
        return (RET_NULL_POINTER);
    }

    *pMinIntegrationTime = pSC2310Ctx->AecMinIntegrationTime;
    *pMaxIntegrationTime = pSC2310Ctx->AecMaxIntegrationTime;

    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);
    return (result);
}

RESULT SC2310_IsiGetGainIss(IsiSensorHandle_t handle, float *pSetGain) {
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (pSC2310Ctx == NULL) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pSetGain == NULL) {
        return (RET_NULL_POINTER);
    }

    *pSetGain = pSC2310Ctx->AecCurGain;

    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC2310_IsiGetLongGainIss(IsiSensorHandle_t handle, float *gain)
{
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;

    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (pSC2310Ctx == NULL) {
        TRACE(SC2310_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }

    if (gain == NULL) {
        return (RET_NULL_POINTER);
    }

    *gain = pSC2310Ctx->AecCurLongGain;

    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);

    return (RET_SUCCESS);
}

RESULT SC2310_IsiGetVSGainIss(IsiSensorHandle_t handle, float *pSetGain) {
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (pSC2310Ctx == NULL) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pSetGain == NULL) {
        return (RET_NULL_POINTER);
    }

    *pSetGain = pSC2310Ctx->AecCurVSGain;

    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);

    return (result);
}

RESULT SC2310_IsiGetGainIncrementIss(IsiSensorHandle_t handle, float *pIncr) {
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (pSC2310Ctx == NULL) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pIncr == NULL)
        return (RET_NULL_POINTER);

    *pIncr = pSC2310Ctx->AecGainIncrement;

    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);

    return (result);
}

RESULT SC2310_IsiSetGainIss
    (IsiSensorHandle_t handle,
     float NewGain, float *pSetGain, float *hdr_ratio) {

    RESULT result = RET_SUCCESS;
    int32_t ret = 0;

    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL || pSC2310Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

    sc2310_set_gain(handle, NewGain, pSetGain);
    pSC2310Ctx->AecCurGain = *pSetGain;

    TRACE(SC2310_DEBUG, "%s: g=%f\n", __func__, *pSetGain);
    return (result);
}

RESULT SC2310_IsiSetLongGainIss(IsiSensorHandle_t handle, float gain)
{
    int ret = 0;
    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;

    if (!pSC2310Ctx || !pSC2310Ctx->IsiCtx.HalHandle)
    {
        TRACE(SC2310_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

    uint32_t SensorGain = 0;
    SensorGain = gain * pSC2310Ctx->gain_accuracy;
    if (pSC2310Ctx->LastLongGain != SensorGain)
    {

        /*TODO*/
#if 0
        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_LONG_GAIN, &SensorGain);
        if (ret != 0)
        {
            return (RET_FAILURE);
            TRACE(SC2310_ERROR,"%s: set long gain failed\n");

        }
#endif
        pSC2310Ctx->LastLongGain = SensorGain;
        pSC2310Ctx->AecCurLongGain = gain;
    }

    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT SC2310_IsiSetVSGainIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float NewGain, float *pSetGain, float *hdr_ratio) {
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;
    RESULT result = RET_SUCCESS;
#if 0
    float Gain = 0.0f;

    uint32_t ucGain = 0U;
    uint32_t again = 0U;
#endif

    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (!pSC2310Ctx) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetGain || !hdr_ratio)
        return (RET_NULL_POINTER);

    uint32_t SensorGain = 0;
    SensorGain = NewGain * pSC2310Ctx->gain_accuracy;

    /*TODO*/
    //ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_VSGAIN, &SensorGain);

    pSC2310Ctx->AecCurVSGain = NewGain;
    *pSetGain = pSC2310Ctx->AecCurGain;
    TRACE(SC2310_DEBUG, "%s: g=%f\n", __func__, *pSetGain);
    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC2310_IsiSetBayerPattern(IsiSensorHandle_t handle, uint8_t pattern)
{

    RESULT result = RET_SUCCESS;
#if 0
    uint8_t h_shift = 0, v_shift = 0;
    uint32_t val_h = 0, val_l = 0;
    uint16_t val = 0;
    uint8_t Start_p = 0;
    bool_t streaming_status;
    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL || pSC2310Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    // pattern 0:B 1:GB 2:GR 3:R
    streaming_status = pSC2310Ctx->Streaming;
    result = SC2310_IsiSensorSetStreamingIss(handle, 0);
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

    SC2310_IsiRegisterReadIss(handle, 0x30a0, &val_h);
    SC2310_IsiRegisterReadIss(handle, 0x30a1, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + h_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    SC2310_IsiRegisterWriteIss(handle, 0x30a0, (uint8_t)val_h);
    SC2310_IsiRegisterWriteIss(handle, 0x30a1, (uint8_t)val_l);

    SC2310_IsiRegisterReadIss(handle, 0x30a2, &val_h);
    SC2310_IsiRegisterReadIss(handle, 0x30a3, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + v_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    SC2310_IsiRegisterWriteIss(handle, 0x30a2, (uint8_t)val_h);
    SC2310_IsiRegisterWriteIss(handle, 0x30a3, (uint8_t)val_l);

    SC2310_IsiRegisterReadIss(handle, 0x30a4, &val_h);
    SC2310_IsiRegisterReadIss(handle, 0x30a5, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + h_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    SC2310_IsiRegisterWriteIss(handle, 0x30a4, (uint8_t)val_h);
    SC2310_IsiRegisterWriteIss(handle, 0x30a5, (uint8_t)val_l);

    SC2310_IsiRegisterReadIss(handle, 0x30a6, &val_h);
    SC2310_IsiRegisterReadIss(handle, 0x30a7, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + v_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    SC2310_IsiRegisterWriteIss(handle, 0x30a6, (uint8_t)val_h);
    SC2310_IsiRegisterWriteIss(handle, 0x30a7, (uint8_t)val_l);

    pSC2310Ctx->pattern = pattern;
    result = SC2310_IsiSensorSetStreamingIss(handle, streaming_status);
#endif

    return (result);
}

RESULT SC2310_IsiGetIntegrationTimeIss
    (IsiSensorHandle_t handle, float *pSetIntegrationTime)
{
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (!pSC2310Ctx) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetIntegrationTime)
        return (RET_NULL_POINTER);
    *pSetIntegrationTime = pSC2310Ctx->AecCurIntegrationTime;
    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC2310_IsiGetLongIntegrationTimeIss(IsiSensorHandle_t handle, float *pIntegrationTime)
{
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (!pSC2310Ctx) {
        TRACE(SC2310_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }
    if (!pIntegrationTime)
        return (RET_NULL_POINTER);

    pSC2310Ctx->AecCurLongIntegrationTime =  pSC2310Ctx->AecCurIntegrationTime;

    *pIntegrationTime = pSC2310Ctx->AecCurLongIntegrationTime;
    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT SC2310_IsiGetVSIntegrationTimeIss
    (IsiSensorHandle_t handle, float *pSetIntegrationTime)
{
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (!pSC2310Ctx) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    if (!pSetIntegrationTime)
        return (RET_NULL_POINTER);

    *pSetIntegrationTime = pSC2310Ctx->AecCurVSIntegrationTime;
    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC2310_IsiGetIntegrationTimeIncrementIss
    (IsiSensorHandle_t handle, float *pIncr)
{
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (!pSC2310Ctx) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pIncr)
        return (RET_NULL_POINTER);

    //_smallest_ increment the sensor/driver can handle (e.g. used for sliders in the application)
    *pIncr = pSC2310Ctx->AecIntegrationTimeIncrement;
    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC2310_IsiSetIntegrationTimeIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float *pSetIntegrationTime,
     uint8_t * pNumberOfFramesToSkip, float *hdr_ratio)
{
    RESULT result = RET_SUCCESS;
    uint32_t exp_lines = 0;

    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

    exp_lines = NewIntegrationTime / pSC2310Ctx->one_line_exp_time;
    if (exp_lines > pSC2310Ctx->MaxIntegrationLine) {
        exp_lines = pSC2310Ctx->MaxIntegrationLine;
    } else if (exp_lines < 3) {
        exp_lines = 3;
    }

    //行长 = 寄存器{16‘h320c, 16′h320d}值*2
    //2*{16’h320e,16’h320f}-6:h320e,h320f为帧长
    uint32_t hval_time = (exp_lines & 0xff0) >> 4;
    uint32_t lval_time = (exp_lines & 0x0f) << 4;

    result = SC2310_IsiRegisterWriteIss(handle, 0x3e01, hval_time);
    result = SC2310_IsiRegisterWriteIss(handle, 0x3e02, lval_time);

    pSC2310Ctx->AecCurIntegrationTime = exp_lines * pSC2310Ctx->one_line_exp_time;
    *pNumberOfFramesToSkip = 1U;
    *pSetIntegrationTime = pSC2310Ctx->AecCurIntegrationTime;

#if 0
    uint32_t exp_line = 0;
    uint32_t exp_line_old = 0;

    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (!pSC2310Ctx) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetIntegrationTime || !pNumberOfFramesToSkip) {
        TRACE(SC2310_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    exp_line = NewIntegrationTime / pSC2310Ctx->one_line_exp_time;
    exp_line_old = exp_line;
    exp_line =
        MIN(pSC2310Ctx->MaxIntegrationLine,
        MAX(pSC2310Ctx->MinIntegrationLine, exp_line));

    TRACE(SC2310_DEBUG, "%s: set AEC_PK_EXPO=0x%05x\n", __func__, exp_line);

    if (exp_line != pSC2310Ctx->OldIntegrationTime) {

        /*TODO*/
        //ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_EXP, &exp_line);
        pSC2310Ctx->OldIntegrationTime = exp_line;    // remember current integration time
        pSC2310Ctx->AecCurIntegrationTime =
            exp_line * pSC2310Ctx->one_line_exp_time;

        *pNumberOfFramesToSkip = 1U;    //skip 1 frame
    } else {
        *pNumberOfFramesToSkip = 0U;    //no frame skip
    }

    if (NewIntegrationTime > pSC2310Ctx->FrameLengthLines * pSC2310Ctx->one_line_exp_time)
        NewIntegrationTime = pSC2310Ctx->FrameLengthLines * pSC2310Ctx->one_line_exp_time;
    float exp_t = NewIntegrationTime * 16.0f / pSC2310Ctx->one_line_exp_time;
    __sc2310_set_exposure(handle, (int)exp_t,
				0, 0, SC2310_INTEGRATION_TIME);

    if (exp_line_old != exp_line) {
        *pSetIntegrationTime = pSC2310Ctx->AecCurIntegrationTime;
    } else {
        *pSetIntegrationTime = NewIntegrationTime;
    }
#endif

    TRACE(SC2310_DEBUG, "%s: Ti=%f\n", __func__, *pSetIntegrationTime);
    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC2310_IsiSetLongIntegrationTimeIss(IsiSensorHandle_t handle,float IntegrationTime)
{
    int ret;
    uint32_t exp_lines;
    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (!handle || !pSC2310Ctx->IsiCtx.HalHandle)
    {
        TRACE(SC2310_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

    exp_lines = IntegrationTime / pSC2310Ctx->one_line_exp_time;
    if (exp_lines > pSC2310Ctx->MaxIntegrationLine) {
        exp_lines = pSC2310Ctx->MaxIntegrationLine;
    } else if (exp_lines < 1) {
        exp_lines = 1;
    }

    uint32_t hval_time =  (exp_lines & 0xf00) >> 4;
    uint32_t lval_time =  exp_lines & 0xff;

    SC2310_IsiRegisterWriteIss(handle, 0x3e01, lval_time);
    SC2310_IsiRegisterWriteIss(handle, 0x3e02, hval_time);

    pSC2310Ctx->AecCurIntegrationTime = exp_lines * pSC2310Ctx->one_line_exp_time;
    pSC2310Ctx->AecCurLongIntegrationTime = exp_lines * pSC2310Ctx->one_line_exp_time;

    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT SC2310_IsiSetVSIntegrationTimeIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float *pSetVSIntegrationTime,
     uint8_t * pNumberOfFramesToSkip, float *hdr_ratio)
{
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    uint32_t exp_line = 0;

    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (!pSC2310Ctx) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetVSIntegrationTime || !pNumberOfFramesToSkip) {
        TRACE(SC2310_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    TRACE(SC2310_INFO,
          "%s:  maxIntegrationTime-=%f minIntegrationTime = %f\n", __func__,
          pSC2310Ctx->AecMaxIntegrationTime,
          pSC2310Ctx->AecMinIntegrationTime);
/*
    uint32_t hval_time =  (((uint32_t)NewIntegrationTime) & 0xf00) >> 4;
    uint32_t lval_time =  ((uint32_t)NewIntegrationTime) & 0xff;

    SC2310_IsiRegisterWriteIss(handle, 0x3e04, lval_time);
    SC2310_IsiRegisterWriteIss(handle, 0x3e05, hval_time);
    */



    exp_line = NewIntegrationTime / pSC2310Ctx->one_line_exp_time;
    exp_line =
        MIN(pSC2310Ctx->MaxIntegrationLine,
        MAX(pSC2310Ctx->MinIntegrationLine, exp_line));

    if (exp_line != pSC2310Ctx->OldVsIntegrationTime) {
    /*TODO*/
    //    ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_VSEXP, &exp_line);
    } else if (1){

        pSC2310Ctx->OldVsIntegrationTime = exp_line;
        pSC2310Ctx->AecCurVSIntegrationTime = exp_line * pSC2310Ctx->one_line_exp_time;    //remember current integration time
        *pNumberOfFramesToSkip = 1U;    //skip 1 frame
    } else {
        *pNumberOfFramesToSkip = 0U;    //no frame skip
    }

    *pSetVSIntegrationTime = pSC2310Ctx->AecCurVSIntegrationTime;

    TRACE(SC2310_DEBUG, "%s: NewIntegrationTime=%f\n", __func__,
          NewIntegrationTime);
    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC2310_IsiExposureControlIss
    (IsiSensorHandle_t handle,
     float NewGain,
     float NewIntegrationTime,
     uint8_t * pNumberOfFramesToSkip,
     float *pSetGain, float *pSetIntegrationTime, float *hdr_ratio)
{
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (pSC2310Ctx == NULL) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pNumberOfFramesToSkip == NULL) || (pSetGain == NULL)
        || (pSetIntegrationTime == NULL)) {
        TRACE(SC2310_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    if (NewGain >= 35) { // More than 35 will not take effect
        NewGain = 35;
    }

    TRACE(SC2310_DEBUG, "%s: g=%f, Ti=%f\n", __func__, NewGain,
          NewIntegrationTime);

    if (NewIntegrationTime > pSC2310Ctx->MaxIntegrationLine * pSC2310Ctx->one_line_exp_time)
        NewIntegrationTime = pSC2310Ctx->MaxIntegrationLine * pSC2310Ctx->one_line_exp_time;


    sc2310_set_gain(handle, NewGain, pSetGain);
    SC2310_IsiSetIntegrationTimeIss(handle, NewIntegrationTime, pSetIntegrationTime, pNumberOfFramesToSkip, hdr_ratio);
    pSC2310Ctx->AecCurGain = NewGain;
    pSC2310Ctx->AecCurIntegrationTime = *pSetIntegrationTime;


    TRACE(SC2310_DEBUG, "%s: set: vsg=%f, vsTi=%f, vsskip=%d\n", __func__,
          NewGain, NewIntegrationTime, *pNumberOfFramesToSkip);
    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);

    return result;
}

RESULT SC2310_IsiGetCurrentExposureIss
    (IsiSensorHandle_t handle, float *pSetGain, float *pSetIntegrationTime, float *hdr_ratio) {
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (pSC2310Ctx == NULL) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pSetGain == NULL) || (pSetIntegrationTime == NULL))
        return (RET_NULL_POINTER);

    *pSetGain = pSC2310Ctx->AecCurGain;
    *pSetIntegrationTime = pSC2310Ctx->AecCurIntegrationTime;
    *hdr_ratio = pSC2310Ctx->CurHdrRatio;

    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC2310_IsiGetResolutionIss(IsiSensorHandle_t handle, uint16_t *pwidth, uint16_t *pheight) {
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (pSC2310Ctx == NULL) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    *pwidth = pSC2310Ctx->SensorMode.width;
    *pheight =  pSC2310Ctx->SensorMode.height;

    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC2310_IsiGetSensorFpsIss(IsiSensorHandle_t handle, uint32_t * pfps)
{
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    RESULT result = RET_SUCCESS;


    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (pSC2310Ctx == NULL) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

    if (pSC2310Ctx->KernelDriverFlag) {
       /*TODO*/
        ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_FPS, pfps);
        pSC2310Ctx->CurrFps = *pfps;
    }

    *pfps = pSC2310Ctx->CurrFps;

    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC2310_IsiSetSensorFpsIss(IsiSensorHandle_t handle, uint32_t fps)
{
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    if (pSC2310Ctx == NULL) {
        TRACE(SC2310_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

    if (fps > pSC2310Ctx->MaxFps) {
        TRACE(SC2310_ERROR,
              "%s: set fps(%d) out of range, correct to %d (%d, %d)\n",
              __func__, fps, pSC2310Ctx->MaxFps, pSC2310Ctx->MinFps,
              pSC2310Ctx->MaxFps);
        fps = pSC2310Ctx->MaxFps;
    }
    if (fps < pSC2310Ctx->MinFps) {
        TRACE(SC2310_ERROR,
              "%s: set fps(%d) out of range, correct to %d (%d, %d)\n",
              __func__, fps, pSC2310Ctx->MinFps, pSC2310Ctx->MinFps,
              pSC2310Ctx->MaxFps);
        fps = pSC2310Ctx->MinFps;
    }

    TRACE(SC2310_INFO, "%s: set sensor fps = %d\n", __func__,
          pSC2310Ctx->CurrFps);

    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);
    return (result);
}

static RESULT SC2310_IsiActivateTestPattern(IsiSensorHandle_t handle,
                        const bool_t enable)
{
    RESULT result = RET_SUCCESS;
    uint32_t reg_val;


    TRACE(SC2310_INFO, "%s: (enter)\n", __func__);

    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL || pSC2310Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    if (pSC2310Ctx->Configured != BOOL_TRUE)
        return RET_WRONG_STATE;

    result = SC2310_IsiRegisterReadIss(handle, 0x4501, &reg_val);
    if (result != RET_SUCCESS) {
        return result;
    }
    reg_val &= ~8;

    if (BOOL_TRUE == enable) {
        reg_val |= 8;
    }

    result = SC2310_IsiRegisterWriteIss(handle, 0x4501, reg_val);
    if (result != RET_SUCCESS) {
        return result;
    }

    pSC2310Ctx->TestPattern = enable;

    TRACE(SC2310_INFO, "%s: (exit)\n", __func__);

    return (result);
}

static RESULT SC2310_IsiSensorSetBlcIss(IsiSensorHandle_t handle, sensor_blc_t * pblc)
{
    int32_t ret = 0;
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL || pSC2310Ctx->IsiCtx.HalHandle == NULL) {
        return RET_WRONG_HANDLE;
    }

    if (pblc == NULL)
        return RET_NULL_POINTER;

    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_BLC, pblc);
    if (ret != 0)
    {
         TRACE(SC2310_ERROR, "%s: set wb error\n", __func__);
    }

    return RET_SUCCESS;
}

static RESULT SC2310_IsiSensorSetWBIss(IsiSensorHandle_t handle, sensor_white_balance_t * pwb)
{
    int32_t ret = 0;
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL || pSC2310Ctx->IsiCtx.HalHandle == NULL) {
        return RET_WRONG_HANDLE;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

    if (pwb == NULL)
        return RET_NULL_POINTER;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_WB, pwb);
    if (ret != 0)
    {
         TRACE(SC2310_ERROR, "%s: set wb error\n", __func__);
    }

    return RET_SUCCESS;
}

static RESULT SC2310_IsiGetSensorAWBModeIss(IsiSensorHandle_t  handle, IsiSensorAwbMode_t *pawbmode)
{
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL || pSC2310Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    if (pSC2310Ctx->SensorMode.hdr_mode == SENSOR_MODE_HDR_NATIVE){
        *pawbmode = ISI_SENSOR_AWB_MODE_SENSOR;
    }else{
        *pawbmode = ISI_SENSOR_AWB_MODE_NORMAL;
    }
    return RET_SUCCESS;
}

static RESULT SC2310_IsiSensorGetExpandCurveIss(IsiSensorHandle_t handle, sensor_expand_curve_t * pexpand_curve)
{
    int32_t ret = 0;
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;
    if (pSC2310Ctx == NULL || pSC2310Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pSC2310Ctx->IsiCtx.HalHandle;

/*
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_EXPAND_CURVE, pexpand_curve);
    if (ret != 0)
    {
        TRACE(SC2310_ERROR, "%s: get  expand cure error\n", __func__);
        return RET_FAILURE;
    }
    */

    return RET_FAILURE;
}

static RESULT SC2310_IsiGetCapsIss(IsiSensorHandle_t handle,
                         IsiSensorCaps_t * pIsiSensorCaps)
{
    SC2310_Context_t *pSC2310Ctx = (SC2310_Context_t *) handle;

    RESULT result = RET_SUCCESS;

    TRACE(SC2310_INFO, "%s (enter)\n", __func__);

    if (pSC2310Ctx == NULL) {
        return (RET_WRONG_HANDLE);
    }

    if (pIsiSensorCaps == NULL) {
        return (RET_NULL_POINTER);
    }

    pIsiSensorCaps->BusWidth = pSC2310Ctx->SensorMode.bit_width;
    pIsiSensorCaps->Mode = ISI_MODE_BAYER;
    pIsiSensorCaps->FieldSelection = ISI_FIELDSEL_BOTH;
    pIsiSensorCaps->YCSequence = ISI_YCSEQ_YCBYCR;
    pIsiSensorCaps->Conv422 = ISI_CONV422_NOCOSITED;
    pIsiSensorCaps->BPat = pSC2310Ctx->SensorMode.bayer_pattern;
    pIsiSensorCaps->HPol = ISI_HPOL_REFPOS;
    pIsiSensorCaps->VPol = ISI_VPOL_NEG;
    pIsiSensorCaps->Edge = ISI_EDGE_RISING;
    pIsiSensorCaps->Resolution.width = pSC2310Ctx->SensorMode.width;
    pIsiSensorCaps->Resolution.height = pSC2310Ctx->SensorMode.height;
    pIsiSensorCaps->SmiaMode = ISI_SMIA_OFF;
    pIsiSensorCaps->MipiLanes = ISI_MIPI_2LANES;

    if (pIsiSensorCaps->BusWidth == 10) {
        pIsiSensorCaps->MipiMode      = ISI_MIPI_MODE_RAW_10;
    }else if (pIsiSensorCaps->BusWidth == 12){
        pIsiSensorCaps->MipiMode      = ISI_MIPI_MODE_RAW_12;
    }else{
        pIsiSensorCaps->MipiMode      = ISI_MIPI_OFF;
    }

    TRACE(SC2310_INFO, "%s (exit)\n", __func__);
    return result;
}

RESULT SC2310_IsiGetSensorIss(IsiSensor_t *pIsiSensor)
{
    RESULT result = RET_SUCCESS;
    TRACE( SC2310_INFO, "%s (enter)\n", __func__);

    if ( pIsiSensor != NULL ) {
        pIsiSensor->pszName                         = SensorName;
        pIsiSensor->pIsiCreateSensorIss             = SC2310_IsiCreateSensorIss;

        pIsiSensor->pIsiInitSensorIss               = SC2310_IsiInitSensorIss;
        pIsiSensor->pIsiGetSensorModeIss            = SC2310_IsiGetSensorModeIss;
        pIsiSensor->pIsiResetSensorIss              = SC2310_IsiResetSensorIss;
        pIsiSensor->pIsiReleaseSensorIss            = SC2310_IsiReleaseSensorIss;
        pIsiSensor->pIsiGetCapsIss                  = SC2310_IsiGetCapsIss;
        pIsiSensor->pIsiSetupSensorIss              = SC2310_IsiSetupSensorIss;
        pIsiSensor->pIsiChangeSensorResolutionIss   = SC2310_IsiChangeSensorResolutionIss;
        pIsiSensor->pIsiSensorSetStreamingIss       = SC2310_IsiSensorSetStreamingIss;
        pIsiSensor->pIsiSensorSetPowerIss           = SC2310_IsiSensorSetPowerIss;
        pIsiSensor->pIsiCheckSensorConnectionIss    = SC2310_IsiCheckSensorConnectionIss;
        pIsiSensor->pIsiGetSensorRevisionIss        = SC2310_IsiGetSensorRevisionIss;
        pIsiSensor->pIsiRegisterReadIss             = SC2310_IsiRegisterReadIss;
        pIsiSensor->pIsiRegisterWriteIss            = SC2310_IsiRegisterWriteIss;

        /* AEC functions */
        pIsiSensor->pIsiExposureControlIss          = SC2310_IsiExposureControlIss;
        pIsiSensor->pIsiGetGainLimitsIss            = SC2310_IsiGetGainLimitsIss;
        pIsiSensor->pIsiGetIntegrationTimeLimitsIss = SC2310_IsiGetIntegrationTimeLimitsIss;
        pIsiSensor->pIsiGetCurrentExposureIss       = SC2310_IsiGetCurrentExposureIss;
        pIsiSensor->pIsiGetVSGainIss                    = SC2310_IsiGetVSGainIss;
        pIsiSensor->pIsiGetGainIss                      = SC2310_IsiGetGainIss;
        pIsiSensor->pIsiGetLongGainIss                  = SC2310_IsiGetLongGainIss;
        pIsiSensor->pIsiGetGainIncrementIss             = SC2310_IsiGetGainIncrementIss;
        pIsiSensor->pIsiSetGainIss                      = SC2310_IsiSetGainIss;
        pIsiSensor->pIsiGetIntegrationTimeIss           = SC2310_IsiGetIntegrationTimeIss;
        pIsiSensor->pIsiGetVSIntegrationTimeIss         = SC2310_IsiGetVSIntegrationTimeIss;
        pIsiSensor->pIsiGetLongIntegrationTimeIss       = SC2310_IsiGetLongIntegrationTimeIss;
        pIsiSensor->pIsiGetIntegrationTimeIncrementIss  = SC2310_IsiGetIntegrationTimeIncrementIss;
        pIsiSensor->pIsiSetIntegrationTimeIss           = SC2310_IsiSetIntegrationTimeIss;
        pIsiSensor->pIsiQuerySensorIss                  = SC2310_IsiQuerySensorIss;
        pIsiSensor->pIsiGetResolutionIss                = SC2310_IsiGetResolutionIss;
        pIsiSensor->pIsiGetSensorFpsIss                 = SC2310_IsiGetSensorFpsIss;
        pIsiSensor->pIsiSetSensorFpsIss                 = SC2310_IsiSetSensorFpsIss;
        pIsiSensor->pIsiSensorGetExpandCurveIss         = SC2310_IsiSensorGetExpandCurveIss;

        /* AWB specific functions */

        /* Testpattern */
        pIsiSensor->pIsiActivateTestPattern         = SC2310_IsiActivateTestPattern;
        pIsiSensor->pIsiSetBayerPattern             = SC2310_IsiSetBayerPattern;

        pIsiSensor->pIsiSensorSetBlcIss             = SC2310_IsiSensorSetBlcIss;
        pIsiSensor->pIsiSensorSetWBIss              = SC2310_IsiSensorSetWBIss;
        pIsiSensor->pIsiGetSensorAWBModeIss         = SC2310_IsiGetSensorAWBModeIss;

    } else {
        result = RET_NULL_POINTER;
    }

    TRACE( SC2310_INFO, "%s (exit)\n", __func__);
    return ( result );
}

/*****************************************************************************
* each sensor driver need declare this struct for isi load
*****************************************************************************/
IsiCamDrvConfig_t SC2310_IsiCamDrvConfig = {
    0,
    SC2310_IsiQuerySensorSupportIss,
    SC2310_IsiGetSensorIss,
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
