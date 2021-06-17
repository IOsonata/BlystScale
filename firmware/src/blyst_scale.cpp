/**-------------------------------------------------------------------------
@example	blyst_scale.cpp

@brief	BLE scale demo

This demo show how to read load cell using the Nuvoton NAU7802 and advertise
over BLE.

@author	Hoang Nguyen Hoan
@date	Nov. 3, 2019

@license

MIT License

Copyright (c) 2019, I-SYST inc., all rights reserved

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

----------------------------------------------------------------------------*/
#include "app_util_platform.h"
#include "app_scheduler.h"

#include "istddef.h"
#include "ble_app.h"
#include "ble_service.h"
#include "bluetooth/blueio_blesrvc.h"
#include "blueio_board.h"
#include "coredev/uart.h"
#include "coredev/i2c.h"
#include "custom_board.h"
#include "iopinctrl.h"
#include "converters/adc_nau7802.h"
#include "bluetooth/bleadv_mandata.h"

#include "board.h"

#define DEVICE_NAME                     "BlystScale"                          /**< Name of device. Will be included in the advertising data. */

#define MANUFACTURER_NAME               "I-SYST inc."                       /**< Manufacturer. Will be passed to Device Information Service. */


#define MANUFACTURER_ID                 ISYST_BLUETOOTH_ID                  /**< Manufacturer ID, part of System ID. Will be passed to Device Information Service. */
#define ORG_UNIQUE_ID                   ISYST_BLUETOOTH_ID                  /**< Organizational Unique ID, part of System ID. Will be passed to Device Information Service. */

#define APP_ADV_INTERVAL                MSEC_TO_UNITS(64, UNIT_0_625_MS)	/**< The advertising interval (in units of 0.625 ms. This value corresponds to 40 ms). */

#define APP_ADV_TIMEOUT					MSEC_TO_UNITS(500, UNIT_10_MS)		/**< The advertising timeout (in units of 10ms seconds). */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(10, UNIT_1_25_MS)     /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(40, UNIT_1_25_MS)     /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */

BleAdvManData_t g_AdvData = {
	BLUEIO_DATA_TYPE_ADC,
};

const BleAppDevInfo_t s_UartBleDevDesc = {
	MODEL_NAME,       		// Model name
	MANUFACTURER_NAME,		// Manufacturer name
	"123",					// Serial number string
	"0.0",					// Firmware version string
	"0.0",					// Hardware version string
};

const BleAppCfg_t s_BleAppCfg = {
	.ClkCfg = { NRF_CLOCK_LF_SRC_XTAL, 0, 0, NRF_CLOCK_LF_ACCURACY_20_PPM},
	.CentLinkCount = 0, 				// Number of central link
	.PeriLinkCount = 1, 				// Number of peripheral link
	.AppMode = BLEAPP_MODE_NOCONNECT,	// Use scheduler
	.pDevName = DEVICE_NAME,			// Device name
	.VendorID = ISYST_BLUETOOTH_ID,		// PnP Bluetooth/USB vendor id
	.ProductId = 1,						// PnP Product ID
	.ProductVer = 0,					// Pnp prod version
	.bEnDevInfoService = true,			// Enable device information service (DIS)
	.pDevDesc = &s_UartBleDevDesc,
	.pAdvManData = (uint8_t*)&g_AdvData,			// Manufacture specific data to advertise
	.AdvManDataLen = sizeof(BleAdvManData_t),	// Length of manufacture specific data
	.pSrManData = NULL,
	.SrManDataLen = 0,
	.SecType = BLEAPP_SECTYPE_NONE,//BLEAPP_SECTYPE_STATICKEY_MITM,//BLEAPP_SECTYPE_NONE,    // Secure connection type
	.SecExchg = BLEAPP_SECEXCHG_NONE,	// Security key exchange
	.pAdvUuids = NULL,      			// Service uuids to advertise
	.NbAdvUuid = 0, 					// Total number of uuids
	.AdvInterval = APP_ADV_INTERVAL,	// Advertising interval in msec
	.AdvTimeout = APP_ADV_TIMEOUT,		// Advertising timeout in sec
	.AdvSlowInterval = 0,				// Slow advertising interval, if > 0, fallback to
										// slow interval on adv timeout and advertise until connected
	.ConnIntervalMin = MIN_CONN_INTERVAL,
	.ConnIntervalMax = MAX_CONN_INTERVAL,
	.ConnLedPort = -1,// Led port nuber
	.ConnLedPin = -1,// Led pin number
	.TxPower = 0,						// Tx power
	.SDEvtHandler = NULL				// RTOS Softdevice handler
};

static const IOPinCfg_t s_I2cPins[] = {
	{I2C_SDA_PORT, I2C_SDA_PIN, I2C_SDA_PINOP, IOPINDIR_BI, IOPINRES_PULLUP, IOPINTYPE_OPENDRAIN},		// SDA
	{I2C_SCL_PORT, I2C_SCL_PIN, I2C_SCL_PINOP, IOPINDIR_OUTPUT, IOPINRES_PULLUP, IOPINTYPE_OPENDRAIN},	// SCL
};

static const I2CCfg_t s_I2cCfg = {
	.DevNo = 0,			// I2C device number
	.Type = I2CTYPE_STANDARD,
	.Mode = I2CMODE_MASTER,
	.pIOPinMap = s_I2cPins,
	.NbIOPins = sizeof(s_I2cPins) / sizeof(IOPinCfg_t),
	.Rate = 100000,		// Rate
	.MaxRetry = 5,			// Retry
	.NbSlaveAddr = 0,			// Number of slave addresses
	.SlaveAddr = {0,},		// Slave addresses
	.bDmaEn = true,
	.bIntEn = false,
	.IntPrio = 7,			// Interrupt prio
	.EvtCB = NULL		// Event callback
};

I2C g_I2C;

static const AdcRefVolt_t s_RefVolt = {
	.Type = ADC_REFVOLT_TYPE_INTERNAL,
	.Voltage = 4.5,
	.Pin = -1
};

void AdcEvt(Device * const pDev, DEV_EVT Evt);

static const AdcCfg_t s_AdcCfg = {
	.Mode = ADC_CONV_MODE_CONTINUOUS,
	.pRefVolt = &s_RefVolt,
	.NbRefVolt = 1,
	.DevAddr = NAU7802_I2C_ADDR,
	.Resolution = 24,
	.Rate = 10000,
	.OvrSample = 0,
	.bInterrupt = true,
	.IntPrio = APP_IRQ_PRIORITY_LOW,
	.EvtHandler = AdcEvt,
};

static const AdcChanCfg_t s_AdcChanCfg[] = {
	{
		.Chan = 0,
		.RefVoltIdx = 0,
		.Type = ADC_CHAN_TYPE_DIFFERENTIAL,
		.Gain = 128,
	},
};

static const IOPinCfg_t s_NauIntPin = {
	NAU7802_INT_PORT, NAU7802_INT_PIN, NAU7802_INT_PINOP, IOPINDIR_INPUT, IOPINRES_PULLUP, IOPINTYPE_NORMAL
};

int g_DelayCnt = 0;
volatile bool g_bUartState = false;
AdcNau7802 g_Adc;
volatile bool g_BleStarted = false;
volatile bool g_bData = false;


void BleAppAdvTimeoutHandler()
{
	BleAppAdvManDataSet((uint8_t*)&g_AdvData, sizeof(BleAdvManData_t), NULL, 0);

	BleAppAdvStart(BLEAPP_ADVMODE_FAST);
}

void AdcEvt(Device * const pDev, DEV_EVT Evt)
{
	AdcNau7802 *dev = (AdcNau7802*)pDev;
	ADC_DATA data;

	dev->Read(0, &data);

	BLUEIO_DATA_ADC *p = (BLUEIO_DATA_ADC*) g_AdvData.Data;

	p->ChanId = 0;
	p->Voltage = data.Data;

	if (g_BleStarted)
	{
		//BleAppAdvManDataSet((uint8_t*)&g_AdvData, sizeof(BLEADV_MANDATA), NULL, 0);
	}
}

void NauIntHandler(int IntNo, void *pCtx)
{
	if (IntNo == 0)
	{
		g_Adc.IntHandler();
		g_bData = true;
	}
}

void BleAppInitUserData()
{
	g_Adc.OpenChannel(s_AdcChanCfg, 2);
	g_Adc.StartConversion();
}

void HardwareInit()
{
	g_I2C.Init(s_I2cCfg);

	IOPinCfg(&s_NauIntPin, 1);
	IOPinEnableInterrupt(0, APP_IRQ_PRIORITY_LOW, NAU7802_INT_PORT, NAU7802_INT_PIN, IOPINSENSE_HIGH_TRANSITION, NauIntHandler, NULL);

	g_Adc.Init(s_AdcCfg, nullptr, &g_I2C);
}

//
// Print a greeting message on standard output and exit.
//
// On embedded platforms this might require semi-hosting or similar.
//
// For example, for toolchains derived from GNU Tools for Embedded,
// to enable semi-hosting, the following was added to the linker:
//
// --specs=rdimon.specs -Wl,--start-group -lgcc -lc -lc -lm -lrdimon -Wl,--end-group
//
// Adjust it for other toolchains.
//

int main()
{
    HardwareInit();

    BleAppInit((const BLEAPP_CFG *)&s_BleAppCfg, true);

    g_BleStarted = true;

	BleAppRun();

	return 0;
}
