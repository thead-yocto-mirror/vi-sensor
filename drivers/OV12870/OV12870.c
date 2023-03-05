#include <ebase/types.h>
#include <ebase/trace.h>
#include <ebase/builtins.h>
#include <common/return_codes.h>
#include <common/misc.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <isi/isi.h>
#include <isi/isi_iss.h>
#include <isi/isi_priv.h>
#include "vvsensor.h"
#include "OV12870_priv.h"
#include "ov12870.h"

CREATE_TRACER( OV12870_INFO , "OV12870: ", INFO,    0);
CREATE_TRACER( OV12870_WARN , "OV12870: ", WARNING, 1);
CREATE_TRACER( OV12870_ERROR, "OV12870: ", ERROR,   1);
CREATE_TRACER( OV12870_DEBUG,     "OV12870: ", INFO, 1);
CREATE_TRACER( OV12870_REG_INFO , "OV12870: ", INFO, 1);
CREATE_TRACER( OV12870_REG_DEBUG, "OV12870: ", INFO, 1);

#ifdef SUBDEV_V4L2
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#undef TRACE
#define TRACE(x, ...)
#endif

#define OV12870_MIN_GAIN_STEP    ( 1.0f/16.0f )  /**< min gain step size used by GUI (hardware min = 1/16; 1/16..32/16 depending on actual gain ) */
#define OV12870_MAX_GAIN_AEC     ( 32.0f )       /**< max. gain used by the AEC (arbitrarily chosen, hardware limit = 62.0, driver limit = 32.0 ) */
#define OV12870_VS_MAX_INTEGRATION_TIME (0.0018)

/*****************************************************************************
 *Sensor Info
*****************************************************************************/
static const char SensorName[16] = "OV12870";

static struct vvcam_mode_info pov12870_mode_info[] = {
    {
        .index     = 0,
        .width     = 640,
        .height    = 480,
        .fps       = 30,
        .hdr_mode  = SENSOR_MODE_LINEAR,
        .bit_width = 12,
        .bayer_pattern = BAYER_BGGR,
        .mipi_phy_freq = 400, //mbps
        .mipi_line_num = 4,
		.config_file_3a = "OV12870_640x480_raw12",  //3aconfig_OV12870_640x480_raw12.json
        .preg_data = (void *)"ov12870 sensor liner mode, raw12, 400mbps(sensor clk 24m), img resolution is 640*480",
        //pclk 90mhz
    },
    {
        .index    = 1,
        .width = 1920,
        .height = 1080,
        .fps      = 30,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 12,
        .bayer_pattern = BAYER_BGGR,
        //.mipi_phy_freq = 1200, //mbps
        .mipi_phy_freq = 400, //mbps
        .mipi_line_num = 4,
		.config_file_3a = "OV12870_1920x1080_raw12",  //3aconfig_OV12870_1920x1080_raw12.json
        .preg_data = (void *)"ov12870 sensor liner mode, raw12, 1200mbps(sensor clk 24m), img resolution is 1280*1080",
        //pclk 180mhz
    },
    {
        .index     = 2,
        .width = 4096,
        .height = 3072,
        .fps      = 30,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_BGGR,
        .mipi_phy_freq = 1200, //mbps
        .mipi_line_num = 4,
		.config_file_3a = "OV12870_4096x3072_raw12",  //3aconfig_OV12870_4096x3072_raw12.json
        .preg_data = (void *)"ov12870 sensor liner mode, raw12, 1200mbps(sensor clk 24m), img resolution is 4096*3072",
        //pclk 180mhz
    },
};

static RESULT OV12870_IsiRegisterWriteIss(IsiSensorHandle_t handle, const uint32_t address, const uint32_t value);

long __ov12870_set_exposure(IsiSensorHandle_t handle, int coarse_itg,
				 int gain, int digitgain, OV12870_EXPOSURE_SETTING_t type)

{

	return 0;
}

static RESULT OV12870_IsiSensorSetPowerIss(IsiSensorHandle_t handle, bool_t on) {
    RESULT result = RET_SUCCESS;

    int ret = 0;
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    TRACE(OV12870_INFO, "%s (enter)\n", __func__);

    int32_t enable = on;
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_POWER, &enable);
    if (ret != 0) {
        // to do
        //TRACE(OV12870_ERROR, "%s: sensor set power error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(OV12870_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT OV12870_IsiResetSensorIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;
    int ret = 0;

    TRACE(OV12870_INFO, "%s (enter)\n", __func__);

    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_RESET, NULL);
    if (ret != 0) {
        TRACE(OV12870_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }
    sleep(0.2);

    ret = OV12870_IsiRegisterWriteIss(handle, 0x301e, 0x0);
    if (ret != 0) {
        TRACE(OV12870_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }

    ret = OV12870_IsiRegisterWriteIss(handle, 0x103, 1);
    if (ret != 0) {
        TRACE(OV12870_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }

    sleep(0.2);

    TRACE(OV12870_INFO, "%s (exit)\n", __func__);
    return (result);
}

#ifdef SUBDEV_CHAR
static RESULT OV12870_IsiSensorSetClkIss(IsiSensorHandle_t handle, uint32_t clk) {
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;

    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    TRACE(OV12870_INFO, "%s (enter)\n", __func__);

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_CLK, &clk);
    if (ret != 0) {
        // to do
        //TRACE(OV12870_ERROR, "%s: sensor set clk error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(OV12870_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT OV12870_IsiSensorGetClkIss
    (IsiSensorHandle_t handle, uint32_t * pclk) {
    RESULT result = RET_SUCCESS;
    int ret = 0;

    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    TRACE(OV12870_INFO, "%s (enter)\n", __func__);

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_CLK, pclk);
    if (ret != 0) {
        // to do
        //TRACE(OV12870_ERROR, "%s: sensor get clk error!\n", __func__);
        //return (RET_FAILURE);
    }

    TRACE(OV12870_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT OV12870_IsiConfigSensorSCCBIss(IsiSensorHandle_t handle)
{
    int ret = 0;
    TRACE(OV12870_INFO, "%s (enter)\n", __func__);
    return RET_SUCCESS;

    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    static const IsiSccbInfo_t SensorSccbInfo = {
        .slave_addr = (0x10),  //0x30 or 0x32
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
        TRACE(OV12870_ERROR, "%s: sensor config sccb info error!\n",
              __func__);
        return (RET_FAILURE);
    }

    TRACE(OV12870_INFO, "%s (exit)\n", __func__);

    return RET_SUCCESS;
}
#endif

static RESULT OV12870_IsiRegisterReadIss
    (IsiSensorHandle_t handle, const uint32_t address, uint32_t * p_value) {
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    TRACE(OV12870_INFO, "%s (enter)\n", __func__);

    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    struct vvcam_sccb_data sccb_data;
    sccb_data.addr = address;
    sccb_data.data = 0;
    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_READ_REG, &sccb_data);
    if (ret != 0) {
        TRACE(OV12870_ERROR, "%s: read sensor register error!\n",
              __func__);
        return (RET_FAILURE);
    }

    *p_value = sccb_data.data;

    TRACE(OV12870_INFO, "%s (exit) result = %d\n", __func__, result);
    return (result);
}

static RESULT OV12870_IsiRegisterWriteIss
    (IsiSensorHandle_t handle, const uint32_t address, const uint32_t value) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    TRACE(OV12870_INFO, "%s (enter)\n", __func__);

    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    struct vvcam_sccb_data sccb_data;
    sccb_data.addr = address;
    sccb_data.data = value;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_WRITE_REG, &sccb_data);
    if (ret != 0) {
        TRACE(OV12870_ERROR, "%s: write sensor register error!\n",
              __func__);
        return (RET_FAILURE);
    }

    TRACE(OV12870_INFO, "%s (exit) result = %d\n", __func__, result);
    return (result);
}

static RESULT OV12870_IsiQuerySensorSupportIss(HalHandle_t  HalHandle, vvcam_mode_info_array_t *pSensorSupportInfo)
{
    //int ret = 0;
    struct vvcam_mode_info_array *psensor_mode_info_arry;

    HalContext_t *pHalCtx = HalHandle;
    if ( pHalCtx == NULL ) {
        return RET_NULL_POINTER;
    }

    psensor_mode_info_arry = pSensorSupportInfo;
    psensor_mode_info_arry->count = sizeof(pov12870_mode_info) / sizeof(struct vvcam_mode_info);
    memcpy(psensor_mode_info_arry->modes, pov12870_mode_info, sizeof(pov12870_mode_info));
    return RET_SUCCESS;
}

static  RESULT OV12870_IsiQuerySensorIss(IsiSensorHandle_t handle, vvcam_mode_info_array_t *pSensorInfo)
{
    RESULT result = RET_SUCCESS;
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;
    OV12870_IsiQuerySensorSupportIss(pHalCtx,pSensorInfo);

    return result;
}

static RESULT OV12870_IsiGetSensorModeIss(IsiSensorHandle_t handle,void *mode)
{
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL) {
        return (RET_WRONG_HANDLE);
    }
    memcpy(mode,&(pOV12870Ctx->SensorMode), sizeof(pOV12870Ctx->SensorMode));

    return ( RET_SUCCESS );
}

static RESULT OV12870_IsiCreateSensorIss(IsiSensorInstanceConfig_t * pConfig) {
    RESULT result = RET_SUCCESS;
    OV12870_Context_t *pOV12870Ctx;

    TRACE(OV12870_INFO, "%s (enter)\n", __func__);

    if (!pConfig || !pConfig->pSensor)
        return (RET_NULL_POINTER);

    pOV12870Ctx = (OV12870_Context_t *) malloc(sizeof(OV12870_Context_t));
    if (!pOV12870Ctx) {
        TRACE(OV12870_ERROR, "%s: Can't allocate ov12870 context\n",
              __func__);
        return (RET_OUTOFMEM);
    }

    MEMSET(pOV12870Ctx, 0, sizeof(OV12870_Context_t));

    result = HalAddRef(pConfig->HalHandle);
    if (result != RET_SUCCESS) {
        free(pOV12870Ctx);
        return (result);
    }

    pOV12870Ctx->IsiCtx.HalHandle = pConfig->HalHandle;
    pOV12870Ctx->IsiCtx.pSensor = pConfig->pSensor;
    pOV12870Ctx->GroupHold = BOOL_FALSE;
    pOV12870Ctx->OldGain = 0;
    pOV12870Ctx->OldIntegrationTime = 0;
    pOV12870Ctx->Configured = BOOL_FALSE;
    pOV12870Ctx->Streaming = BOOL_FALSE;
    pOV12870Ctx->TestPattern = BOOL_FALSE;
    pOV12870Ctx->isAfpsRun = BOOL_FALSE;
    pOV12870Ctx->SensorMode.index = pConfig->SensorModeIndex;
    pConfig->hSensor = (IsiSensorHandle_t) pOV12870Ctx;
#ifdef SUBDEV_CHAR
    struct vvcam_mode_info *SensorDefaultMode = NULL;
    for (int i=0; i < sizeof(pov12870_mode_info)/ sizeof(struct vvcam_mode_info); i++)
    {
        if (pov12870_mode_info[i].index == pOV12870Ctx->SensorMode.index)
        {
            SensorDefaultMode = &(pov12870_mode_info[i]);
            break;
        }
    }

    if (SensorDefaultMode != NULL)
    {
        strcpy(pOV12870Ctx->SensorRegCfgFile, get_vi_config_path());
        switch(SensorDefaultMode->index)
        {
            case 0:
                strcat(pOV12870Ctx->SensorRegCfgFile,
                    "OV12870_mipi4lane_640x480_init.txt");
                break;
            case 1:
                strcat(pOV12870Ctx->SensorRegCfgFile,
                    "OV12870_mipi4lane_1920x1080_1200_30f.txt");
                break;
            case 2:
                strcat(pOV12870Ctx->SensorRegCfgFile,
                    "OV12870_mipi4lane_4096X3072_1200_30f_init.txt");
                break;
            default:
                return -1;
        }

        if (access(pOV12870Ctx->SensorRegCfgFile, F_OK) == 0) {
            pOV12870Ctx->KernelDriverFlag = 0;
            memcpy(&(pOV12870Ctx->SensorMode),SensorDefaultMode,sizeof(struct vvcam_mode_info));
        } else {
            pOV12870Ctx->KernelDriverFlag = 1;
        }
    }else
    {
        pOV12870Ctx->KernelDriverFlag = 1;
    }

    result = OV12870_IsiSensorSetPowerIss(pOV12870Ctx, BOOL_TRUE);
    system("echo 456 > /sys/class/gpio/export");
    system("echo out > /sys/class/gpio/gpio456/direction");
    system("echo 0 > /sys/class/gpio/gpio456/value");
    sleep(0.2);
    system("echo 1 > /sys/class/gpio/gpio456/value");
    sleep(0.2);
    system("echo 456 > /sys/class/gpio/unexport");

    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    uint32_t SensorClkIn = 0;
    if (pOV12870Ctx->KernelDriverFlag) {
        result = OV12870_IsiSensorGetClkIss(pOV12870Ctx, &SensorClkIn);
        RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);
    }

    result = OV12870_IsiSensorSetClkIss(pOV12870Ctx, SensorClkIn);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    result = OV12870_IsiResetSensorIss(pOV12870Ctx);
    RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);

    pOV12870Ctx->pattern = ISI_BPAT_BGBGGRGR;

    if (!pOV12870Ctx->KernelDriverFlag) {
        result = OV12870_IsiConfigSensorSCCBIss(pOV12870Ctx);
        RETURN_RESULT_IF_DIFFERENT(RET_SUCCESS, result);
    }
#endif

    TRACE(OV12870_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT OV12870_IsiGetRegCfgIss(const char *registerFileName,
                     struct vvcam_sccb_array *arry)
{
    if (NULL == registerFileName) {
        TRACE(OV12870_ERROR, "%s:registerFileName is NULL\n", __func__);
        return (RET_NULL_POINTER);
    }
#ifdef SUBDEV_CHAR
    FILE *fp = NULL;
    fp = fopen(registerFileName, "rb");
    if (!fp) {
        TRACE(OV12870_ERROR, "%s:load register file  %s error!\n",
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
        TRACE(OV12870_ERROR, "%s:malloc failed NULL Point!\n", __func__,
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

static RESULT OV12870_IsiInitSensorIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;

    int ret = 0;
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;

    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;
    TRACE(OV12870_INFO, "%s (enter)\n", __func__);

    if (pOV12870Ctx == NULL) {
        return (RET_WRONG_HANDLE);
    }

    if (pOV12870Ctx->KernelDriverFlag) {
        ;
    } else {
        struct vvcam_sccb_array arry;
        result = OV12870_IsiGetRegCfgIss(pOV12870Ctx->SensorRegCfgFile, &arry);
        if (result != 0) {
            TRACE(OV12870_ERROR,
                  "%s:OV12870_IsiGetRegCfgIss error!\n", __func__);
            return (RET_FAILURE);
        }

        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_WRITE_ARRAY, &arry);
        if (ret != 0) {
            TRACE(OV12870_ERROR, "%s:Sensor Write Reg arry error!\n",
                  __func__);
            return (RET_FAILURE);
        }

        switch(pOV12870Ctx->SensorMode.index)
        {
            case 0:
                pOV12870Ctx->one_line_exp_time = 0.000001;
                pOV12870Ctx->FrameLengthLines = 480-36;
                pOV12870Ctx->CurFrameLengthLines = pOV12870Ctx->FrameLengthLines;
                pOV12870Ctx->MaxIntegrationLine = pOV12870Ctx->CurFrameLengthLines;
                pOV12870Ctx->MinIntegrationLine = 8;
                pOV12870Ctx->AecMaxGain = 0xffff;
                pOV12870Ctx->AecMinGain = 1;
                break;
            case 1:
                pOV12870Ctx->one_line_exp_time = 0.000001;
                pOV12870Ctx->FrameLengthLines = 2142;
                pOV12870Ctx->CurFrameLengthLines = pOV12870Ctx->FrameLengthLines;
                pOV12870Ctx->MaxIntegrationLine = pOV12870Ctx->CurFrameLengthLines;
                pOV12870Ctx->MinIntegrationLine = 8;
                pOV12870Ctx->AecMaxGain = 0xffff;
                pOV12870Ctx->AecMinGain = 1;
                break;
            case 2:
                break;
            default:
                return ( RET_NOTAVAILABLE );
                break;
        }
		pOV12870Ctx->AecIntegrationTimeIncrement = pOV12870Ctx->one_line_exp_time;
		pOV12870Ctx->AecMinIntegrationTime =
			pOV12870Ctx->one_line_exp_time * pOV12870Ctx->MinIntegrationLine;
		pOV12870Ctx->AecMaxIntegrationTime =
			pOV12870Ctx->one_line_exp_time * pOV12870Ctx->MaxIntegrationLine;


        pOV12870Ctx->MaxFps  = pOV12870Ctx->SensorMode.fps;
        pOV12870Ctx->MinFps  = 1;
        pOV12870Ctx->CurrFps = pOV12870Ctx->MaxFps;
    }

    TRACE(OV12870_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT OV12870_IsiReleaseSensorIss(IsiSensorHandle_t handle) {
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(OV12870_INFO, "%s (enter)\n", __func__);

    if (pOV12870Ctx == NULL)
        return (RET_WRONG_HANDLE);

    (void)OV12870_IsiSensorSetStreamingIss(pOV12870Ctx, BOOL_FALSE);
    (void)OV12870_IsiSensorSetPowerIss(pOV12870Ctx, BOOL_FALSE);
    (void)HalDelRef(pOV12870Ctx->IsiCtx.HalHandle);

    MEMSET(pOV12870Ctx, 0, sizeof(OV12870_Context_t));
    free(pOV12870Ctx);
    TRACE(OV12870_INFO, "%s (exit)\n", __func__);
    return (result);
}

struct ov12870_fmt {
    int width;
    int height;
    int fps;
};

static RESULT OV12870_IsiSetupSensorIss
    (IsiSensorHandle_t handle, const IsiSensorConfig_t * pConfig) {

    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    RESULT result = RET_SUCCESS;

    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (!pOV12870Ctx) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pConfig) {
        TRACE(OV12870_ERROR,
              "%s: Invalid configuration (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    if (pOV12870Ctx->Streaming != BOOL_FALSE) {
        return RET_WRONG_STATE;
    }

    memcpy(&pOV12870Ctx->Config, pConfig, sizeof(IsiSensorConfig_t));

    /* 1.) SW reset of image sensor (via I2C register interface)  be careful, bits 6..0 are reserved, reset bit is not sticky */
    TRACE(OV12870_DEBUG, "%s: OV12870 System-Reset executed\n", __func__);
    osSleep(100);

    //OV12870_AecSetModeParameters not defined yet as of 2021/8/9.
    //result = OV12870_AecSetModeParameters(pOV12870Ctx, pConfig);
    //if (result != RET_SUCCESS) {
    //    TRACE(OV12870_ERROR, "%s: SetupOutputWindow failed.\n",
    //          __func__);
    //    return (result);
    //}
#if 1
    struct ov12870_fmt fmt;
    fmt.width = pConfig->Resolution.width;
    fmt.height = pConfig->Resolution.height;

    ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fmt);//result = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fmt);
#endif
    pOV12870Ctx->Configured = BOOL_TRUE;
    TRACE(OV12870_INFO, "%s: (exit) ret=0x%x \n", __func__, result);
    return result;
}

static RESULT OV12870_IsiChangeSensorResolutionIss(IsiSensorHandle_t handle, uint16_t width, uint16_t height) {
    RESULT result = RET_SUCCESS;
#if 0
    struct ov12870_fmt fmt;
    fmt.width = width;
    fmt.height = height;

    int ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fmt);
#endif
    TRACE(OV12870_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT OV12870_IsiSensorSetStreamingIss
    (IsiSensorHandle_t handle, bool_t on) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    TRACE(OV12870_INFO, "%s (enter)\n", __func__);

    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    if (pOV12870Ctx->Configured != BOOL_TRUE)
        return RET_WRONG_STATE;

    int32_t enable = (uint32_t) on;
    ret = OV12870_IsiRegisterWriteIss(handle, 0x100, on);

    if (ret != 0) {
        return (RET_FAILURE);
    }

    pOV12870Ctx->Streaming = on;

    TRACE(OV12870_INFO, "%s (exit)\n", __func__);
    return (result);
}

static uint32_t sensor_get_chip_id(IsiSensorHandle_t handle, uint32_t *chip_id)
{
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    uint32_t id_val = 0;

    ret = OV12870_IsiRegisterReadIss(handle, 0x6000, &id_val);
    if (ret != 0) {
        TRACE(OV12870_ERROR,
            "%s: Read Sensor correct ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    *chip_id |= id_val << 16;

    ret = OV12870_IsiRegisterReadIss(handle, 0x6001, &id_val);
    if (ret != 0) {
        TRACE(OV12870_ERROR,
            "%s: Read Sensor correct ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    *chip_id |= id_val << 8;

    ret = OV12870_IsiRegisterReadIss(handle, 0x6002, &id_val);
    if (ret != 0) {
        TRACE(OV12870_ERROR,
            "%s: Read Sensor correct ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    *chip_id |= id_val;

    return 0;
}

static RESULT OV12870_IsiCheckSensorConnectionIss(IsiSensorHandle_t handle) {
    RESULT result = RET_SUCCESS;
    int ret = 0;
    //uint32_t correct_id = 0x12870;
    uint32_t correct_id = 0x0;
    uint32_t sensor_id = 0;

    TRACE(OV12870_INFO, "%s (enter)\n", __func__);

    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    ret = sensor_get_chip_id(handle, &sensor_id);
    if (ret != 0) {
        TRACE(OV12870_ERROR,
            "%s: Read Sensor chip ID Error! \n", __func__);
        return (RET_FAILURE);
    }

    if (correct_id != sensor_id) {
        TRACE(OV12870_ERROR, "%s:ChipID =0x%x sensor_id=%x error! \n",
              __func__, correct_id, sensor_id);
        return (RET_FAILURE);
    }

    TRACE(OV12870_INFO,
          "%s ChipID = 0x%08x, sensor_id = 0x%08x, success! \n", __func__,
          correct_id, sensor_id);
    TRACE(OV12870_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT OV12870_IsiGetSensorRevisionIss
    (IsiSensorHandle_t handle, uint32_t * p_value) {
    RESULT result = RET_SUCCESS;
    TRACE(OV12870_INFO, "%s (enter)\n", __func__);

    *p_value = 0x12870;
    TRACE(OV12870_INFO, "%s (exit)\n", __func__);
    return (result);
}

static RESULT OV12870_IsiGetGainLimitsIss
    (IsiSensorHandle_t handle, float *pMinGain, float *pMaxGain) {
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    /*TODO*/

    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (pOV12870Ctx == NULL) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pMinGain == NULL) || (pMaxGain == NULL)) {
        TRACE(OV12870_ERROR, "%s: NULL pointer received!!\n");
        return (RET_NULL_POINTER);
    }

    *pMinGain = pOV12870Ctx->AecMinGain;
    *pMaxGain = pOV12870Ctx->AecMaxGain;

    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);
    return (result);
}

static RESULT OV12870_IsiGetIntegrationTimeLimitsIss
    (IsiSensorHandle_t handle,
     float *pMinIntegrationTime, float *pMaxIntegrationTime) {
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    /*TODO*/

    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);
    if (pOV12870Ctx == NULL) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pMinIntegrationTime == NULL) || (pMaxIntegrationTime == NULL)) {
        TRACE(OV12870_ERROR, "%s: NULL pointer received!!\n");
        return (RET_NULL_POINTER);
    }

    *pMinIntegrationTime = pOV12870Ctx->AecMinIntegrationTime;
    *pMaxIntegrationTime = pOV12870Ctx->AecMaxIntegrationTime;

    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);
    return (result);
}

RESULT OV12870_IsiGetGainIss(IsiSensorHandle_t handle, float *pSetGain) {
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (pOV12870Ctx == NULL) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pSetGain == NULL) {
        return (RET_NULL_POINTER);
    }

    *pSetGain = pOV12870Ctx->AecCurGain;

    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT OV12870_IsiGetLongGainIss(IsiSensorHandle_t handle, float *gain)
{
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;

    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (pOV12870Ctx == NULL) {
        TRACE(OV12870_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }

    if (gain == NULL) {
        return (RET_NULL_POINTER);
    }

    *gain = pOV12870Ctx->AecCurLongGain;

    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);

    return (RET_SUCCESS);
}

RESULT OV12870_IsiGetVSGainIss(IsiSensorHandle_t handle, float *pSetGain) {
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (pOV12870Ctx == NULL) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pSetGain == NULL) {
        return (RET_NULL_POINTER);
    }

    *pSetGain = pOV12870Ctx->AecCurVSGain;

    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);

    return (result);
}

RESULT OV12870_IsiGetGainIncrementIss(IsiSensorHandle_t handle, float *pIncr) {
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (pOV12870Ctx == NULL) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (pIncr == NULL)
        return (RET_NULL_POINTER);

    *pIncr = pOV12870Ctx->AecGainIncrement;

    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);

    return (result);
}

RESULT OV12870_IsiSetGainIss
    (IsiSensorHandle_t handle,
     float NewGain, float *pSetGain, float *hdr_ratio) {

    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    int TmpGain;

    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    if (NewGain >= 25) { // More than 25 will not take effect
        NewGain = 25;
    }

    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    uint32_t SensorGain = 0;
    SensorGain = NewGain * pOV12870Ctx->gain_accuracy;

    /*TODO*/
#if 0
    ret |= ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_GAIN, &SensorGain);
    if (ret != 0) {
        TRACE(OV12870_ERROR, "%s: set sensor gain error\n",
                  __func__);
        return RET_FAILURE;
    }
#endif

    pOV12870Ctx->AecCurGain = ((float)(NewGain));
    TmpGain = (int)NewGain;
    __ov12870_set_exposure(handle, 0,
				((int)NewGain << 4) + round((NewGain - TmpGain) / 0.0625f), 0, OV12870_ANALOG_GAIN);

    *pSetGain = pOV12870Ctx->AecCurGain;
    TRACE(OV12870_DEBUG, "%s: g=%f\n", __func__, *pSetGain);
    return (result);
}

RESULT OV12870_IsiSetLongGainIss(IsiSensorHandle_t handle, float gain)
{
    int ret = 0;
    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;

    if (!pOV12870Ctx || !pOV12870Ctx->IsiCtx.HalHandle)
    {
        TRACE(OV12870_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }

    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    uint32_t SensorGain = 0;
    SensorGain = gain * pOV12870Ctx->gain_accuracy;
    if (pOV12870Ctx->LastLongGain != SensorGain)
    {

        /*TODO*/
#if 0
        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_LONG_GAIN, &SensorGain);
        if (ret != 0)
        {
            return (RET_FAILURE);
            TRACE(OV12870_ERROR,"%s: set long gain failed\n");

        }
#endif
        pOV12870Ctx->LastLongGain = SensorGain;
        pOV12870Ctx->AecCurLongGain = gain;
    }

    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT OV12870_IsiSetVSGainIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float NewGain, float *pSetGain, float *hdr_ratio) {
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;
    RESULT result = RET_SUCCESS;
#if 0
    float Gain = 0.0f;

    uint32_t ucGain = 0U;
    uint32_t again = 0U;
#endif

    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (!pOV12870Ctx) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetGain || !hdr_ratio)
        return (RET_NULL_POINTER);

    uint32_t SensorGain = 0;
    SensorGain = NewGain * pOV12870Ctx->gain_accuracy;

    /*TODO*/
    //ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_VSGAIN, &SensorGain);

    pOV12870Ctx->AecCurVSGain = NewGain;
    *pSetGain = pOV12870Ctx->AecCurGain;
    TRACE(OV12870_DEBUG, "%s: g=%f\n", __func__, *pSetGain);
    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT OV12870_IsiSetBayerPattern(IsiSensorHandle_t handle, uint8_t pattern)
{

    RESULT result = RET_SUCCESS;
#if 0
    uint8_t h_shift = 0, v_shift = 0;
    uint32_t val_h = 0, val_l = 0;
    uint16_t val = 0;
    uint8_t Start_p = 0;
    bool_t streaming_status;
    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    // pattern 0:B 1:GB 2:GR 3:R
    streaming_status = pOV12870Ctx->Streaming;
    result = OV12870_IsiSensorSetStreamingIss(handle, 0);
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

    OV12870_IsiRegisterReadIss(handle, 0x30a0, &val_h);
    OV12870_IsiRegisterReadIss(handle, 0x30a1, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + h_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    OV12870_IsiRegisterWriteIss(handle, 0x30a0, (uint8_t)val_h);
    OV12870_IsiRegisterWriteIss(handle, 0x30a1, (uint8_t)val_l);

    OV12870_IsiRegisterReadIss(handle, 0x30a2, &val_h);
    OV12870_IsiRegisterReadIss(handle, 0x30a3, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + v_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    OV12870_IsiRegisterWriteIss(handle, 0x30a2, (uint8_t)val_h);
    OV12870_IsiRegisterWriteIss(handle, 0x30a3, (uint8_t)val_l);

    OV12870_IsiRegisterReadIss(handle, 0x30a4, &val_h);
    OV12870_IsiRegisterReadIss(handle, 0x30a5, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + h_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    OV12870_IsiRegisterWriteIss(handle, 0x30a4, (uint8_t)val_h);
    OV12870_IsiRegisterWriteIss(handle, 0x30a5, (uint8_t)val_l);

    OV12870_IsiRegisterReadIss(handle, 0x30a6, &val_h);
    OV12870_IsiRegisterReadIss(handle, 0x30a7, &val_l);
    val = (((val_h << 8) & 0xff00) | (val_l & 0x00ff)) + v_shift;
    val_h = (val >> 8) & 0xff;
    val_l = val & 0xff;
    OV12870_IsiRegisterWriteIss(handle, 0x30a6, (uint8_t)val_h);
    OV12870_IsiRegisterWriteIss(handle, 0x30a7, (uint8_t)val_l);

    pOV12870Ctx->pattern = pattern;
    result = OV12870_IsiSensorSetStreamingIss(handle, streaming_status);
#endif

    return (result);
}

RESULT OV12870_IsiGetIntegrationTimeIss
    (IsiSensorHandle_t handle, float *pSetIntegrationTime)
{
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (!pOV12870Ctx) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetIntegrationTime)
        return (RET_NULL_POINTER);
    *pSetIntegrationTime = pOV12870Ctx->AecCurIntegrationTime;
    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT OV12870_IsiGetLongIntegrationTimeIss(IsiSensorHandle_t handle, float *pIntegrationTime)
{
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (!pOV12870Ctx) {
        TRACE(OV12870_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }
    if (!pIntegrationTime)
        return (RET_NULL_POINTER);

    pOV12870Ctx->AecCurLongIntegrationTime =  pOV12870Ctx->AecCurIntegrationTime;

    *pIntegrationTime = pOV12870Ctx->AecCurLongIntegrationTime;
    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT OV12870_IsiGetVSIntegrationTimeIss
    (IsiSensorHandle_t handle, float *pSetIntegrationTime)
{
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (!pOV12870Ctx) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    if (!pSetIntegrationTime)
        return (RET_NULL_POINTER);

    *pSetIntegrationTime = pOV12870Ctx->AecCurVSIntegrationTime;
    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT OV12870_IsiGetIntegrationTimeIncrementIss
    (IsiSensorHandle_t handle, float *pIncr)
{
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (!pOV12870Ctx) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pIncr)
        return (RET_NULL_POINTER);

    //_smallest_ increment the sensor/driver can handle (e.g. used for sliders in the application)
    *pIncr = pOV12870Ctx->AecIntegrationTimeIncrement;
    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT OV12870_IsiSetIntegrationTimeIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float *pSetIntegrationTime,
     uint8_t * pNumberOfFramesToSkip, float *hdr_ratio)
{
    RESULT result = RET_SUCCESS;

    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    uint32_t exp_line = 0;
    uint32_t exp_line_old = 0;

    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (!pOV12870Ctx) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetIntegrationTime || !pNumberOfFramesToSkip) {
        TRACE(OV12870_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    exp_line = NewIntegrationTime / pOV12870Ctx->one_line_exp_time;
    exp_line_old = exp_line;
    exp_line =
        MIN(pOV12870Ctx->MaxIntegrationLine,
        MAX(pOV12870Ctx->MinIntegrationLine, exp_line));

    TRACE(OV12870_DEBUG, "%s: set AEC_PK_EXPO=0x%05x\n", __func__, exp_line);

    if (exp_line != pOV12870Ctx->OldIntegrationTime) {

        /*TODO*/
        //ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_EXP, &exp_line);
        pOV12870Ctx->OldIntegrationTime = exp_line;    // remember current integration time
        pOV12870Ctx->AecCurIntegrationTime =
            exp_line * pOV12870Ctx->one_line_exp_time;

        *pNumberOfFramesToSkip = 1U;    //skip 1 frame
    } else {
        *pNumberOfFramesToSkip = 0U;    //no frame skip
    }
    uint32_t reg_h = (exp_line & 0xff00) >> 8;
    uint32_t reg_l = (exp_line & 0xff);


    int ret = OV12870_IsiRegisterWriteIss(handle, 0x3501, reg_h);
    if (ret != 0) {
        TRACE(OV12870_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }

    ret = OV12870_IsiRegisterWriteIss(handle, 0x3502, reg_l);
    if (ret != 0) {
        TRACE(OV12870_ERROR, "%s: sensor reset error!\n", __func__);
        return (RET_FAILURE);
    }


    if (exp_line_old != exp_line) {
        *pSetIntegrationTime = pOV12870Ctx->AecCurIntegrationTime;
    } else {
        *pSetIntegrationTime = NewIntegrationTime;
    }

    TRACE(OV12870_DEBUG, "%s: Ti=%f\n", __func__, *pSetIntegrationTime);
    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT OV12870_IsiSetLongIntegrationTimeIss(IsiSensorHandle_t handle,float IntegrationTime)
{
    int ret;
    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (!handle || !pOV12870Ctx->IsiCtx.HalHandle)
    {
        TRACE(OV12870_ERROR,"%s: Invalid sensor handle (NULL pointer detected)\n",__func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    uint32_t exp_line = 0;
    exp_line = IntegrationTime / pOV12870Ctx->one_line_exp_time;
    exp_line = MIN(pOV12870Ctx->MaxIntegrationLine, MAX(pOV12870Ctx->MinIntegrationLine, exp_line));

    if (exp_line != pOV12870Ctx->LastLongExpLine)
    {
        if (pOV12870Ctx->KernelDriverFlag)
        {
            ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_LONG_EXP, &exp_line);
            if (ret != 0)
            {
                TRACE(OV12870_ERROR,"%s: set long gain failed\n");
                return RET_FAILURE;
            }
        }

        pOV12870Ctx->LastLongExpLine = exp_line;
        pOV12870Ctx->AecCurLongIntegrationTime =  pOV12870Ctx->LastLongExpLine*pOV12870Ctx->one_line_exp_time;
    }


    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);
    return (RET_SUCCESS);
}

RESULT OV12870_IsiSetVSIntegrationTimeIss
    (IsiSensorHandle_t handle,
     float NewIntegrationTime,
     float *pSetVSIntegrationTime,
     uint8_t * pNumberOfFramesToSkip, float *hdr_ratio)
{
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    uint32_t exp_line = 0;

    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (!pOV12870Ctx) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if (!pSetVSIntegrationTime || !pNumberOfFramesToSkip) {
        TRACE(OV12870_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    TRACE(OV12870_INFO,
          "%s:  maxIntegrationTime-=%f minIntegrationTime = %f\n", __func__,
          pOV12870Ctx->AecMaxIntegrationTime,
          pOV12870Ctx->AecMinIntegrationTime);


    exp_line = NewIntegrationTime / pOV12870Ctx->one_line_exp_time;
    exp_line =
        MIN(pOV12870Ctx->MaxIntegrationLine,
        MAX(pOV12870Ctx->MinIntegrationLine, exp_line));

    if (exp_line != pOV12870Ctx->OldVsIntegrationTime) {
    /*TODO*/
    //    ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_VSEXP, &exp_line);
    } else if (1){

        pOV12870Ctx->OldVsIntegrationTime = exp_line;
        pOV12870Ctx->AecCurVSIntegrationTime = exp_line * pOV12870Ctx->one_line_exp_time;    //remember current integration time
        *pNumberOfFramesToSkip = 1U;    //skip 1 frame
    } else {
        *pNumberOfFramesToSkip = 0U;    //no frame skip
    }

    *pSetVSIntegrationTime = pOV12870Ctx->AecCurVSIntegrationTime;

    TRACE(OV12870_DEBUG, "%s: NewIntegrationTime=%f\n", __func__,
          NewIntegrationTime);
    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT OV12870_IsiExposureControlIss
    (IsiSensorHandle_t handle,
     float NewGain,
     float NewIntegrationTime,
     uint8_t * pNumberOfFramesToSkip,
     float *pSetGain, float *pSetIntegrationTime, float *hdr_ratio)
{
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    int TmpGain;
    /*TODO*/

    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (pOV12870Ctx == NULL) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pNumberOfFramesToSkip == NULL) || (pSetGain == NULL)
        || (pSetIntegrationTime == NULL)) {
        TRACE(OV12870_ERROR,
              "%s: Invalid parameter (NULL pointer detected)\n",
              __func__);
        return (RET_NULL_POINTER);
    }

    if (NewGain >= 25) { // More than 25 will not take effect
        NewGain = 25;
    }

    TRACE(OV12870_DEBUG, "%s: g=%f, Ti=%f\n", __func__, NewGain,
          NewIntegrationTime);

    if (NewIntegrationTime > pOV12870Ctx->FrameLengthLines * pOV12870Ctx->one_line_exp_time)
        NewIntegrationTime = pOV12870Ctx->FrameLengthLines * pOV12870Ctx->one_line_exp_time;

    float exp_t = NewIntegrationTime * 16.0f / pOV12870Ctx->one_line_exp_time;

    TmpGain = (int)NewGain;

    __ov12870_set_exposure(handle, (int)exp_t,
				((int)NewGain << 4) + round((NewGain - TmpGain) / 0.0625f), 1024, OV12870_ANALOG_GAIN | OV12870_INTEGRATION_TIME | OV12870_DIGITAL_GAIN);

    *pSetGain = NewGain;
    *pSetIntegrationTime = (int)exp_t / 16.0f * pOV12870Ctx->one_line_exp_time;
    pOV12870Ctx->AecCurGain = NewGain;
    pOV12870Ctx->AecCurIntegrationTime = *pSetIntegrationTime;

#if 0
    float long_gain=0;
    float long_exp=0;
    float short_gain=0;
    float short_exp=0;

    if (pOV12870Ctx->SensorMode.hdr_mode != SENSOR_MODE_LINEAR)
    {

        long_exp = NewIntegrationTime;
        long_gain = NewGain;

        float short_exposure_measure = NewIntegrationTime*NewGain / *hdr_ratio;

        if (short_exposure_measure < 48 * pOV12870Ctx->one_line_exp_time * pOV12870Ctx->AecMinGain)
        {
            short_exp = short_exposure_measure / pOV12870Ctx->AecMinGain;
            short_gain = pOV12870Ctx->AecMinGain;
        }else
        {
            short_exp = 48 * pOV12870Ctx->one_line_exp_time;
            short_gain = short_exposure_measure / short_exp;
        }

    }else
    {
        long_exp = NewIntegrationTime;
        long_gain = NewGain;
    }

    if (pOV12870Ctx->SensorMode.hdr_mode != SENSOR_MODE_LINEAR)
    {
        result = OV12870_IsiSetVSIntegrationTimeIss(handle,
                              short_exp,
                              pSetIntegrationTime,
                              pNumberOfFramesToSkip,
                              hdr_ratio);
        result =
            OV12870_IsiSetVSGainIss(handle, short_exp, short_gain,
                       pSetGain, hdr_ratio);

        result = OV12870_IsiSetLongGainIss(handle, long_gain * (*hdr_ratio));
    }
    TRACE(OV12870_DEBUG, "%s: set: NewGain=%f, hcgTi=%f, hcgskip=%d\n",
          __func__, NewGain, NewIntegrationTime, *pNumberOfFramesToSkip);
    result = OV12870_IsiSetGainIss(handle, long_gain, pSetGain, hdr_ratio);
    TRACE(OV12870_DEBUG, "%s: set: NewGain=%f, hcgTi=%f, hcgskip=%d\n",
          __func__, NewGain, NewIntegrationTime, *pNumberOfFramesToSkip);

    pOV12870Ctx->CurHdrRatio = *hdr_ratio;
#endif

    result = OV12870_IsiSetIntegrationTimeIss(handle, NewIntegrationTime, pSetIntegrationTime, pNumberOfFramesToSkip, hdr_ratio);
    TRACE(OV12870_DEBUG, "%s: set: vsg=%f, vsTi=%f, vsskip=%d\n", __func__,
          NewGain, NewIntegrationTime, *pNumberOfFramesToSkip);
    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);

    return result;
}

RESULT OV12870_IsiGetCurrentExposureIss
    (IsiSensorHandle_t handle, float *pSetGain, float *pSetIntegrationTime, float *hdr_ratio) {
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (pOV12870Ctx == NULL) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    if ((pSetGain == NULL) || (pSetIntegrationTime == NULL))
        return (RET_NULL_POINTER);

    *pSetGain = pOV12870Ctx->AecCurGain;
    *pSetIntegrationTime = pOV12870Ctx->AecCurIntegrationTime;
    *hdr_ratio = pOV12870Ctx->CurHdrRatio;

    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT OV12870_IsiGetResolutionIss(IsiSensorHandle_t handle, uint16_t *pwidth, uint16_t *pheight) {
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    RESULT result = RET_SUCCESS;

    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (pOV12870Ctx == NULL) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }

    *pwidth = pOV12870Ctx->SensorMode.width;
    *pheight =  pOV12870Ctx->SensorMode.height;

    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT OV12870_IsiGetSensorFpsIss(IsiSensorHandle_t handle, uint32_t * pfps)
{
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    RESULT result = RET_SUCCESS;


    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (pOV12870Ctx == NULL) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    if (pOV12870Ctx->KernelDriverFlag) {
       /*TODO*/
        ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_FPS, pfps);
        pOV12870Ctx->CurrFps = *pfps;
    }

    *pfps = pOV12870Ctx->CurrFps;

    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);
    return (result);
}

RESULT OV12870_IsiSetSensorFpsIss(IsiSensorHandle_t handle, uint32_t fps)
{
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    RESULT result = RET_SUCCESS;
    int32_t ret = 0;
    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    if (pOV12870Ctx == NULL) {
        TRACE(OV12870_ERROR,
              "%s: Invalid sensor handle (NULL pointer detected)\n",
              __func__);
        return (RET_WRONG_HANDLE);
    }
    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    if (fps > pOV12870Ctx->MaxFps) {
        TRACE(OV12870_ERROR,
              "%s: set fps(%d) out of range, correct to %d (%d, %d)\n",
              __func__, fps, pOV12870Ctx->MaxFps, pOV12870Ctx->MinFps,
              pOV12870Ctx->MaxFps);
        fps = pOV12870Ctx->MaxFps;
    }
    if (fps < pOV12870Ctx->MinFps) {
        TRACE(OV12870_ERROR,
              "%s: set fps(%d) out of range, correct to %d (%d, %d)\n",
              __func__, fps, pOV12870Ctx->MinFps, pOV12870Ctx->MinFps,
              pOV12870Ctx->MaxFps);
        fps = pOV12870Ctx->MinFps;
    }
    if (pOV12870Ctx->KernelDriverFlag) {
        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_FPS, &fps);
        if (ret != 0) {
            TRACE(OV12870_ERROR, "%s: set sensor fps=%d error\n",
                  __func__);
            return (RET_FAILURE);
        }

        ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_SENSOR_MODE, &(pOV12870Ctx->SensorMode));
        {
            pOV12870Ctx->MaxIntegrationLine = pOV12870Ctx->SensorMode.ae_info.max_integration_time;
            pOV12870Ctx->AecMaxIntegrationTime = pOV12870Ctx->MaxIntegrationLine * pOV12870Ctx->one_line_exp_time;
        }
#ifdef SUBDEV_CHAR
        struct vvcam_ae_info_s ae_info;
        ret =
            ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_AE_INFO, &ae_info);
        if (ret != 0) {
            TRACE(OV12870_ERROR, "%s:sensor get ae info error!\n",
                  __func__);
            return (RET_FAILURE);
        }
        pOV12870Ctx->one_line_exp_time =
            (float)ae_info.one_line_exp_time_ns / 1000000000;
        pOV12870Ctx->MaxIntegrationLine = ae_info.max_integration_time;
        pOV12870Ctx->AecMaxIntegrationTime =
            pOV12870Ctx->MaxIntegrationLine *
            pOV12870Ctx->one_line_exp_time;
#endif
    }

    TRACE(OV12870_INFO, "%s: set sensor fps = %d\n", __func__,
          pOV12870Ctx->CurrFps);

    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);
    return (result);
}

static RESULT OV12870_IsiActivateTestPattern(IsiSensorHandle_t handle,
                        const bool_t enable)
{
    RESULT result = RET_SUCCESS;

    TRACE(OV12870_INFO, "%s: (enter)\n", __func__);

    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }

    if (pOV12870Ctx->Configured != BOOL_TRUE)
        return RET_WRONG_STATE;

    if (BOOL_TRUE == enable) {
        //result = OV12870_IsiRegisterWriteIss(handle, 0x3253, 0x80);
    } else {
        //result = OV12870_IsiRegisterWriteIss(handle, 0x3253, 0x00);
    }
    pOV12870Ctx->TestPattern = enable;

    TRACE(OV12870_INFO, "%s: (exit)\n", __func__);

    return (result);
}

static RESULT OV12870_IsiSensorSetBlcIss(IsiSensorHandle_t handle, sensor_blc_t * pblc)
{
    int32_t ret = 0;
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_WRONG_HANDLE;
    }

    if (pblc == NULL)
        return RET_NULL_POINTER;

    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_BLC, pblc);
    if (ret != 0)
    {
         TRACE(OV12870_ERROR, "%s: set wb error\n", __func__);
    }

    return RET_SUCCESS;
}

static RESULT OV12870_IsiSensorSetWBIss(IsiSensorHandle_t handle, sensor_white_balance_t * pwb)
{
    int32_t ret = 0;
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_WRONG_HANDLE;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    if (pwb == NULL)
        return RET_NULL_POINTER;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_S_WB, pwb);
    if (ret != 0)
    {
         TRACE(OV12870_ERROR, "%s: set wb error\n", __func__);
    }

    return RET_SUCCESS;
}

static RESULT OV12870_IsiGetSensorAWBModeIss(IsiSensorHandle_t  handle, IsiSensorAwbMode_t *pawbmode)
{
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    if (pOV12870Ctx->SensorMode.hdr_mode == SENSOR_MODE_HDR_NATIVE){
        *pawbmode = ISI_SENSOR_AWB_MODE_SENSOR;
    }else{
        *pawbmode = ISI_SENSOR_AWB_MODE_NORMAL;
    }
    return RET_SUCCESS;
}

static RESULT OV12870_IsiSensorGetExpandCurveIss(IsiSensorHandle_t handle, sensor_expand_curve_t * pexpand_curve)
{
    int32_t ret = 0;
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;
    if (pOV12870Ctx == NULL || pOV12870Ctx->IsiCtx.HalHandle == NULL) {
        return RET_NULL_POINTER;
    }
    HalContext_t *pHalCtx = (HalContext_t *) pOV12870Ctx->IsiCtx.HalHandle;

    ret = ioctl(pHalCtx->sensor_fd, VVSENSORIOC_G_EXPAND_CURVE, pexpand_curve);
    if (ret != 0)
    {
        TRACE(OV12870_ERROR, "%s: get  expand cure error\n", __func__);
        return RET_FAILURE;
    }

    return RET_SUCCESS;
}

static RESULT OV12870_IsiGetCapsIss(IsiSensorHandle_t handle,
                         IsiSensorCaps_t * pIsiSensorCaps)
{
    OV12870_Context_t *pOV12870Ctx = (OV12870_Context_t *) handle;

    RESULT result = RET_SUCCESS;

    TRACE(OV12870_INFO, "%s (enter)\n", __func__);

    if (pOV12870Ctx == NULL) {
        return (RET_WRONG_HANDLE);
    }

    if (pIsiSensorCaps == NULL) {
        return (RET_NULL_POINTER);
    }

    pIsiSensorCaps->BusWidth = pOV12870Ctx->SensorMode.bit_width;
    pIsiSensorCaps->Mode = ISI_MODE_BAYER;
    pIsiSensorCaps->FieldSelection = ISI_FIELDSEL_BOTH;
    pIsiSensorCaps->YCSequence = ISI_YCSEQ_YCBYCR;
    pIsiSensorCaps->Conv422 = ISI_CONV422_NOCOSITED;
    pIsiSensorCaps->BPat = pOV12870Ctx->SensorMode.bayer_pattern;
    pIsiSensorCaps->HPol = ISI_HPOL_REFPOS;
    pIsiSensorCaps->VPol = ISI_VPOL_NEG;
    pIsiSensorCaps->Edge = ISI_EDGE_RISING;
    pIsiSensorCaps->Resolution.width = pOV12870Ctx->SensorMode.width;
    pIsiSensorCaps->Resolution.height = pOV12870Ctx->SensorMode.height;
    pIsiSensorCaps->SmiaMode = ISI_SMIA_OFF;
    pIsiSensorCaps->MipiLanes = ISI_MIPI_2LANES;

    if (pIsiSensorCaps->BusWidth == 10) {
        pIsiSensorCaps->MipiMode      = ISI_MIPI_MODE_RAW_10;
    }else if (pIsiSensorCaps->BusWidth == 12){
        pIsiSensorCaps->MipiMode      = ISI_MIPI_MODE_RAW_12;
    }else{
        pIsiSensorCaps->MipiMode      = ISI_MIPI_OFF;
    }

    TRACE(OV12870_INFO, "%s (exit)\n", __func__);
    return result;
}

RESULT OV12870_IsiGetSensorIss(IsiSensor_t *pIsiSensor)
{
    RESULT result = RET_SUCCESS;
    TRACE( OV12870_INFO, "%s (enter)\n", __func__);

    if ( pIsiSensor != NULL ) {
        pIsiSensor->pszName                         = SensorName;
        pIsiSensor->pIsiCreateSensorIss             = OV12870_IsiCreateSensorIss;

        pIsiSensor->pIsiInitSensorIss               = OV12870_IsiInitSensorIss;
        pIsiSensor->pIsiGetSensorModeIss            = OV12870_IsiGetSensorModeIss;
        pIsiSensor->pIsiResetSensorIss              = OV12870_IsiResetSensorIss;
        pIsiSensor->pIsiReleaseSensorIss            = OV12870_IsiReleaseSensorIss;
        pIsiSensor->pIsiGetCapsIss                  = OV12870_IsiGetCapsIss;
        pIsiSensor->pIsiSetupSensorIss              = OV12870_IsiSetupSensorIss;
        pIsiSensor->pIsiChangeSensorResolutionIss   = OV12870_IsiChangeSensorResolutionIss;
        pIsiSensor->pIsiSensorSetStreamingIss       = OV12870_IsiSensorSetStreamingIss;
        pIsiSensor->pIsiSensorSetPowerIss           = OV12870_IsiSensorSetPowerIss;
        pIsiSensor->pIsiCheckSensorConnectionIss    = OV12870_IsiCheckSensorConnectionIss;
        pIsiSensor->pIsiGetSensorRevisionIss        = OV12870_IsiGetSensorRevisionIss;
        pIsiSensor->pIsiRegisterReadIss             = OV12870_IsiRegisterReadIss;
        pIsiSensor->pIsiRegisterWriteIss            = OV12870_IsiRegisterWriteIss;

        /* AEC functions */
        pIsiSensor->pIsiExposureControlIss          = OV12870_IsiExposureControlIss;
        pIsiSensor->pIsiGetGainLimitsIss            = OV12870_IsiGetGainLimitsIss;
        pIsiSensor->pIsiGetIntegrationTimeLimitsIss = OV12870_IsiGetIntegrationTimeLimitsIss;
        pIsiSensor->pIsiGetCurrentExposureIss       = OV12870_IsiGetCurrentExposureIss;
        pIsiSensor->pIsiGetVSGainIss                    = OV12870_IsiGetVSGainIss;
        pIsiSensor->pIsiGetGainIss                      = OV12870_IsiGetGainIss;
        pIsiSensor->pIsiGetLongGainIss                  = OV12870_IsiGetLongGainIss;
        pIsiSensor->pIsiGetGainIncrementIss             = OV12870_IsiGetGainIncrementIss;
        pIsiSensor->pIsiSetGainIss                      = OV12870_IsiSetGainIss;
        pIsiSensor->pIsiGetIntegrationTimeIss           = OV12870_IsiGetIntegrationTimeIss;
        pIsiSensor->pIsiGetVSIntegrationTimeIss         = OV12870_IsiGetVSIntegrationTimeIss;
        pIsiSensor->pIsiGetLongIntegrationTimeIss       = OV12870_IsiGetLongIntegrationTimeIss;
        pIsiSensor->pIsiGetIntegrationTimeIncrementIss  = OV12870_IsiGetIntegrationTimeIncrementIss;
        pIsiSensor->pIsiSetIntegrationTimeIss           = OV12870_IsiSetIntegrationTimeIss;
        pIsiSensor->pIsiQuerySensorIss                  = OV12870_IsiQuerySensorIss;
        pIsiSensor->pIsiGetResolutionIss                = OV12870_IsiGetResolutionIss;
        pIsiSensor->pIsiGetSensorFpsIss                 = OV12870_IsiGetSensorFpsIss;
        pIsiSensor->pIsiSetSensorFpsIss                 = OV12870_IsiSetSensorFpsIss;
        pIsiSensor->pIsiSensorGetExpandCurveIss         = OV12870_IsiSensorGetExpandCurveIss;

        /* AWB specific functions */

        /* Testpattern */
        pIsiSensor->pIsiActivateTestPattern         = OV12870_IsiActivateTestPattern;
        pIsiSensor->pIsiSetBayerPattern             = OV12870_IsiSetBayerPattern;

        pIsiSensor->pIsiSensorSetBlcIss             = OV12870_IsiSensorSetBlcIss;
        pIsiSensor->pIsiSensorSetWBIss              = OV12870_IsiSensorSetWBIss;
        pIsiSensor->pIsiGetSensorAWBModeIss         = OV12870_IsiGetSensorAWBModeIss;

    } else {
        result = RET_NULL_POINTER;
    }

    TRACE( OV12870_INFO, "%s (exit)\n", __func__);
    return ( result );
}

/*****************************************************************************
* each sensor driver need declare this struct for isi load
*****************************************************************************/
IsiCamDrvConfig_t OV12870_IsiCamDrvConfig = {
    0,
    OV12870_IsiQuerySensorSupportIss,
    OV12870_IsiGetSensorIss,
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
