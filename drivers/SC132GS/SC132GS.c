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
#include "SC132GS_priv.h"
#include "sc132gs.h"

CREATE_TRACER( SC132GS_INFO , "SC132GS: ", INFO,    0);
CREATE_TRACER( SC132GS_WARN , "SC132GS: ", WARNING, 1);
CREATE_TRACER( SC132GS_ERROR, "SC132GS: ", ERROR,   1);
CREATE_TRACER( SC132GS_DEBUG,     "SC132GS: ", INFO, 1);
CREATE_TRACER( SC132GS_REG_INFO , "SC132GS: ", INFO, 1);
CREATE_TRACER( SC132GS_REG_DEBUG, "SC132GS: ", INFO, 1);

#ifdef SUBDEV_V4L2
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#undef TRACE
#define TRACE(x, ...)
#endif
#define SC132GS_MIN_GAIN_STEP    ( 1.0f/16.0f )  /**< min gain step size used by GUI (hardware min = 1/16; 1/16..32/16 depending on actual gain ) */
#define SC132GS_MAX_GAIN_AEC     ( 32.0f )       /**< max. gain used by the AEC (arbitrarily chosen, hardware limit = 62.0, driver limit = 32.0 ) */
#define SC132GS_VS_MAX_INTEGRATION_TIME (0.0018)

/*****************************************************************************
 *Sensor Info
*****************************************************************************/
static const char SensorName[16] = "SC132GS";

static struct vvcam_mode_info psc132gs_mode_info[] = {
    {
        .index     = 0,
        .width     = 1080,
        .height    = 1280,
        .fps       = 30,
        .hdr_mode  = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_BGGR,
        .mipi_phy_freq = 607, //mbps
        .mipi_line_num = 2,
        .preg_data = (void *)"sc132gs sensor liner mode, raw10, 607mbps(sensor clk 24m), img resolution is 1080*1280",
    },
    {
        .index     = 1,
        .width     = 1080,
        .height    = 1280,
        .fps       = 30,
        .hdr_mode  = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_BGGR,
        .mipi_phy_freq = 607, //mbps
        .mipi_line_num = 2,
        .preg_data = (void *)"sc132gs sensor liner mode, raw10, 607mbps(sensor clk 24m), img resolution is 1080*1280, dual camera synchronization mode, sensor is master",
    },
    {
        .index     = 2,
        .width     = 1080,
        .height    = 1280,
        .fps       = 30,
        .hdr_mode  = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_BGGR,
        .mipi_phy_freq = 607, //mbps
        .mipi_line_num = 2,
        .preg_data = (void *)"sc132gs sensor liner mode, raw10, 607mbps(sensor clk 24m), img resolution is 1080*1280, dual camera synchronization mode, sensor is slave",
    },
    {
        .index     = 3,
        .width     = 960,
        .height    = 1280,
        .fps       = 30,
        .hdr_mode  = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_BGGR,
        .mipi_phy_freq = 360, //mbps
        .mipi_line_num = 2,
        .preg_data = (void *)"sc132gs sensor liner mode, raw10 30fps, 360mbps, img resolution is 960*1280",
    },
    {
        .index     = 4,
        .width     = 960,
        .height    = 1280,
        .fps       = 30,
        .hdr_mode  = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_BGGR,
        .mipi_phy_freq = 360, //mbps
        .mipi_line_num = 2,
        .preg_data = (void *)"sc132gs sensor liner mode, raw10 30fps, 360mbps, img resolution is 960*1280, dual camera synchronization mode, sensor is master",
    },
    {
        .index     = 5,
        .width     = 960,
        .height    = 1280,
        .fps       = 30,
        .hdr_mode  = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_BGGR,
        .mipi_phy_freq = 360, //mbps
        .mipi_line_num = 2,
        .preg_data = (void *)"sc132gs sensor liner mode, raw10 30fps, 360mbps, img resolution is 960*1280, dual camera synchronization mode, sensor is slave",
    },

};

static RESULT SC132GS_IsiRegisterWriteIss(IsiSensorHandle_t handle, const uint32_t address, const uint32_t value);

typedef struct {
    int ana_reg_val;
    float step;
    float max_val;
} sc132gs_gain_map_t;

static sc132gs_gain_map_t sc132gs_gain_map[] = {
    {0x03, 0.031, 1},
    {0x03, 0.031, 1.781},
    {0x023, 0.056, 3.568},
    {0x027, 0.114, 7.137},
    {0x02f, 0.226, 14.273},
    {0x03f, 0.450, 28.547},
};

static int sc132gs_set_gain(IsiSensorHandle_t handle, float gain, float *set_gain)
{
//Normal 模式/ HDR 模式下的长曝光数据
#define ANA_GAIN 0x3e08
#define ANA_FINE 0x3e09
//HDR 模式下的短曝光数据
#define ANA_VS_GAIN 0x3e12
#define ANA_VS_FINE 0x3e13

    int ret = 0;
    int i = 0;
    uint32_t ana_gain_val = 0;
    uint32_t ana_fine_val = 0;

    if (gain <= 1.0) {
        ana_gain_val = 0x3;
        ana_fine_val = 0x20;
        *set_gain = 1;
    } else if (gain >= 28.547) {
        ana_gain_val = 0x3f;
        ana_fine_val = 0x3f;
        *set_gain = 28.547;
    } else {
        for(i = 0; i < sizeof(sc132gs_gain_map) / sizeof(sc132gs_gain_map[0]); i++) {
            if (sc132gs_gain_map[i].max_val > gain) {
                ana_gain_val = sc132gs_gain_map[i].ana_reg_val;
                ana_fine_val =  0x20 + (gain - sc132gs_gain_map[i - 1].max_val) / sc132gs_gain_map[i].step;
                *set_gain = sc132gs_gain_map[i - 1].max_val + (gain - sc132gs_gain_map[i - 1].max_val) / sc132gs_gain_map[i].step;
                break;
            }
        }

        if (ana_fine_val > 0x3f) {
            ana_fine_val = 0x3f;
            *set_gain = sc132gs_gain_map[i].max_val;
        }
    }

    ret |= SC132GS_IsiRegisterWriteIss(handle, ANA_GAIN, ana_gain_val);
    ret |= SC132GS_IsiRegisterWriteIss(handle, ANA_FINE, ana_fine_val);

    if (ret != 0) {
        return -1;
    }

    return  ret;
}

long __sc132gs_set_exposure(IsiSensorHandle_t handle, int coarse_itg,
				 int gain, int digitgain, SC132GS_EXPOSURE_SETTING_t type)

{

	return 0;
}

static RESULT SC132GS_IsiSensorSetPowerIss(IsiSensorHandle_t handle, bool_t on) {
    RESULT result = RET_SUCCESS;

    int ret = 0;
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    TRACE(SC132GS_INFO, "%s (enter)\n", __func__);

    int32_t enable = on;
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_POWER, &enable);
    if (ret != 0) {
        // to do
        //TRACE(SC132GS_ERROR, "%s: sensor set power error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(SC132GS_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT SC132GS_IsiResetSensorIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;
    int ret = 0;

    TRACE(SC132GS_INFO, "%s (enter)\n", __func__);

    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_RESET, NULL);
    if (ret != 0) {
        TRACE(SC132GS_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }
    sleep(0.01);

    ret = SC132GS_IsiRegisterWriteIss(handle, 0x103, 1);
    if (ret != 0) {
        TRACE(SC132GS_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }

    sleep(0.01);

    ret = SC132GS_IsiRegisterWriteIss(handle, 0x103, 0);
    if (ret != 0) {
        TRACE(SC132GS_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }

    TRACE(SC132GS_INFO, "%s (exit)\n", __func__);
    return (result);
}

#ifdef SUBDEV_CHAR
static RESULT SC132GS_IsiSensorSetClkIss(IsiSensorHandle_t handle, uint32_t clk) {
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;

    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    TRACE(SC132GS_INFO, "%s (enter)\n", __func__);

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_CLK, &clk);
    if (ret != 0) {
        // to do
        //TRACE(SC132GS_ERROR, "%s: sensor set clk error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(SC132GS_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT SC132GS_IsiSensorGetClkIss
    (IsiSensorHandle_t handle, uint32_t * pclk) {
    RESULT result = RET_SUCCESS;
    int ret = 0;

    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    TRACE(SC132GS_INFO, "%s (enter)\n", __func__);

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_CLK, pclk);
    if (ret != 0) {
        // to do
        //TRACE(SC132GS_ERROR, "%s: sensor get clk error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(SC132GS_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT SC132GS_IsiConfigSensorSCCBIss(IsiSensorHandle_t handle)
{
    RESULT result = RET_SUCCESS;
    return result;
    int ret = 0;
    TRACE(SC132GS_INFO, "%s (enter)\n", __func__);

    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    static const IsiSccbInfo_t SensorSccbInfo = {
        .slave_addr = (0x31),  //0x30 or 0x32
        .addr_byte = 2,
        .data_byte = 1,
    };

    struct vvcam_sccb_cfg_s sensor_sccb_config;
    sensor_sccb_config.slave_addr = SensorSccbInfo.slave_addr;
    sensor_sccb_config.addr_byte = SensorSccbInfo.addr_byte;
    sensor_sccb_config.data_byte = SensorSccbInfo.data_byte;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_SENSOR_SCCB_CFG,
          &sensor_sccb_config);
    if (ret != 0) {
        TRACE(SC132GS_ERROR, "%s: sensor config sccb info error!\n",
              __func__);
        return (RET_FAILURE);
    }

    TRACE(SC132GS_INFO, "%s (exit) result = %d\n", __func__, result);
    return (result);

    return RET_SUCCESS;
}
#endif

static RESULT SC132GS_IsiRegisterReadIss
    (IsiSensorHandle_t handle, const uint32_t address, uint32_t * p_value) {
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    TRACE(SC132GS_INFO, "%s (enter)\n", __func__);

    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    struct vvcam_sccb_data sccb_data;
    sccb_data.addr = address;
    sccb_data.data = 0;
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_READ_REG, &sccb_data);
    if (ret != 0) {
        TRACE(SC132GS_ERROR, "%s: read sensor register error!\n",
              __func__);
        return (RET_FAILURE);
    }

    *p_value = sccb_data.data;

    TRACE(SC132GS_INFO, "%s (exit) result = %d\n", __func__, result);
    return (result);
}

static RESULT SC132GS_IsiRegisterWriteIss
    (IsiSensorHandle_t handle, const uint32_t address, const uint32_t value) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    TRACE(SC132GS_INFO, "%s (enter)\n", __func__);

    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    struct vvcam_sccb_data sccb_data;
    sccb_data.addr = address;
    sccb_data.data = value;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_WRITE_REG, &sccb_data);
    if (ret != 0) {
        TRACE(SC132GS_ERROR, "%s: write sensor register error!\n",
              __func__);
        return (RET_FAILURE);
    }

    TRACE(SC132GS_INFO, "%s (exit) result = %d\n", __func__, result);
    return (result);
}

static RESULT SC132GS_IsiQuerySensorSupportIss(HalHandle_t  HalHandle, vvcam_mode_info_array_t *pSensorSupportInfo)
{
    //int ret = 0;
    struct vvcam_mode_info_array *psensor_mode_info_arry;

    HalContext_t *pHalCtx = HalHandle;
    if ( pHalCtx == NULL ) {
        return RET_NULL_POINTER;
    }

    psensor_mode_info_arry = pSensorSupportInfo;
    psensor_mode_info_arry->count = sizeof(psc132gs_mode_info) / sizeof(struct vvcam_mode_info);
    memcpy(psensor_mode_info_arry->modes, psc132gs_mode_info, sizeof(psc132gs_mode_info));
    return RET_SUCCESS;
}

static  RESULT SC132GS_IsiQuerySensorIss(IsiSensorHandle_t handle, vvcam_mode_info_array_t *pSensorInfo)
{
    RESULT result = RET_SUCCESS;
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;
    SC132GS_IsiQuerySensorSupportIss(pHalCtx,pSensorInfo);

    return result;
}

static RESULT SC132GS_IsiGetSensorModeIss(IsiSensorHandle_t handle,void *mode)
{
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL) {
        return (RET_WRONG_HANDLE);
    }
    memcpy(mode,&(pSC132GSCtx->SensorMode), sizeof(pSC132GSCtx->SensorMode));

    return ( RET_SUCCESS );
}

static RESULT SC132GS_IsiCreateSensorIss(IsiSensorInstanceConfig_t * pConfig) {
    RESULT result = RET_SUCCESS;
    SC132GS_Context_t *pSC132GSCtx;

    TRACE(SC132GS_INFO, "%s (enter)\n", __func__);

    if (!pConfig || !pConfig->pSensor)
        return (RET_NULL_POINTER);

    pSC132GSCtx = (SC132GS_Context_t *) malloc(sizeof(SC132GS_Context_t));
    if (!pSC132GSCtx) {
        TRACE(SC132GS_ERROR, "%s: Can't allocate sc132gs context\n",
              __func__);
        return (RET_OUTOFMEM);
    }

    MEMSET(pSC132GSCtx, 0, sizeof(SC132GS_Context_t));

    result = HalAddRef(pConfig->HalHandle);
    if (result != RET_SUCCESS) {
        free(pSC132GSCtx);
        return (result);
    }

    pSC132GSCtx->IsiCtx.HalHandle = pConfig->HalHandle;
    pSC132GSCtx->IsiCtx.pSensor = pConfig->pSensor;
    pSC132GSCtx->GroupHold = BOOL_FALSE;
    pSC132GSCtx->OldGain = 0;
    pSC132GSCtx->OldIntegrationTime = 0;
    pSC132GSCtx->Configured = BOOL_FALSE;
    pSC132GSCtx->Streaming = BOOL_FALSE;
    pSC132GSCtx->TestPattern = BOOL_FALSE;
    pSC132GSCtx->isAfpsRun = BOOL_FALSE;
    pSC132GSCtx->SensorMode.index = pConfig->SensorModeIndex;
    pConfig->hSensor = (IsiSensorHandle_t) pSC132GSCtx;
#ifdef SUBDEV_CHAR
    struct vvcam_mode_info *SensorDefaultMode = NULL;
    for (int i=0; i < sizeof(psc132gs_mode_info)/ sizeof(struct vvcam_mode_info); i++)
    {
        if (psc132gs_mode_info[i].index == pSC132GSCtx->SensorMode.index)
        {
            SensorDefaultMode = &(psc132gs_mode_info[i]);
            break;
        }
    }

    if (SensorDefaultMode != NULL)
    {
        strcpy(pSC132GSCtx->SensorRegCfgFile, get_vi_config_path());
        switch(SensorDefaultMode->index)
        {
            case 0:
                strcat(pSC132GSCtx->SensorRegCfgFile,
                    "SC132GS_mipi2lane_1080x1280_init.txt");
                break;
            case 1:
                strcat(pSC132GSCtx->SensorRegCfgFile,
                    "SC132GS_mipi2lane_1080x1280_master_init.txt");
                break;

            case 2:
                strcat(pSC132GSCtx->SensorRegCfgFile,
                    "SC132GS_mipi2lane_1080x1280_slave_init.txt");
                break;
            case 3:
                strcat(pSC132GSCtx->SensorRegCfgFile,
                    "SC132GS_mipi2lane_960x1280_init.txt");
                break;
            case 4:
                strcat(pSC132GSCtx->SensorRegCfgFile,
                    "SC132GS_mipi2lane_960x1280_master_init.txt");
                break;
            case 5:
                strcat(pSC132GSCtx->SensorRegCfgFile,
                    "SC132GS_mipi2lane_960x1280_slave_init.txt");
                break;
            default:
                return -1;
        }

        if (access(pSC132GSCtx->SensorRegCfgFile, F_OK) == 0) {
            pSC132GSCtx->KernelDriverFlag = 0;
            memcpy(&(pSC132GSCtx->SensorMode),SensorDefaultMode,sizeof(struct vvcam_mode_info));
        } else {
            return -1;
        }
    }else
    {
        pSC132GSCtx->KernelDriverFlag = 1;
    }

    result = SC132GS_IsiSensorSetPowerIss(pSC132GSCtx, BOOL_TRUE);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    uint32_t SensorClkIn = 0;
    if (pSC132GSCtx->KernelDriverFlag) {
        result = SC132GS_IsiSensorGetClkIss(pSC132GSCtx, &SensorClkIn);
        RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);
    }

    result = SC132GS_IsiSensorSetClkIss(pSC132GSCtx, SensorClkIn);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    result = SC132GS_IsiResetSensorIss(pSC132GSCtx);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    pSC132GSCtx->pattern = ISI_BPAT_BGBGGRGR;

    if (!pSC132GSCtx->KernelDriverFlag) {
        result = SC132GS_IsiConfigSensorSCCBIss(pSC132GSCtx);
        RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);
    }
#endif

    TRACE(SC132GS_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT SC132GS_IsiGetRegCfgIss(const char *registerFileName,
                     struct vvcam_sccb_array *arry)
{
    if (NULL == registerFileName) {
        TRACE(SC132GS_ERROR, "%s:registerFileName is NULL\n", __func__);
        return (RET_NULL_POINTER);
    }
#ifdef SUBDEV_CHAR
    FILE *fp = NULL;
    fp = fopen(registerFileName, "rb");
    if (!fp) {
        TRACE(SC132GS_ERROR, "%s:load register file  %s error!\n",
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
        TRACE(SC132GS_ERROR, "%s:malloc failed NULL Point!\n", __func__,
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

static RESULT SC132GS_IsiInitSensorIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;

    int ret = 0;
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;

    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;
    TRACE(SC132GS_INFO, "%s (enter)\n", __func__);

    if (pSC132GSCtx == NULL) {
        return (RET_WRONG_HANDLE);
    }

    if (pSC132GSCtx->KernelDriverFlag) {
        ;
    } else {
        struct vvcam_sccb_array arry;
        result = SC132GS_IsiGetRegCfgIss(pSC132GSCtx->SensorRegCfgFile, &arry);
        if (result != 0) {
            TRACE(SC132GS_ERROR,
                  "%s:SC132GS_IsiGetRegCfgIss error!\n", __func__);
            return (RET_FAILURE);
        }

        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_WRITE_ARRAY, &arry);
        if (ret != 0) {
            TRACE(SC132GS_ERROR, "%s:Sensor Write Reg arry error!\n",
                  __func__);
            return (RET_FAILURE);
        }

        switch(pSC132GSCtx->SensorMode.index)
        {
            case 0:
                pSC132GSCtx->one_line_exp_time = 0.81257f; //us 1/16  * line_time
                pSC132GSCtx->FrameLengthLines = (0x546 - 8) * 16;
                pSC132GSCtx->CurFrameLengthLines = pSC132GSCtx->FrameLengthLines;
                pSC132GSCtx->MaxIntegrationLine = pSC132GSCtx->CurFrameLengthLines - 3;
                pSC132GSCtx->MinIntegrationLine = 1;
                pSC132GSCtx->AecMaxGain = 28;
                pSC132GSCtx->AecMinGain = 1;
                break;
            case 1:
                pSC132GSCtx->one_line_exp_time = 0.81257f; //us 1/16  * line_time
                pSC132GSCtx->FrameLengthLines = (0x546 - 8) * 16;
                pSC132GSCtx->CurFrameLengthLines = pSC132GSCtx->FrameLengthLines;
                pSC132GSCtx->MaxIntegrationLine = pSC132GSCtx->CurFrameLengthLines - 3;
                pSC132GSCtx->MinIntegrationLine = 1;
                pSC132GSCtx->AecMaxGain = 28;
                pSC132GSCtx->AecMinGain = 1;
                break;
            case 2:
                pSC132GSCtx->one_line_exp_time = 0.81257f; //us 1/16  * line_time
                pSC132GSCtx->FrameLengthLines = (0x546 - 8) * 16;
                pSC132GSCtx->CurFrameLengthLines = pSC132GSCtx->FrameLengthLines;
                pSC132GSCtx->MaxIntegrationLine = pSC132GSCtx->CurFrameLengthLines - 3;
                pSC132GSCtx->MinIntegrationLine = 1;
                pSC132GSCtx->AecMaxGain = 28;
                pSC132GSCtx->AecMinGain = 1;
                break;
            case 3:
                pSC132GSCtx->one_line_exp_time = 0.81257f; //us 1/16  * line_time
                pSC132GSCtx->FrameLengthLines = (0x546 - 8) * 16;
                pSC132GSCtx->CurFrameLengthLines = pSC132GSCtx->FrameLengthLines;
                pSC132GSCtx->MaxIntegrationLine = pSC132GSCtx->CurFrameLengthLines - 3;
                pSC132GSCtx->MinIntegrationLine = 1;
                pSC132GSCtx->AecMaxGain = 28;
                pSC132GSCtx->AecMinGain = 1;
                break;
            case 4:
                pSC132GSCtx->one_line_exp_time = 0.81257f; //us 1/16  * line_time
                pSC132GSCtx->FrameLengthLines = (0x546 - 8) * 16;
                pSC132GSCtx->CurFrameLengthLines = pSC132GSCtx->FrameLengthLines;
                pSC132GSCtx->MaxIntegrationLine = pSC132GSCtx->CurFrameLengthLines - 3;
                pSC132GSCtx->MinIntegrationLine = 1;
                pSC132GSCtx->AecMaxGain = 28;
                pSC132GSCtx->AecMinGain = 1;
                break;
            case 5:
                pSC132GSCtx->one_line_exp_time = 0.81257f; //us 1/16  * line_time
                pSC132GSCtx->FrameLengthLines = (0x546 - 8) * 16;
                pSC132GSCtx->CurFrameLengthLines = pSC132GSCtx->FrameLengthLines;
                pSC132GSCtx->MaxIntegrationLine = pSC132GSCtx->CurFrameLengthLines - 3;
                pSC132GSCtx->MinIntegrationLine = 1;
                pSC132GSCtx->AecMaxGain = 28;
                pSC132GSCtx->AecMinGain = 1;
                break;
            default:
                return ( RET_NOTAVAILABLE );
        }

        pSC132GSCtx->AecIntegrationTimeIncrement = pSC132GSCtx->one_line_exp_time;
        pSC132GSCtx->AecMinIntegrationTime =
            pSC132GSCtx->one_line_exp_time * pSC132GSCtx->MinIntegrationLine;
        pSC132GSCtx->AecMaxIntegrationTime =
            pSC132GSCtx->one_line_exp_time * pSC132GSCtx->FrameLengthLines;

        pSC132GSCtx->MaxFps  = pSC132GSCtx->SensorMode.fps;
        pSC132GSCtx->MinFps  = 1;
        pSC132GSCtx->CurrFps = pSC132GSCtx->MaxFps;
    }

    TRACE(SC132GS_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT SC132GS_IsiReleaseSensorIss(IsiSensorHandle_t handle) {
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(SC132GS_INFO, "%s (enter)\n", __func__);

    if (pSC132GSCtx == NULL)
        return (RET_WRONG_HANDLE);

    (void)SC132GS_IsiSensorSetStreamingIss(pSC132GSCtx, BOOL_FALSE);
    (void)SC132GS_IsiSensorSetPowerIss(pSC132GSCtx, BOOL_FALSE);
    (void)HalDelRef(pSC132GSCtx->IsiCtx.HalHandle);

    MEMSET(pSC132GSCtx, 0, sizeof(SC132GS_Context_t));
    free(pSC132GSCtx);
    TRACE(SC132GS_INFO, "%s (exit)\n", __func__);
    return (result);
}

struct sc132gs_fmt {
    int width;
    int height;
    int fps;
};

static RESULT SC132GS_IsiSetupSensorIss
    (IsiSensorHandle_t handle, const IsiSensorConfig_t * pConfig) {

    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    RESULT result = RET_SUCCESS;

    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (!pSC132GSCtx) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pConfig) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid configuration (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    if (pSC132GSCtx->Streaming != BOOL_FALSE) {
        return RET_WRONG_STATE;
    }

    memcpy(&pSC132GSCtx->Config, pConfig, sizeof(IsiSensorConfig_t));

    /* 1.) SW reset of image sensor (via I2C register interface)  be careful, bits 6..0 are reserved, reset bit is not sticky */
    TRACE(SC132GS_DEBUG, "%s: SC132GS System-Reset executed\n", __func__);
    osSleep(100);

    //SC132GS_AecSetModeParameters not defined yet as of 2021/8/9.
    //result = SC132GS_AecSetModeParameters(pSC132GSCtx, pConfig);
    //if (result != RET_SUCCESS) {
    //    TRACE(SC132GS_ERROR, "%s: SetupOutputWindow failed.\n",
    //          __func__);
    //    return (result);
    //}
#if 1
    struct sc132gs_fmt fmt;
    fmt.width = pConfig->Resolution.width;
    fmt.height = pConfig->Resolution.height;

    ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fmt);//result = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fmt);
#endif
    pSC132GSCtx->Configured = BOOL_TRUE;
    TRACE(SC132GS_INFO, "%s: (exit) ret=0x%x \n", __func__, result);
    return result;
}

static RESULT SC132GS_IsiChangeSensorResolutionIss(IsiSensorHandle_t handle, uint16_t width, uint16_t height) {
    RESULT result = RET_SUCCESS;
#if 0
    struct sc132gs_fmt fmt;
    fmt.width = width;
    fmt.height = height;

    int ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fmt);
#endif
    TRACE(SC132GS_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT SC132GS_IsiSensorSetStreamingIss
    (IsiSensorHandle_t handle, bool_t on) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    TRACE(SC132GS_INFO, "%s (enter)\n", __func__);

    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    if (pSC132GSCtx->Configured != BOOL_TRUE)
        return RET_WRONG_STATE;

    int32_t enable = (uint32_t) on;

    if (on == 0) {
        ret = SC132GS_IsiRegisterWriteIss(handle, 0x3800, 0x00);
        if (ret != 0) {
            return (RET_FAILURE);
        }

        ret = SC132GS_IsiRegisterWriteIss(handle, 0x3817, 0x01);
        if (ret != 0) {
            return (RET_FAILURE);
        }

        ret = SC132GS_IsiRegisterWriteIss(handle, 0x100, on);
        if (ret != 0) {
            return (RET_FAILURE);
        }

        ret = SC132GS_IsiRegisterWriteIss(handle, 0x3800, 0x10);
        if (ret != 0) {
            return (RET_FAILURE);
        }

        ret = SC132GS_IsiRegisterWriteIss(handle, 0x3800, 0x40);
        if (ret != 0) {
            return (RET_FAILURE);
        }
    } else {
        ret = SC132GS_IsiRegisterWriteIss(handle, 0x100, on);
        if (ret != 0) {
            return (RET_FAILURE);
        }
    }

    pSC132GSCtx->Streaming = on;

    TRACE(SC132GS_INFO, "%s (exit)\n", __func__);
    return (result);
}

static int32_t sensor_get_chip_id(IsiSensorHandle_t handle, uint32_t *chip_id)
{
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    int32_t chip_id_high = 0;
    int32_t chip_id_low = 0;

    ret = SC132GS_IsiRegisterReadIss(handle, 0x3107, &chip_id_high);
    if (ret != 0) {
        TRACE(SC132GS_ERROR,
            "%s: Read Sensor correct ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    ret = SC132GS_IsiRegisterReadIss(handle, 0x3108, &chip_id_low);
    if (ret != 0) {
        TRACE(SC132GS_ERROR,
            "%s: Read Sensor correct ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    *chip_id = ((chip_id_high & 0xff)<<8) | (chip_id_low & 0xff);

    return 0;
}

static RESULT SC132GS_IsiCheckSensorConnectionIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    uint32_t correct_id = 0x132;
    uint32_t sensor_id = 0;

    TRACE(SC132GS_INFO, "%s (enter)\n", __func__);

    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    ret = sensor_get_chip_id(handle, &sensor_id);
    if (ret != 0) {
        TRACE(SC132GS_ERROR,
            "%s: Read Sensor chip ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    if (correct_id != sensor_id) {
        TRACE(SC132GS_ERROR, "%s:ChipID =0x%x sensor_id=%x error! \n",
              __func__, correct_id, sensor_id);
        return (RET_FAILURE);
    }

    TRACE(SC132GS_INFO,
          "%s ChipID = 0x%08x, sensor_id = 0x%08x, success! \n", __func__,
          correct_id, sensor_id);
    TRACE(SC132GS_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT SC132GS_IsiGetSensorRevisionIss
    (IsiSensorHandle_t handle, uint32_t * p_value) {
    RESULT result = RET_SUCCESS;
    TRACE(SC132GS_INFO, "%s (enter)\n", __func__);

    *p_value = 0X132;
    TRACE(SC132GS_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT SC132GS_IsiGetGainLimitsIss
    (IsiSensorHandle_t handle, float *pMinGain, float *pMaxGain) {
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    /*TODO*/

    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (pSC132GSCtx == NULL) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pMinGain == NULL) || (pMaxGain == NULL)) {
        TRACE(SC132GS_ERROR, "%s: NULL pointer received!!\n");
        return (RET_NULL_POINTER);
    }

    *pMinGain = pSC132GSCtx->AecMinGain;
    *pMaxGain = pSC132GSCtx->AecMaxGain;

    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);
    return (result);
}

static RESULT SC132GS_IsiGetIntegrationTimeLimitsIss
    (IsiSensorHandle_t handle,
     float *pMinIntegrationTime, float *pMaxIntegrationTime) {
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    /*TODO*/

    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);
    if (pSC132GSCtx == NULL) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pMinIntegrationTime == NULL) || (pMaxIntegrationTime == NULL)) {
        TRACE(SC132GS_ERROR, "%s: NULL pointer received!!\n");
        return (RET_NULL_POINTER);
    }

    *pMinIntegrationTime = pSC132GSCtx->AecMinIntegrationTime;
    *pMaxIntegrationTime = pSC132GSCtx->AecMaxIntegrationTime;

    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);
    return (result);
}

RESULT SC132GS_IsiGetGainIss(IsiSensorHandle_t handle, float *pSetGain) {
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (pSC132GSCtx == NULL) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pSetGain == NULL) {
        return (RET_NULL_POINTER);
    }

    *pSetGain = pSC132GSCtx->AecCurGain;

    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC132GS_IsiGetLongGainIss(IsiSensorHandle_t handle, float *gain)
{
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;

    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (pSC132GSCtx == NULL) {
        TRACE(SC132GS_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }

    if (gain == NULL) {
        return (RET_NULL_POINTER);
    }

    *gain = pSC132GSCtx->AecCurLongGain;

    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);

    return (RET_SUCCESS);
}

RESULT SC132GS_IsiGetVSGainIss(IsiSensorHandle_t handle, float *pSetGain) {
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (pSC132GSCtx == NULL) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pSetGain == NULL) {
        return (RET_NULL_POINTER);
    }

    *pSetGain = pSC132GSCtx->AecCurVSGain;

    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);

    return (result);
}

RESULT SC132GS_IsiGetGainIncrementIss(IsiSensorHandle_t handle, float *pIncr) {
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (pSC132GSCtx == NULL) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pIncr == NULL)
        return (RET_NULL_POINTER);

    *pIncr = pSC132GSCtx->AecGainIncrement;

    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);

    return (result);
}

RESULT SC132GS_IsiSetGainIss
    (IsiSensorHandle_t handle,
     float NewGain, float *pSetGain, float *hdr_ratio) {

    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    int TmpGain;

    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    if (NewGain >= 25) { // More than 25 will not take effect
        NewGain = 25;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    sc132gs_set_gain(handle, NewGain, pSetGain);
    pSC132GSCtx->AecCurGain = *pSetGain;

    TRACE(SC132GS_DEBUG, "%s: g=%f\n", __func__, *pSetGain);
    return (result);
}

RESULT SC132GS_IsiSetLongGainIss(IsiSensorHandle_t handle, float gain)
{
    int ret = 0;
    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;

    if (!pSC132GSCtx || !pSC132GSCtx->IsiCtx.HalHandle)
    {
        TRACE(SC132GS_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }

    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    uint32_t SensorGain = 0;
    SensorGain = gain * pSC132GSCtx->gain_accuracy;
    if (pSC132GSCtx->LastLongGain != SensorGain)
    {

        /*TODO*/
#if 0
        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_LONG_GAIN, &SensorGain);
        if (ret != 0)
        {
            return (RET_FAILURE);
            TRACE(SC132GS_ERROR,"%s: set long gain failed\n");

        }
#endif
        pSC132GSCtx->LastLongGain = SensorGain;
        pSC132GSCtx->AecCurLongGain = gain;
    }

    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT SC132GS_IsiSetVSGainIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float NewGain, float *pSetGain, float *hdr_ratio) {
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;
    RESULT result = RET_SUCCESS;
#if 0
    float Gain = 0.0f;

    uint32_t ucGain = 0U;
    uint32_t again = 0U;
#endif

    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (!pSC132GSCtx) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetGain || !hdr_ratio)
        return (RET_NULL_POINTER);

    uint32_t SensorGain = 0;
    SensorGain = NewGain * pSC132GSCtx->gain_accuracy;

    /*TODO*/
    //ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_VSGAIN, &SensorGain);

    pSC132GSCtx->AecCurVSGain = NewGain;
    *pSetGain = pSC132GSCtx->AecCurGain;
    TRACE(SC132GS_DEBUG, "%s: g=%f\n", __func__, *pSetGain);
    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC132GS_IsiSetBayerPattern(IsiSensorHandle_t handle, uint8_t pattern)
{

    RESULT result = RET_SUCCESS;
#if 0
    uint8_t h_shift = 0, v_shift = 0;
    uint32_t val_h = 0, val_l = 0;
    uint16_t val = 0;
    uint8_t Start_p = 0;
    bool_t streaming_status;
    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    // pattern 0:B 1:GB 2:GR 3:R
    streaming_status = pSC132GSCtx->Streaming;
    result = SC132GS_IsiSensorSetStreamingIss(handle, 0);
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

    SC132GS_IsiRegisterReadIss(handle, 0x30a0, &val_h);
    SC132GS_IsiRegisterReadIss(handle, 0x30a1, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + h_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    SC132GS_IsiRegisterWriteIss(handle, 0x30a0, (uint8_t)val_h);
    SC132GS_IsiRegisterWriteIss(handle, 0x30a1, (uint8_t)val_l);

    SC132GS_IsiRegisterReadIss(handle, 0x30a2, &val_h);
    SC132GS_IsiRegisterReadIss(handle, 0x30a3, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + v_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    SC132GS_IsiRegisterWriteIss(handle, 0x30a2, (uint8_t)val_h);
    SC132GS_IsiRegisterWriteIss(handle, 0x30a3, (uint8_t)val_l);

    SC132GS_IsiRegisterReadIss(handle, 0x30a4, &val_h);
    SC132GS_IsiRegisterReadIss(handle, 0x30a5, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + h_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    SC132GS_IsiRegisterWriteIss(handle, 0x30a4, (uint8_t)val_h);
    SC132GS_IsiRegisterWriteIss(handle, 0x30a5, (uint8_t)val_l);

    SC132GS_IsiRegisterReadIss(handle, 0x30a6, &val_h);
    SC132GS_IsiRegisterReadIss(handle, 0x30a7, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + v_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    SC132GS_IsiRegisterWriteIss(handle, 0x30a6, (uint8_t)val_h);
    SC132GS_IsiRegisterWriteIss(handle, 0x30a7, (uint8_t)val_l);

    pSC132GSCtx->pattern = pattern;
    result = SC132GS_IsiSensorSetStreamingIss(handle, streaming_status);
#endif

    return (result);
}

RESULT SC132GS_IsiGetIntegrationTimeIss
    (IsiSensorHandle_t handle, float *pSetIntegrationTime)
{
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (!pSC132GSCtx) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetIntegrationTime)
        return (RET_NULL_POINTER);
    *pSetIntegrationTime = pSC132GSCtx->AecCurIntegrationTime;
    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC132GS_IsiGetLongIntegrationTimeIss(IsiSensorHandle_t handle, float *pIntegrationTime)
{
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (!pSC132GSCtx) {
        TRACE(SC132GS_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }
    if (!pIntegrationTime)
        return (RET_NULL_POINTER);

    pSC132GSCtx->AecCurLongIntegrationTime =  pSC132GSCtx->AecCurIntegrationTime;

    *pIntegrationTime = pSC132GSCtx->AecCurLongIntegrationTime;
    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT SC132GS_IsiGetVSIntegrationTimeIss
    (IsiSensorHandle_t handle, float *pSetIntegrationTime)
{
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (!pSC132GSCtx) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    if (!pSetIntegrationTime)
        return (RET_NULL_POINTER);

    *pSetIntegrationTime = pSC132GSCtx->AecCurVSIntegrationTime;
    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC132GS_IsiGetIntegrationTimeIncrementIss
    (IsiSensorHandle_t handle, float *pIncr)
{
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (!pSC132GSCtx) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pIncr)
        return (RET_NULL_POINTER);

    //_smallest_ increment the sensor/driver can handle (e.g. used for sliders in the application)
    *pIncr = pSC132GSCtx->AecIntegrationTimeIncrement;
    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC132GS_IsiSetIntegrationTimeIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float *pSetIntegrationTime,
     uint8_t * pNumberOfFramesToSkip, float *hdr_ratio)
{
    RESULT result = RET_SUCCESS;
    uint32_t exp_lines = 0;

    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    NewIntegrationTime *= 1000000;  //us

    // 曝光时间小于3ms, 对应到寄存器值3692
    //if (NewIntegrationTime > 3000) {
    //    NewIntegrationTime = 3000;
    //}
    // time to lines
    exp_lines = NewIntegrationTime / pSC132GSCtx->one_line_exp_time;

    if (exp_lines > pSC132GSCtx->FrameLengthLines) {
        exp_lines = pSC132GSCtx->FrameLengthLines;
    }

    //行长 = 寄存器{16‘h320c, 16′h320d}值*2
    //2*{16’h320e,16’h320f}-6:h320e,h320f为帧长
    uint32_t hval_time =  (exp_lines & 0xf0000) >> 16;
    uint32_t mval_time =  (exp_lines & 0xff00) >> 8;
    uint32_t lval_time =  exp_lines & 0xff;

    result = SC132GS_IsiRegisterWriteIss(handle, 0x3e00, hval_time);
    result = SC132GS_IsiRegisterWriteIss(handle, 0x3e01, mval_time);
    result = SC132GS_IsiRegisterWriteIss(handle, 0x3e02, lval_time);

    pSC132GSCtx->AecCurIntegrationTime = exp_lines * pSC132GSCtx->one_line_exp_time;
    *pNumberOfFramesToSkip = 1U;
    *pSetIntegrationTime = pSC132GSCtx->AecCurIntegrationTime;

#if 0
    uint32_t exp_line = 0;
    uint32_t exp_line_old = 0;

    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (!pSC132GSCtx) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetIntegrationTime || !pNumberOfFramesToSkip) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    exp_line = NewIntegrationTime / pSC132GSCtx->one_line_exp_time;
    exp_line_old = exp_line;
    exp_line =
        MIN(pSC132GSCtx->MaxIntegrationLine,
        MAX(pSC132GSCtx->MinIntegrationLine, exp_line));

    TRACE(SC132GS_DEBUG, "%s: set AEC_PK_EXPO=0x%05x\n", __func__, exp_line);

    if (exp_line != pSC132GSCtx->OldIntegrationTime) {

        /*TODO*/
        //ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_EXP, &exp_line);
        pSC132GSCtx->OldIntegrationTime = exp_line;    // remember current integration time
        pSC132GSCtx->AecCurIntegrationTime =
            exp_line * pSC132GSCtx->one_line_exp_time;

        *pNumberOfFramesToSkip = 1U;    //skip 1 frame
    } else {
        *pNumberOfFramesToSkip = 0U;    //no frame skip
    }

    if (NewIntegrationTime > pSC132GSCtx->FrameLengthLines * pSC132GSCtx->one_line_exp_time)
        NewIntegrationTime = pSC132GSCtx->FrameLengthLines * pSC132GSCtx->one_line_exp_time;
    float exp_t = NewIntegrationTime * 16.0f / pSC132GSCtx->one_line_exp_time;
    __sc132gs_set_exposure(handle, (int)exp_t,
				0, 0, SC132GS_INTEGRATION_TIME);

    if (exp_line_old != exp_line) {
        *pSetIntegrationTime = pSC132GSCtx->AecCurIntegrationTime;
    } else {
        *pSetIntegrationTime = NewIntegrationTime;
    }
#endif

    TRACE(SC132GS_DEBUG, "%s: Ti=%f\n", __func__, *pSetIntegrationTime);
    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC132GS_IsiSetLongIntegrationTimeIss(IsiSensorHandle_t handle,float IntegrationTime)
{
    int ret;
    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (!handle || !pSC132GSCtx->IsiCtx.HalHandle)
    {
        TRACE(SC132GS_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    uint32_t exp_line = 0;
    exp_line = IntegrationTime / pSC132GSCtx->one_line_exp_time;
    exp_line = MIN(pSC132GSCtx->MaxIntegrationLine, MAX(pSC132GSCtx->MinIntegrationLine, exp_line));

    if (exp_line != pSC132GSCtx->LastLongExpLine)
    {
        if (pSC132GSCtx->KernelDriverFlag)
        {
            ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_LONG_EXP, &exp_line);
            if (ret != 0)
            {
                TRACE(SC132GS_ERROR,"%s: set long gain failed\n");
                return RET_FAILURE;
            }
        }

        pSC132GSCtx->LastLongExpLine = exp_line;
        pSC132GSCtx->AecCurLongIntegrationTime =  pSC132GSCtx->LastLongExpLine*pSC132GSCtx->one_line_exp_time;
    }


    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT SC132GS_IsiSetVSIntegrationTimeIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float *pSetVSIntegrationTime,
     uint8_t * pNumberOfFramesToSkip, float *hdr_ratio)
{
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    uint32_t exp_line = 0;

    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (!pSC132GSCtx) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetVSIntegrationTime || !pNumberOfFramesToSkip) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    TRACE(SC132GS_INFO,
          "%s:  maxIntegrationTime-=%f minIntegrationTime = %f\n", __func__,
          pSC132GSCtx->AecMaxIntegrationTime,
          pSC132GSCtx->AecMinIntegrationTime);


    exp_line = NewIntegrationTime / pSC132GSCtx->one_line_exp_time;
    exp_line =
        MIN(pSC132GSCtx->MaxIntegrationLine,
        MAX(pSC132GSCtx->MinIntegrationLine, exp_line));

    if (exp_line != pSC132GSCtx->OldVsIntegrationTime) {
    /*TODO*/
    //    ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_VSEXP, &exp_line);
    } else if (1){

        pSC132GSCtx->OldVsIntegrationTime = exp_line;
        pSC132GSCtx->AecCurVSIntegrationTime = exp_line * pSC132GSCtx->one_line_exp_time;    //remember current integration time
        *pNumberOfFramesToSkip = 1U;    //skip 1 frame
    } else {
        *pNumberOfFramesToSkip = 0U;    //no frame skip
    }

    *pSetVSIntegrationTime = pSC132GSCtx->AecCurVSIntegrationTime;

    TRACE(SC132GS_DEBUG, "%s: NewIntegrationTime=%f\n", __func__,
          NewIntegrationTime);
    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC132GS_IsiExposureControlIss
    (IsiSensorHandle_t handle,
     float NewGain,
     float NewIntegrationTime,
     uint8_t * pNumberOfFramesToSkip,
     float *pSetGain, float *pSetIntegrationTime, float *hdr_ratio)
{
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (pSC132GSCtx == NULL) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pNumberOfFramesToSkip == NULL) || (pSetGain == NULL)
        || (pSetIntegrationTime == NULL)) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    if (NewGain >= 28) {
        NewGain = 28;
    }

    TRACE(SC132GS_DEBUG, "%s: g=%f, Ti=%f\n", __func__, NewGain,
          NewIntegrationTime);

    if (NewIntegrationTime > pSC132GSCtx->FrameLengthLines * pSC132GSCtx->one_line_exp_time)
        NewIntegrationTime = pSC132GSCtx->FrameLengthLines * pSC132GSCtx->one_line_exp_time;

    sc132gs_set_gain(handle, NewGain, pSetGain);
    SC132GS_IsiSetIntegrationTimeIss(handle, NewIntegrationTime, pSetIntegrationTime, pNumberOfFramesToSkip, hdr_ratio);
    pSC132GSCtx->AecCurGain = NewGain;
    pSC132GSCtx->AecCurIntegrationTime = *pSetIntegrationTime;

    TRACE(SC132GS_DEBUG, "%s: set: vsg=%f, vsTi=%f, vsskip=%d\n", __func__,
          NewGain, NewIntegrationTime, *pNumberOfFramesToSkip);
    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);

    return result;
}

RESULT SC132GS_IsiGetCurrentExposureIss
    (IsiSensorHandle_t handle, float *pSetGain, float *pSetIntegrationTime, float *hdr_ratio) {
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (pSC132GSCtx == NULL) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pSetGain == NULL) || (pSetIntegrationTime == NULL))
        return (RET_NULL_POINTER);

    *pSetGain = pSC132GSCtx->AecCurGain;
    *pSetIntegrationTime = pSC132GSCtx->AecCurIntegrationTime;
    *hdr_ratio = pSC132GSCtx->CurHdrRatio;

    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC132GS_IsiGetResolutionIss(IsiSensorHandle_t handle, uint16_t *pwidth, uint16_t *pheight) {
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (pSC132GSCtx == NULL) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    *pwidth = pSC132GSCtx->SensorMode.width;
    *pheight =  pSC132GSCtx->SensorMode.height;

    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC132GS_IsiGetSensorFpsIss(IsiSensorHandle_t handle, uint32_t * pfps)
{
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    RESULT result = RET_SUCCESS;


    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (pSC132GSCtx == NULL) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    if (pSC132GSCtx->KernelDriverFlag) {
       /*TODO*/
        ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_FPS, pfps);
        pSC132GSCtx->CurrFps = *pfps;
    }

    *pfps = pSC132GSCtx->CurrFps;

    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT SC132GS_IsiSetSensorFpsIss(IsiSensorHandle_t handle, uint32_t fps)
{
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    if (pSC132GSCtx == NULL) {
        TRACE(SC132GS_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    if (fps > pSC132GSCtx->MaxFps) {
        TRACE(SC132GS_ERROR,
              "%s: set fps(%d) out of range, correct to %d (%d, %d)\n",
              __func__, fps, pSC132GSCtx->MaxFps, pSC132GSCtx->MinFps,
              pSC132GSCtx->MaxFps);
        fps = pSC132GSCtx->MaxFps;
    }
    if (fps < pSC132GSCtx->MinFps) {
        TRACE(SC132GS_ERROR,
              "%s: set fps(%d) out of range, correct to %d (%d, %d)\n",
              __func__, fps, pSC132GSCtx->MinFps, pSC132GSCtx->MinFps,
              pSC132GSCtx->MaxFps);
        fps = pSC132GSCtx->MinFps;
    }
    if (pSC132GSCtx->KernelDriverFlag) {
        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fps);
        if (ret != 0) {
            TRACE(SC132GS_ERROR, "%s: set sensor fps=%d error\n",
                  __func__);
            return (RET_FAILURE);
        }

        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_SENSOR_MODE, &(pSC132GSCtx->SensorMode));
        {
            pSC132GSCtx->MaxIntegrationLine = pSC132GSCtx->SensorMode.ae_info.max_integration_time;
            pSC132GSCtx->AecMaxIntegrationTime = pSC132GSCtx->MaxIntegrationLine * pSC132GSCtx->one_line_exp_time;
        }
#ifdef SUBDEV_CHAR
        struct vvcam_ae_info_s ae_info;
        ret =
            ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_AE_INFO, &ae_info);
        if (ret != 0) {
            TRACE(SC132GS_ERROR, "%s:sensor get ae info error!\n",
                  __func__);
            return (RET_FAILURE);
        }
        pSC132GSCtx->one_line_exp_time =
            (float)ae_info.one_line_exp_time_ns / 1000000000;
        pSC132GSCtx->MaxIntegrationLine = ae_info.max_integration_time;
        pSC132GSCtx->AecMaxIntegrationTime =
            pSC132GSCtx->MaxIntegrationLine *
            pSC132GSCtx->one_line_exp_time;
#endif
    }

    TRACE(SC132GS_INFO, "%s: set sensor fps = %d\n", __func__,
          pSC132GSCtx->CurrFps);

    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);
    return (result);
}

static RESULT SC132GS_IsiActivateTestPattern(IsiSensorHandle_t handle,
                        const bool_t enable)
{
    RESULT result = RET_SUCCESS;

    TRACE(SC132GS_INFO, "%s: (enter)\n", __func__);

    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    if (pSC132GSCtx->Configured != BOOL_TRUE)
        return RET_WRONG_STATE;

    if (BOOL_TRUE == enable) {
        //result = SC132GS_IsiRegisterWriteIss(handle, 0x3253, 0x80);
    } else {
        //result = SC132GS_IsiRegisterWriteIss(handle, 0x3253, 0x00);
    }
    pSC132GSCtx->TestPattern = enable;

    TRACE(SC132GS_INFO, "%s: (exit)\n", __func__);

    return (result);
}

static RESULT SC132GS_IsiSensorSetBlcIss(IsiSensorHandle_t handle, sensor_blc_t * pblc)
{
    int32_t ret = 0;
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_WRONG_HANDLE;
    }

    if (pblc == NULL)
        return RET_NULL_POINTER;

    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_BLC, pblc);
    if (ret != 0)
    {
         TRACE(SC132GS_ERROR, "%s: set wb error\n", __func__);
    }

    return RET_SUCCESS;
}

static RESULT SC132GS_IsiSensorSetWBIss(IsiSensorHandle_t handle, sensor_white_balance_t * pwb)
{
    int32_t ret = 0;
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_WRONG_HANDLE;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    if (pwb == NULL)
        return RET_NULL_POINTER;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_WB, pwb);
    if (ret != 0)
    {
         TRACE(SC132GS_ERROR, "%s: set wb error\n", __func__);
    }

    return RET_SUCCESS;
}

static RESULT SC132GS_IsiGetSensorAWBModeIss(IsiSensorHandle_t  handle, IsiSensorAwbMode_t *pawbmode)
{
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    if (pSC132GSCtx->SensorMode.hdr_mode == SENSOR_MODE_HDR_NATIVE){
        *pawbmode = ISI_SENSOR_AWB_MODE_SENSOR;
    }else{
        *pawbmode = ISI_SENSOR_AWB_MODE_NORMAL;
    }
    return RET_SUCCESS;
}

static RESULT SC132GS_IsiSensorGetExpandCurveIss(IsiSensorHandle_t handle, sensor_expand_curve_t * pexpand_curve)
{
    int32_t ret = 0;
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_EXPAND_CURVE, pexpand_curve);
    if (ret != 0)
    {
        TRACE(SC132GS_ERROR, "%s: get  expand cure error\n", __func__);
        return RET_FAILURE;
    }

    return RET_SUCCESS;
}

static RESULT SC132GS_IsiGetCapsIss(IsiSensorHandle_t handle,
                         IsiSensorCaps_t * pIsiSensorCaps)
{
    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;

    RESULT result = RET_SUCCESS;

    TRACE(SC132GS_INFO, "%s (enter)\n", __func__);

    if (pSC132GSCtx == NULL) {
        return (RET_WRONG_HANDLE);
    }

    if (pIsiSensorCaps == NULL) {
        return (RET_NULL_POINTER);
    }

    pIsiSensorCaps->BusWidth = pSC132GSCtx->SensorMode.bit_width;
    pIsiSensorCaps->Mode = ISI_MODE_BAYER;
    pIsiSensorCaps->FieldSelection = ISI_FIELDSEL_BOTH;
    pIsiSensorCaps->YCSequence = ISI_YCSEQ_YCBYCR;
    pIsiSensorCaps->Conv422 = ISI_CONV422_NOCOSITED;
    pIsiSensorCaps->BPat = pSC132GSCtx->SensorMode.bayer_pattern;
    pIsiSensorCaps->HPol = ISI_HPOL_REFPOS;
    pIsiSensorCaps->VPol = ISI_VPOL_NEG;
    pIsiSensorCaps->Edge = ISI_EDGE_RISING;
    pIsiSensorCaps->Resolution.width = pSC132GSCtx->SensorMode.width;
    pIsiSensorCaps->Resolution.height = pSC132GSCtx->SensorMode.height;
    pIsiSensorCaps->SmiaMode = ISI_SMIA_OFF;
    pIsiSensorCaps->MipiLanes = ISI_MIPI_2LANES;

    if (pIsiSensorCaps->BusWidth == 10) {
        pIsiSensorCaps->MipiMode      = ISI_MIPI_MODE_RAW_10;
    }else if (pIsiSensorCaps->BusWidth == 12){
        pIsiSensorCaps->MipiMode      = ISI_MIPI_MODE_RAW_12;
    }else{
        pIsiSensorCaps->MipiMode      = ISI_MIPI_OFF;
    }

    TRACE(SC132GS_INFO, "%s (exit)\n", __func__);
    return result;
}

static RESULT SC132GS_IsiGetSensorTemperature(IsiSensorHandle_t handle, int32_t *val)
{
    RESULT result = RET_SUCCESS;
    int ret = 0;
    uint32_t i = 0, f = 0;

    TRACE(SC132GS_INFO, "%s (enter)\n", __func__);

    SC132GS_Context_t *pSC132GSCtx = (SC132GS_Context_t *) handle;
    if (pSC132GSCtx == NULL || pSC132GSCtx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pSC132GSCtx->IsiCtx.HalHandle;

    /*
    ret = SC132GS_IsiRegisterReadIss(handle, 0x4c11, &f); //float
    if (ret != 0) {
        TRACE(SC132GS_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }
    */

    ret = SC132GS_IsiRegisterReadIss(handle, 0x4c10, &i); //integer
    if (ret != 0) {
        TRACE(SC132GS_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }

    *val = i * 2 - 273;

    TRACE(SC132GS_INFO, "%s (exit)\n", __func__);
    return (result);
}


RESULT SC132GS_IsiGetSensorIss(IsiSensor_t *pIsiSensor)
{
    RESULT result = RET_SUCCESS;
    TRACE( SC132GS_INFO, "%s (enter)\n", __func__);

    if ( pIsiSensor != NULL ) {
        pIsiSensor->pszName                         = SensorName;
        pIsiSensor->pIsiCreateSensorIss             = SC132GS_IsiCreateSensorIss;

        pIsiSensor->pIsiInitSensorIss               = SC132GS_IsiInitSensorIss;
        pIsiSensor->pIsiGetSensorModeIss            = SC132GS_IsiGetSensorModeIss;
        pIsiSensor->pIsiResetSensorIss              = SC132GS_IsiResetSensorIss;
        pIsiSensor->pIsiReleaseSensorIss            = SC132GS_IsiReleaseSensorIss;
        pIsiSensor->pIsiGetCapsIss                  = SC132GS_IsiGetCapsIss;
        pIsiSensor->pIsiSetupSensorIss              = SC132GS_IsiSetupSensorIss;
        pIsiSensor->pIsiChangeSensorResolutionIss   = SC132GS_IsiChangeSensorResolutionIss;
        pIsiSensor->pIsiSensorSetStreamingIss       = SC132GS_IsiSensorSetStreamingIss;
        pIsiSensor->pIsiSensorSetPowerIss           = SC132GS_IsiSensorSetPowerIss;
        pIsiSensor->pIsiCheckSensorConnectionIss    = SC132GS_IsiCheckSensorConnectionIss;
        pIsiSensor->pIsiGetSensorRevisionIss        = SC132GS_IsiGetSensorRevisionIss;
        pIsiSensor->pIsiRegisterReadIss             = SC132GS_IsiRegisterReadIss;
        pIsiSensor->pIsiRegisterWriteIss            = SC132GS_IsiRegisterWriteIss;

        /* AEC functions */
        pIsiSensor->pIsiExposureControlIss          = SC132GS_IsiExposureControlIss;
        pIsiSensor->pIsiGetGainLimitsIss            = SC132GS_IsiGetGainLimitsIss;
        pIsiSensor->pIsiGetIntegrationTimeLimitsIss = SC132GS_IsiGetIntegrationTimeLimitsIss;
        pIsiSensor->pIsiGetCurrentExposureIss       = SC132GS_IsiGetCurrentExposureIss;
        pIsiSensor->pIsiGetVSGainIss                    = SC132GS_IsiGetVSGainIss;
        pIsiSensor->pIsiGetGainIss                      = SC132GS_IsiGetGainIss;
        pIsiSensor->pIsiGetLongGainIss                  = SC132GS_IsiGetLongGainIss;
        pIsiSensor->pIsiGetGainIncrementIss             = SC132GS_IsiGetGainIncrementIss;
        pIsiSensor->pIsiSetGainIss                      = SC132GS_IsiSetGainIss;
        pIsiSensor->pIsiGetIntegrationTimeIss           = SC132GS_IsiGetIntegrationTimeIss;
        pIsiSensor->pIsiGetVSIntegrationTimeIss         = SC132GS_IsiGetVSIntegrationTimeIss;
        pIsiSensor->pIsiGetLongIntegrationTimeIss       = SC132GS_IsiGetLongIntegrationTimeIss;
        pIsiSensor->pIsiGetIntegrationTimeIncrementIss  = SC132GS_IsiGetIntegrationTimeIncrementIss;
        pIsiSensor->pIsiSetIntegrationTimeIss           = SC132GS_IsiSetIntegrationTimeIss;
        pIsiSensor->pIsiQuerySensorIss                  = SC132GS_IsiQuerySensorIss;
        pIsiSensor->pIsiGetResolutionIss                = SC132GS_IsiGetResolutionIss;
        pIsiSensor->pIsiGetSensorFpsIss                 = SC132GS_IsiGetSensorFpsIss;
        pIsiSensor->pIsiSetSensorFpsIss                 = SC132GS_IsiSetSensorFpsIss;
        pIsiSensor->pIsiSensorGetExpandCurveIss         = SC132GS_IsiSensorGetExpandCurveIss;

        /* AWB specific functions */

        /* Testpattern */
        pIsiSensor->pIsiActivateTestPattern         = SC132GS_IsiActivateTestPattern;
        pIsiSensor->pIsiSetBayerPattern             = SC132GS_IsiSetBayerPattern;

        pIsiSensor->pIsiSensorSetBlcIss             = SC132GS_IsiSensorSetBlcIss;
        pIsiSensor->pIsiSensorSetWBIss              = SC132GS_IsiSensorSetWBIss;
        pIsiSensor->pIsiGetSensorAWBModeIss         = SC132GS_IsiGetSensorAWBModeIss;
        pIsiSensor->pIsiGetSensorTemperature        = SC132GS_IsiGetSensorTemperature;
    } else {
        result = RET_NULL_POINTER;
    }

    TRACE( SC132GS_INFO, "%s (exit)\n", __func__);
    return ( result );
}

/*****************************************************************************
* each sensor driver need declare this struct for isi load
*****************************************************************************/
IsiCamDrvConfig_t SC132GS_IsiCamDrvConfig = {
    0,
    SC132GS_IsiQuerySensorSupportIss,
    SC132GS_IsiGetSensorIss,
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
