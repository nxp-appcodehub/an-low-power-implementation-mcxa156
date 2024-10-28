/*
 * Copyright 2022-2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <low_power_implementation.h>
#include "fsl_cmc.h"
#include "fsl_spc.h"
#include "fsl_clock.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "fsl_lpuart.h"
#include "fsl_wuu.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define APP_POWER_MODE_NAME                                          \
    {                                                                \
        "Active", "Sleep", "DeepSleep", "PowerDown", "DeepPowerDown" \
    }

#define APP_POWER_MODE_DESC                                                                                            			\
    {                                                                                                                  			\
        "Active: CPU/System/Bus clock all ON.",                                                                                	\
        "Sleep: CPU clock is off, and the system clock and bus clock remain ON. ",                                     			\
        "Deep Sleep: CPU/System/Bus clock are gated off. ",                                                           			\
        "Power Down: CPU/System/Bus clock are gated off, CORE domain is in static state, Flash memory is powered off.", 		\
        "Deep Power Down: The whole core domain is power gated."                                           						\
    }

#define APP_CMC           				CMC

#define APP_SPC                   		SPC0
#define APP_SPC_ISO_VALUE         		(0x2U) /* VDD_USB */
#define APP_SPC_ISO_DOMAINS       		"VDD_USB"
#define APP_SPC_MAIN_POWER_DOMAIN 		(kSPC_PowerDomain0)

#define APP_WUU             			WUU0
#define APP_WUU_WAKEUP_IDX  			2U
#define APP_WUU_IRQN        			WUU0_IRQn
#define APP_WUU_IRQ_HANDLER 			WUU0_IRQHandler
#define APP_WUU_WAKEUP_BUTTON_NAME      "SW2"

/* LPUART RX */
#define APP_DEBUG_CONSOLE_RX_PORT   	PORT0
#define APP_DEBUG_CONSOLE_RX_GPIO   	GPIO0
#define APP_DEBUG_CONSOLE_RX_PIN    	2U
#define APP_DEBUG_CONSOLE_RX_PINMUX 	kPORT_MuxAlt2
/* LPUART TX */
#define APP_DEBUG_CONSOLE_TX_PORT   	PORT0
#define APP_DEBUG_CONSOLE_TX_GPIO   	GPIO0
#define APP_DEBUG_CONSOLE_TX_PIN    	3U
#define APP_DEBUG_CONSOLE_TX_PINMUX 	kPORT_MuxAlt2


/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void APP_InitDebugConsole(void);
void APP_DeinitDebugConsole(void);
static void APP_SetSPCConfiguration(void);
static void APP_SetVBATConfiguration(void);
static void APP_SetCMCConfiguration(void);

static void APP_GetWakeupConfig(app_power_mode_t targetMode);

static void APP_PowerPreSwitchHook(void);
static void APP_PowerPostSwitchHook(void);

static void APP_EnterSleepMode(void);
static void APP_EnterDeepSleepMode(void);
static void APP_EnterPowerDownMode(void);
static void APP_EnterDeepPowerDownMode(void);
static void APP_PowerModeSwitch(app_power_mode_t targetPowerMode);
static app_power_mode_t APP_GetTargetPowerMode(void);
static void APP_SetTargetPowerMode(app_power_mode_t targetPowerMode);
static void APP_SetSleepMode(app_power_mode_t targetPowerMode);
static void APP_SetDeepSleepMode(app_power_mode_t targetPowerMode);
static void APP_SetPowerDownMode(app_power_mode_t targetPowerMode);
static void APP_SetDeepPowerDownMode(app_power_mode_t targetPowerMode);
static void APP_SetCoreFreqAndLDO(app_power_mode_t targetPowerMode);
static void APP_SetSIRC(void);
static void APP_SetRAMRetention(app_power_mode_t targetPowerMode);
static void APP_DisVoltageDetect(void);
static void APP_EnVoltageDetect(void);
/*******************************************************************************
 * Variables
 ******************************************************************************/

char *const g_modeNameArray[] = APP_POWER_MODE_NAME;
char *const g_modeDescArray[] = APP_POWER_MODE_DESC;

/*******************************************************************************
 * Code
 ******************************************************************************/
void APP_InitDebugConsole(void)
{
    /*
     * Debug console RX pin is set to disable for current leakage, need to re-configure pinmux.
     * Debug console TX pin: Don't need to change.
     */
    BOARD_InitPins();
    BOARD_BootClockFRO48M(kSPC_CoreLDO_MidDriveVoltage, kSPC_CoreLDO_LowDriveStrength);
    BOARD_InitDebugConsole();
}

void APP_DeinitDebugConsole(void)
{
    DbgConsole_Deinit();
    PORT_SetPinMux(APP_DEBUG_CONSOLE_RX_PORT, APP_DEBUG_CONSOLE_RX_PIN, kPORT_MuxAsGpio);
    PORT_SetPinMux(APP_DEBUG_CONSOLE_TX_PORT, APP_DEBUG_CONSOLE_TX_PIN, kPORT_MuxAsGpio);
}


int main(void)
{
    uint32_t freq;
    app_power_mode_t targetPowerMode;
    bool needSetWakeup = false;

    BOARD_InitPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();

    /* Release the I/O pads and certain peripherals to normal run mode state, for in Power Down mode
     * they will be in a latched state. */
    if ((CMC_GetSystemResetStatus(APP_CMC) & kCMC_WakeUpReset) != 0UL)
    {
        SPC_ClearPeriphIOIsolationFlag(APP_SPC);
    }

    APP_SetVBATConfiguration();
    APP_SetSPCConfiguration();

    /* clear wake up related flag for Deep Power Down */
    WUU0->PF|= WUU_PF_WUF9_MASK;
    NVIC_ClearPendingIRQ(WUU0_IRQn);

    PRINTF("\r\nNormal Boot.\r\n");

    while (1)
    {
        if ((CMC_GetSystemResetStatus(APP_CMC) & kCMC_WakeUpReset) != 0UL)
        {
            /* Clear ISO flags. */
            SPC_ClearPeriphIOIsolationFlag(APP_SPC);
        }

        /* Clear CORE domain's low power request flag. */
        SPC_ClearPowerDomainLowPowerRequestFlag(APP_SPC, APP_SPC_MAIN_POWER_DOMAIN);
        SPC_ClearLowPowerRequest(APP_SPC);

        /* Normal start. */
        APP_SetCMCConfiguration();

        freq = CLOCK_GetFreq(kCLOCK_CoreSysClk);
        PRINTF("\r\n###########################    Power Mode Switch Demo    ###########################\r\n");
        PRINTF("    Core Clock = %dHz \r\n", freq);
        PRINTF("    Power mode: Active\r\n");
        targetPowerMode = APP_GetTargetPowerMode();

        if ((targetPowerMode > kAPP_PowerModeMin) && (targetPowerMode < kAPP_PowerModeMax))
        {
            /* If target mode is Active mode, don't need to set wakeup source. */
            if (targetPowerMode == kAPP_PowerModeActive)
            {
                needSetWakeup = false;
            }
            else
            {
                needSetWakeup = true;
            }
        }

        /* Print description of selected power mode. */
        PRINTF("\r\n");
        if (needSetWakeup)
        {
            APP_GetWakeupConfig(targetPowerMode);
            APP_SetTargetPowerMode(targetPowerMode);
            APP_PowerPreSwitchHook();
            APP_PowerModeSwitch(targetPowerMode);
            APP_PowerPostSwitchHook();
        }

        PRINTF("\r\nNext loop.\r\n");
    }
}

static void APP_SetSPCConfiguration(void)
{
    status_t status;

    spc_active_mode_regulators_config_t activeModeRegulatorOption;

    SPC_EnableSRAMLdo(APP_SPC, true);

    /* Disable all modules that controlled by SPC in active mode.. */
    SPC_DisableActiveModeAnalogModules(APP_SPC, kSPC_controlAllModules);

    /* Disable LVDs and HVDs */
//    SPC_EnableActiveModeCoreLowVoltageDetect(APP_SPC, false);
//    SPC_EnableActiveModeSystemHighVoltageDetect(APP_SPC, false);
//    SPC_EnableActiveModeSystemLowVoltageDetect(APP_SPC, false);
//    activeModeRegulatorOption.bandgapMode = kSPC_BandgapEnabledBufferDisabled;
    activeModeRegulatorOption.CoreLDOOption.CoreLDOVoltage       = kSPC_CoreLDO_MidDriveVoltage;
    activeModeRegulatorOption.CoreLDOOption.CoreLDODriveStrength = kSPC_CoreLDO_NormalDriveStrength;

    status = SPC_SetActiveModeRegulatorsConfig(APP_SPC, &activeModeRegulatorOption);
#if !(defined(FSL_FEATURE_MCX_SPC_HAS_NO_GLITCH_DETECT) && FSL_FEATURE_MCX_SPC_HAS_NO_GLITCH_DETECT)
    /* Disable Vdd Core Glitch detector in active mode. */
    SPC_DisableActiveModeVddCoreGlitchDetect(APP_SPC, true);
#endif
    if (status != kStatus_Success)
    {
        PRINTF("Fail to set regulators in Active mode.");
        return;
    }
    while (SPC_GetBusyStatusFlag(APP_SPC))
        ;

    SPC_DisableLowPowerModeAnalogModules(APP_SPC, kSPC_controlAllModules);
    SPC_SetLowPowerWakeUpDelay(APP_SPC, 0x5b);
    spc_lowpower_mode_regulators_config_t lowPowerRegulatorOption;

    lowPowerRegulatorOption.lpIREF                             = false;
    lowPowerRegulatorOption.bandgapMode                        = kSPC_BandgapDisabled;
    lowPowerRegulatorOption.CoreLDOOption.CoreLDOVoltage       = kSPC_CoreLDO_MidDriveVoltage;
    lowPowerRegulatorOption.CoreLDOOption.CoreLDODriveStrength = kSPC_CoreLDO_LowDriveStrength;

    status = SPC_SetLowPowerModeRegulatorsConfig(APP_SPC, &lowPowerRegulatorOption);
#if !(defined(FSL_FEATURE_MCX_SPC_HAS_NO_GLITCH_DETECT) && FSL_FEATURE_MCX_SPC_HAS_NO_GLITCH_DETECT)
    /* Disable Vdd Core Glitch detector in low power mode. */
    SPC_DisableLowPowerModeVddCoreGlitchDetect(APP_SPC, true);
#endif
    if (status != kStatus_Success)
    {
        PRINTF("Fail to set regulators in Low Power Mode.");
        return;
    }
    while (SPC_GetBusyStatusFlag(APP_SPC))
        ;

    /* Enable Low power request output to observe the entry/exit of
     * low power modes(including: deep sleep mode, power down mode, and deep power down mode).
     */
    spc_lowpower_request_config_t lpReqConfig = {
        .enable   = true,
        .override = kSPC_LowPowerRequestNotForced,
        .polarity = kSPC_LowTruePolarity,
    };

    SPC_SetLowPowerRequestConfig(APP_SPC, &lpReqConfig);
}

static void APP_SetVBATConfiguration(void)
{
    CLOCK_SetupFRO16KClocking(kCLKE_16K_SYSTEM | kCLKE_16K_COREMAIN);
}

static void APP_SetCMCConfiguration(void)
{
    /* Disable low power debug. */
    CMC_EnableDebugOperation(APP_CMC, false);
    /* Allow all power mode */
    CMC_SetPowerModeProtection(APP_CMC, kCMC_AllowAllLowPowerModes);

    /* Disable flash memory accesses and place flash memory in low-power state whenever the core clock
       is gated. And an attempt to access the flash memory will cause the flash memory to exit low-power
       state for the duration of the flash memory access. */
    CMC_ConfigFlashMode(APP_CMC, true, true, false);
}

static app_power_mode_t APP_GetTargetPowerMode(void)
{
    uint8_t ch;

    app_power_mode_t inputPowerMode;

    do
    {
        PRINTF("\r\nSelect the desired operation \n\r\n");
        for (app_power_mode_t modeIndex = kAPP_PowerModeActive; modeIndex <= kAPP_PowerModeDeepPowerDown; modeIndex++)
        {
            PRINTF("\tPress %c to enter: %s mode\r\n", modeIndex,
                   g_modeNameArray[(uint8_t)(modeIndex - kAPP_PowerModeActive)]);
        }

        PRINTF("\r\nWaiting for power mode select...\r\n\r\n");

        ch = GETCHAR();

        if ((ch >= 'a') && (ch <= 'z'))
        {
            ch -= 'a' - 'A';
        }
        inputPowerMode = (app_power_mode_t)ch;

        if ((inputPowerMode > kAPP_PowerModeDeepPowerDown) || (inputPowerMode < kAPP_PowerModeActive))
        {
            PRINTF("Wrong Input!");
        }
    } while (inputPowerMode > kAPP_PowerModeDeepPowerDown);

    PRINTF("\t%s\r\n", g_modeDescArray[(uint8_t)(inputPowerMode - kAPP_PowerModeActive)]);

    return inputPowerMode;
}

static void APP_SetTargetPowerMode(app_power_mode_t targetPowerMode)
{
	switch(targetPowerMode)
	{
		case kAPP_PowerModeSleep:
			APP_SetSleepMode(targetPowerMode);
			break;
		case kAPP_PowerModeDeepSleep:
			APP_SetDeepSleepMode(targetPowerMode);
			break;
		case kAPP_PowerModePowerDown:
			APP_SetPowerDownMode(targetPowerMode);
			break;
		case kAPP_PowerModeDeepPowerDown:
			APP_SetDeepPowerDownMode(targetPowerMode);
			break;
		default:
			assert(false);
			break;
	}
}

static void APP_SetSleepMode(app_power_mode_t targetPowerMode)
{
	APP_SetCoreFreqAndLDO(targetPowerMode);
	PRINTF("\r\nEntering Sleep mode...\r\n");
	PRINTF("Please press %s to wakeup.(Please only press the wakeup button when this message appears, otherwise it will result in failure to wake up!)\r\n", APP_WUU_WAKEUP_BUTTON_NAME);
    SCG0->FIRCCSR &= ~SCG_FIRCCSR_LK_MASK;
    SCG0->FIRCCSR &= ~SCG_FIRCCSR_FIRC_SCLK_PERIPH_EN_MASK;
    SCG0->FIRCCSR &= ~SCG_FIRCCSR_FIRC_FCLK_PERIPH_EN_MASK;
    SCG0->FIRCCSR |= SCG_FIRCCSR_LK_MASK;

    SCG0->SIRCCSR &= ~SCG_SIRCCSR_LK_MASK;
    SCG0->SIRCCSR &= ~SCG_SIRCCSR_SIRC_CLK_PERIPH_EN_MASK;
    SCG0->SIRCCSR |= SCG_SIRCCSR_LK_MASK;
}

static void APP_SetDeepSleepMode(app_power_mode_t targetPowerMode)
{
	APP_SetCoreFreqAndLDO(targetPowerMode);
	APP_SetSIRC();
	PRINTF("\r\nEntering DeepSleep mode...\r\n");
	PRINTF("Please press %s to wakeup.(Please only press the wakeup button when this message appears, otherwise it will result in failure to wake up!)\r\n", APP_WUU_WAKEUP_BUTTON_NAME);
}

static void APP_SetPowerDownMode(app_power_mode_t targetPowerMode)
{
	APP_SetCoreFreqAndLDO(targetPowerMode);
	APP_SetRAMRetention(targetPowerMode);
	PRINTF("\r\nEntering PowerDown mode...\r\n");
	PRINTF("Please press %s to wakeup.(Please only press the wakeup button when this message appears, otherwise it will result in failure to wake up!)\r\n", APP_WUU_WAKEUP_BUTTON_NAME);
}

static void APP_SetDeepPowerDownMode(app_power_mode_t targetPowerMode)
{
	APP_SetCoreFreqAndLDO(targetPowerMode);
	APP_SetRAMRetention(targetPowerMode);
	PRINTF("\r\nEntering DeepPowerDown mode...\r\n");
	PRINTF("Please press %s to wakeup.(Please only press the wakeup button when this message appears, otherwise it will result in failure to wake up!)\r\n", APP_WUU_WAKEUP_BUTTON_NAME);
}

static void APP_SetCoreFreqAndLDO(app_power_mode_t targetPowerMode)
{
	uint8_t ch;

	do
	{
		PRINTF("\r\nSelect the desired Core Frequency and LDO configuration:\n\r\n");
		PRINTF("\tA: CPU_CLK=96MHz(FRO192M), VDD_CORE=1.1V\r\n");
		if(targetPowerMode == kAPP_PowerModePowerDown)
		{
			PRINTF("\tB: CPU_CLK=48MHz(FRO192M), VDD_CORE=0.6V\r\n");
			PRINTF("\tC: CPU_CLK=12MHz(FRO12M) , VDD_CORE=0.6V\n\r\n");
		}
		else
		{
			PRINTF("\tB: CPU_CLK=48MHz(FRO192M), VDD_CORE=1.0V\r\n");
			PRINTF("\tC: CPU_CLK=12MHz(FRO12M) , VDD_CORE=1.0V\n\r\n");
		}

		ch = GETCHAR();

        if ((ch >= 'a') && (ch <= 'z'))
        {
            ch -= 'a' - 'A';
        }
		if((ch < 'A') || (ch > 'C'))
		{
			PRINTF("Wrong Input!");
		}

	}while((ch < 'A') || (ch > 'C'));

	switch(ch)
	{
		case 'A':
			PRINTF("\tSelect CPU_CLK=96MHz(FRO192M), VDD_CORE=1.1V\r\n");
			SPC_SetLowPowerWakeUpDelay(APP_SPC, 0x0);
			BOARD_BootClockFRO96M(kSPC_CoreLDO_NormalVoltage, kSPC_CoreLDO_NormalDriveStrength);
			break;
		case 'B':
			if(targetPowerMode == kAPP_PowerModePowerDown)
			{
				PRINTF("\tSelect CPU_CLK=48MHz(FRO192M), VDD_CORE=0.6V\r\n");
				SPC_SetLowPowerWakeUpDelay(APP_SPC, 0x5b);
				BOARD_BootClockFRO48M(kSPC_Core_LDO_RetentionVoltage, kSPC_CoreLDO_LowDriveStrength);
			}
			else
			{
				PRINTF("\tSelect CPU_CLK=48MHz(FRO192M), VDD_CORE=1.0V\r\n");
				SPC_SetLowPowerWakeUpDelay(APP_SPC, 0x0);
				BOARD_BootClockFRO48M(kSPC_CoreLDO_MidDriveVoltage, kSPC_CoreLDO_LowDriveStrength);
			}
			break;
		case 'C':
			if(targetPowerMode == kAPP_PowerModePowerDown)
			{
				PRINTF("\tSelect CPU_CLK=12MHz(FRO12M), VDD_CORE=0.6V\r\n");
				SPC_SetLowPowerWakeUpDelay(APP_SPC, 0x5b);
				BOARD_BootClockFRO12M(kSPC_Core_LDO_RetentionVoltage, kSPC_CoreLDO_LowDriveStrength);
			}
			else
			{
				PRINTF("\tSelect CPU_CLK=12MHz(FRO12M), VDD_CORE=1.0V\r\n");
				SPC_SetLowPowerWakeUpDelay(APP_SPC, 0x0);
				if(targetPowerMode == kAPP_PowerModeSleep)
				{
					APP_DisVoltageDetect();
				}
				BOARD_BootClockFRO12M(kSPC_CoreLDO_MidDriveVoltage, kSPC_CoreLDO_LowDriveStrength);

			    SCG0->FIRCCSR &= ~SCG_FIRCCSR_LK_MASK;
			    SCG0->FIRCCSR &= ~SCG_FIRCCSR_FIRC_SCLK_PERIPH_EN_MASK;
			    SCG0->FIRCCSR &= ~SCG_FIRCCSR_FIRC_FCLK_PERIPH_EN_MASK;
			    SCG0->FIRCCSR &= ~SCG_FIRCCSR_FIRCEN_MASK;
			    SCG0->FIRCCSR |= SCG_FIRCCSR_LK_MASK;
			}
			break;
		default:
			assert(false);
			break;
	}
}

static void APP_SetSIRC(void)
{
	uint8_t ch;

	do
	{
		PRINTF("\r\nConfigure FRO12M in DeepSleep mode:\n\r\n");
		PRINTF("\tA: Enable FRO12M and clock to peripherals in DeepSleep mode\r\n");
		PRINTF("\tB: Disable FROM12M in DeepSleep mode\r\n");

		ch = GETCHAR();

        if ((ch >= 'a') && (ch <= 'z'))
        {
            ch -= 'a' - 'A';
        }
		if((ch < 'A') || (ch > 'B'))
		{
			PRINTF("Wrong Input!");
		}

	}while((ch < 'A') || (ch > 'B'));

	switch(ch)
	{
		case 'A':
			PRINTF("\tSelect ENABLE FRO12M in DeepSleep mode\r\n");
		    /* Unlock SIRCCSR */
		    SCG0->SIRCCSR &= ~SCG_SIRCCSR_LK_MASK;

			SCG0->SIRCCSR |= SCG_SIRCCSR_SIRCSTEN_MASK;
			SCG0->SIRCCSR |= SCG_SIRCCSR_SIRC_CLK_PERIPH_EN_MASK;

			/* Lock SIRCCSR */
		    SCG0->SIRCCSR |= SCG_SIRCCSR_LK_MASK;
			break;
		case 'B':
			PRINTF("\tSelect DISABLE FRO12M in DeepSleep mode\r\n");
		    /* Unlock SIRCCSR */
		    SCG0->SIRCCSR &= ~SCG_SIRCCSR_LK_MASK;

			SCG0->SIRCCSR &= ~SCG_SIRCCSR_SIRCSTEN_MASK;

			/* Lock SIRCCSR */
		    SCG0->SIRCCSR |= SCG_SIRCCSR_LK_MASK;
			break;
		default:
			assert(false);
			break;
	}
}

static void APP_SetRAMRetention(app_power_mode_t targetPowerMode)
{
	uint8_t ch;

	do
	{
		PRINTF("\r\nConfigure the RAM retention:\n\r\n");
		if(targetPowerMode == kAPP_PowerModeDeepPowerDown)
		{
			PRINTF("\tA: No RAM retained\r\n");
		}
		PRINTF("\tB: All RAM retained\r\n");
		PRINTF("\tC: RAMX0/X1, RAMA0~A3 retained\r\n");
		PRINTF("\tD: RAMX0/X1/A0 retained\r\n");
		if(targetPowerMode == kAPP_PowerModeDeepPowerDown)
		{
			PRINTF("\tE: RAMA0 retained\r\n");
		}
		PRINTF("\tF: RAMX0/X1 retained\r\n\n");

		ch = GETCHAR();

        if ((ch >= 'a') && (ch <= 'z'))
        {
            ch -= 'a' - 'A';
        }

		if((ch < 'A') || (ch > 'F'))
		{
			PRINTF("Wrong Input!");
		}
	}while((ch < 'A') || (ch > 'F'));


	switch(ch)
	{
		case 'A':
			if(targetPowerMode == kAPP_PowerModeDeepPowerDown)
			{
				PRINTF("\tSelect No RAM retained\r\n");
				SPC0->LP_CFG &= ~SPC_LP_CFG_SRAMLDO_DPD_ON_MASK;
			}
			break;
		case 'B':
			PRINTF("\tSelect All RAM retained\r\n");
			SPC0->LP_CFG |= SPC_LP_CFG_SRAMLDO_DPD_ON_MASK;
			SPC0->SRAMRETLDO_CNTRL |= SPC_SRAMRETLDO_CNTRL_SRAM_RET_EN_MASK;
			break;
		case 'C':
			PRINTF("\tSelect RAMX0/X1, RAMA0~A3 retained\r\n");
			SPC0->LP_CFG |= SPC_LP_CFG_SRAMLDO_DPD_ON_MASK;
	        SPC0->SRAMRETLDO_CNTRL &= ~SPC_SRAMRETLDO_CNTRL_SRAM_RET_EN_MASK;
	        SPC0->SRAMRETLDO_CNTRL |= (7 << SPC_SRAMRETLDO_CNTRL_SRAM_RET_EN_SHIFT);
			break;
		case 'D':
			PRINTF("\tSelect RAMX0/X1/A0 retained\r\n");
			SPC0->LP_CFG |= SPC_LP_CFG_SRAMLDO_DPD_ON_MASK;
	        SPC0->SRAMRETLDO_CNTRL &= ~SPC_SRAMRETLDO_CNTRL_SRAM_RET_EN_MASK;
	        SPC0->SRAMRETLDO_CNTRL |= (3 << SPC_SRAMRETLDO_CNTRL_SRAM_RET_EN_SHIFT);
			break;
		case 'E':
			if(targetPowerMode == kAPP_PowerModeDeepPowerDown)
			{
				PRINTF("\tSelect RAMA0 retained\r\n");
				SPC0->LP_CFG |= SPC_LP_CFG_SRAMLDO_DPD_ON_MASK;
				SPC0->SRAMRETLDO_CNTRL &= ~SPC_SRAMRETLDO_CNTRL_SRAM_RET_EN_MASK;
				SPC0->SRAMRETLDO_CNTRL |= (2 << SPC_SRAMRETLDO_CNTRL_SRAM_RET_EN_SHIFT);
			}
			break;
		case 'F':
			PRINTF("\tSelect RAMX0/RAMX1 retained\r\n");
			SPC0->LP_CFG |= SPC_LP_CFG_SRAMLDO_DPD_ON_MASK;
	        SPC0->SRAMRETLDO_CNTRL &= ~SPC_SRAMRETLDO_CNTRL_SRAM_RET_EN_MASK;
	        SPC0->SRAMRETLDO_CNTRL |= (1 << SPC_SRAMRETLDO_CNTRL_SRAM_RET_EN_SHIFT);
			break;
		default:
			assert(false);
			break;
	}
}

/* Get wakeup timeout and wakeup source. */
static void APP_GetWakeupConfig(app_power_mode_t targetMode)
{
    char *isoDomains = NULL;

    PRINTF("Wakeup Button Selected As Wakeup Source.\r\n");
    /* Set WUU to detect on rising edge for all power modes. */
    wuu_external_wakeup_pin_config_t wakeupButtonConfig;

    wakeupButtonConfig.edge  = kWUU_ExternalPinFallingEdge;
    wakeupButtonConfig.event = kWUU_ExternalPinInterrupt;
    wakeupButtonConfig.mode  = kWUU_ExternalPinActiveAlways;
    WUU_SetExternalWakeUpPinsConfig(APP_WUU, 9, &wakeupButtonConfig);

    if (targetMode > kAPP_PowerModeSleep)
    {
        /* Isolate some power domains that are not used in low power modes.*/
        SPC_SetExternalVoltageDomainsConfig(APP_SPC, APP_SPC_ISO_VALUE, 0x0U);
        isoDomains = APP_SPC_ISO_DOMAINS;
        PRINTF("Isolate power domains: %s\r\n", isoDomains);
    }
}

static void APP_PowerPreSwitchHook(void)
{
    /* Wait for debug console output finished. */
    while (!(kLPUART_TransmissionCompleteFlag & LPUART_GetStatusFlags((LPUART_Type *)BOARD_DEBUG_UART_BASEADDR)))
    {
    }
    APP_DeinitDebugConsole();

    SYSCON->CLKUNLOCK &= ~SYSCON_CLKUNLOCK_UNLOCK_MASK;
    MRCC0->MRCC_GLB_CC0 = 0x00008000;
    MRCC0->MRCC_GLB_CC1 = 0x000C0000;
    MRCC0->MRCC_GLB_ACC0 = 0x00008000;
    MRCC0->MRCC_GLB_ACC1 = 0x000C0000;
    SYSCON->CLKUNLOCK |= SYSCON_CLKUNLOCK_UNLOCK_MASK;
}

static void APP_PowerPostSwitchHook(void)
{
    BOARD_BootClockFRO48M(kSPC_CoreLDO_MidDriveVoltage, kSPC_CoreLDO_LowDriveStrength);
    SPC_SetActiveModeBandgapModeConfig(SPC0, kSPC_BandgapEnabledBufferDisabled);
    APP_EnVoltageDetect();
    SYSCON->CLKUNLOCK &= ~SYSCON_CLKUNLOCK_UNLOCK_MASK;
    MRCC0->MRCC_GLB_CC0 = 0x08408000;
    MRCC0->MRCC_GLB_CC1 = 0x00800080;
    MRCC0->MRCC_GLB_ACC0 = 0x08008000;
    MRCC0->MRCC_GLB_ACC1 = 0x020C0000;
    SYSCON->CLKUNLOCK |= SYSCON_CLKUNLOCK_UNLOCK_MASK;
    APP_InitDebugConsole();
}

static void APP_PowerModeSwitch(app_power_mode_t targetPowerMode)
{
    if (targetPowerMode != kAPP_PowerModeActive)
    {
        switch (targetPowerMode)
        {
            case kAPP_PowerModeSleep:
                APP_EnterSleepMode();
                break;
            case kAPP_PowerModeDeepSleep:
                APP_EnterDeepSleepMode();
                break;
            case kAPP_PowerModePowerDown:
                APP_EnterPowerDownMode();
                break;
            case kAPP_PowerModeDeepPowerDown:
                APP_EnterDeepPowerDownMode();
                break;
            default:
                assert(false);
                break;
        }
    }
}

static void APP_EnterSleepMode(void)
{
    cmc_power_domain_config_t config;

    config.clock_mode  = kCMC_GateNoneClock;
    config.main_domain = kCMC_ActiveOrSleepMode;

    CMC_EnterLowPowerMode(APP_CMC, &config);
}

static void APP_EnterDeepSleepMode(void)
{
    cmc_power_domain_config_t config;

    config.clock_mode  = kCMC_GateAllSystemClocksEnterLowPowerMode;
    config.main_domain = kCMC_DeepSleepMode;

    CMC_EnterLowPowerMode(APP_CMC, &config);
}

static void APP_EnterPowerDownMode(void)
{
    cmc_power_domain_config_t config;

    config.clock_mode  = kCMC_GateAllSystemClocksEnterLowPowerMode;
    config.main_domain = kCMC_PowerDownMode;

    CMC_EnterLowPowerMode(APP_CMC, &config);
}

static void APP_EnterDeepPowerDownMode(void)
{
    cmc_power_domain_config_t config;

    config.clock_mode  = kCMC_GateAllSystemClocksEnterLowPowerMode;
    config.main_domain = kCMC_DeepPowerDown;

    CMC_EnterLowPowerMode(APP_CMC, &config);
}

static void APP_DisVoltageDetect(void)
{
	SPC_EnableActiveModeCoreLowVoltageDetect(APP_SPC, false);
	SPC_EnableActiveModeSystemHighVoltageDetect(APP_SPC, false);
	SPC_EnableActiveModeSystemLowVoltageDetect(APP_SPC, false);
}

static void APP_EnVoltageDetect(void)
{
	SPC_EnableActiveModeCoreLowVoltageDetect(APP_SPC, true);
	SPC_EnableActiveModeSystemHighVoltageDetect(APP_SPC, true);
	SPC_EnableActiveModeSystemLowVoltageDetect(APP_SPC, true);
}
