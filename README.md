# NXP Application Code Hub
[<img src="https://mcuxpresso.nxp.com/static/icon/nxp-logo-color.svg" width="100"/>](https://www.nxp.com)

## AN14447: How to implement low power on MCXA156

The low power implementation demo provides different user interfaces to facilitate users to measure different power consumption and corresponding wake-up time.

#### Boards: FRDM-MCXA156
#### Categories: Low Power
#### Toolchains: MCUXpresso IDE

## Table of Contents
1. [Software](#step1)
2. [Hardware](#step2)
3. [Setup](#step3)
4. [Results](#step4)
5. [FAQs](#step5) 
6. [Support](#step6)
7. [Release Notes](#step7)

## 1. Software<a name="step1"></a>
- [MCUXpresso IDE V11.9.0 or later](https://www.nxp.com/design/software/development-software/mcuxpresso-software-and-tools-/mcuxpresso-integrated-development-environment-ide:MCUXpresso-IDE)
- [SDK_2.16.0_FRDM-MCXA156](https://mcuxpresso.nxp.com/en/welcome)
- MCUXpresso for Visual Studio Code: This example supports MCUXpresso for Visual Studio Code, for more information about how to use Visual Studio Code please refer [here](https://www.nxp.com/design/training/getting-started-with-mcuxpresso-for-visual-studio-code:TIP-GETTING-STARTED-WITH-MCUXPRESSO-FOR-VS-CODE).

## 2. Hardware<a name="step2"></a>
- FRDM-MCXA156 board:

![FRDM-MCXA156](image/FRDM_MCXA156.png)

- One Type-C USB cable.

> If you want to measure power consumption, prepare [MCU-Link Pro](https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/mcu-link-pro-debug-probe:MCU-LINK-PRO) or Multimeter.

>If you want to measure wake-up time, **rework the FRDM-MCXA156 to connect SW2-4 to J1-1** and prepare an oscilloscope or logic analyzer.

## 3. Setup<a name="step3"></a>

### 3.1 Hardware connection
- Use a Type-C USB cable to connect J21 of FRDM-MCXA156 and the USB port of the PC.

### 3.2 Import Project
1. Open MCUXpresso IDE 11.9.0, in the Quick Start Panel, choose **Import from Application Code Hub**

![import_from_ACH](image/import_from_ACH.png)


2. Enter the **demo name** in the search bar.

![ACH](image/ACH.JPG)

3. Click **Copy GitHub link**, MCUXpresso IDE will automatically retrieve project attributes, then click **Next>**.

![copy_github_link](image/copy_github_link.png)

4. Select **main** branch and then click **Next>**, select the MCUXpresso project, click **Finish** button to complete import.

> You need to install the [SDK_2.16.0_FRDM-MCXA156](https://mcuxpresso.nxp.com/en/welcome) on your MCUXpresso IDE.

### 3.3 Build and Flash Project
- Click **Build** button from the toolbar, then wait for the build to complete.

![MUCXpresso_Build](image/MCUXpresso_Build.png)

- Select the **GUI Flash Tool** from the toolbar to program the executable to the board.

![MUCXpresso_Flash](image/MCUXpresso_Flash.png)

### 3.4 Select low power mode and corresponding configurations
1. Open a serial terminal with a 115200 baud rate.
2. Follow the prompts and enter one from A to E to enter different low power mode.

![select_low_power_mode](image/select_low_power_mode.png)

3. Different low power modes will provide different configurations, and you need to select the corresponding configuration according to the prompts. The following screenshot shows the configurations provided in Deep Power Down mode.

![select_low_power_configurations](image/select_low_power_configurations.png)

4. The following screenshot shows the entire configuration process, and press SW2 on FRDM-MCXA156 to wake up the MCU. 
 
  **Please only press the wakeup button when prompt message appears, otherwise it will result in failure to wake up!**

![all_prompts](image/all_prompts.png) 

### 3.5 Measure power consumption
- Use an multimeter to measure the current at JP2 of the FRDM-MCXA156 board.

![Multimeter_current_measurement](image/Multimeter_current_measurement.png)

- Use MCU-Link Pro and MCUXpresso IDE to measure power consumption:
  - Configure the MCU-Link Pro current measurement range from 350mA to 50mA by changing the 3 jumpers J16/J17/J18 to short pins 2-3.
  - Connect MCU-Link Pro board to FRDM-MCXA156 board.

  |MCU-Link Pro|FRDM-MCXA156|
  |--|--|
  |J9-1|JP2-1|
  |J9-3|JP2-2|
  |J9-2|J3-14|

  - Follow the below steps to measure current with MCUXpresso IDE.

  ![MCUXpresso_energy_measurement](image/MCUXpresso_energy_measurement.png)

### 3.6 Measure wake-up time
- Get the wake-up time by measuring the delay between the falling edges of J1-1 (P1_7) and J6-1 (P3_30) using logic analyzer or oscilloscope.

![measure_wake_up_time](image/measure_wake_up_time.png)


## 4. Results<a name="step4"></a>
The following power consumption and wake-up time are provided as a reference.

>Different samples, temperature, and measuring instruments affect test results.

>Before measuring each data, POR is recommended.

>This demo is not configured exactly the same as the data sheet, so the test data may be slightly different.

>Refer to “Power mode transition operating behaviors” table in MCXA156 data sheet that lists wake-up time, and “Power consumption operating behaviors” section in MCXA156 data sheet that describes different power consumption data.

|Power mode|Description|Tested power consumption|Power consumption in data sheet|Tested wake-up time|Wake-up time in data sheet|
|--|--|--|--|--| -- |
|Sleep|VDD_CORE=1.1V<br>CPU_CLK=96MHz|3.429mA|3.34mA|0.15us|N/A|
|Sleep|VDD_CORE=1.0V<br>CPU_CLK=48MHz|1.804mA|1.81mA|0.27us|0.23us|
|Sleep|VDD_CORE=1.0V<br>CPU_CLK=12MHz|0.446mA|0.43mA|1.086us|N/A|
|DeepSleep|VDD_CORE=1.1V<br>CPU_CLK=96MHz<br>FRO12M disabled|256.27uA|257.98uA|6.552us|N/A|
|DeepSleep|VDD_CORE=1.0V<br>CPU_CLK=48MHz<br>FRO12M disabled|31.07uA|32.26uA|7.284us|7.1us|
|DeepSleep|VDD_CORE=1.0V<br>CPU_CLK=48MHz<br>FRO12M enabled|107.13uA|104.53uA|7.281us|N/A|
|DeepSleep|VDD_CORE=1.0V<br>CPU_CLK=12MHz<br>FRO12M disabled|31.02uA|N/A|14.274us|N/A|
|PowerDown|VDD_CORE=1.1V<br>CPU_CLK=96MHz<br>All RAM retained|254.37uA|N/A|7.514us|N/A|
|PowerDown|VDD_CORE=retention voltage<br>CPU_CLK=48MHz<br>All RAM retained|10.58uA|9.47uA|16.848us|16.6us|
|PowerDown|VDD_CORE=retention voltage<br>CPU_CLK=48MHz<br>RAM X0/X1 and RAM A0 retained|9.28uA|8.20uA|16.848us|N/A|
|PowerDown|VDD_CORE=retention voltage<br>CPU_CLK=12MHz<br>All RAM retained|10.53uA|N/A|23.758us|N/A|
|DeepPowerDown|VDD_CORE=1.0V<br>CPU_CLK=48MHz<br>All RAM OFF<br>FRO16K enabled|0.65uA|0.60uA|1.44ms|1.44ms|
|DeepPowerDown|VDD_CORE=1.0V<br>CPU_CLK=48MHz<br>All RAM retained<br>FRO16K enabled|2.37uA|2.19uA|1.44ms|N/A|
|DeepPowerDown|VDD_CORE=1.0V<br>CPU_CLK=48MHz<br>RAM X0/X1 and A0-A3 retained<br>FRO16K enabled|1.71uA|1.57uA|1.44ms|N/A|
|DeepPowerDown|VDD_CORE=1.0V<br>CPU_CLK=48MHz<br>RAM X0/X1 and A0 retained<br>FRO16K enabled|1.02uA|0.93uA|1.44ms|N/A|
|DeepPowerDown|VDD_CORE=1.0V<br>CPU_CLK=48MHz<br>RAM A0 retained<br>FRO16K enabled|0.87uA|0.79uA|1.44ms|N/A|
|DeepPowerDown|VDD_CORE=1.0V<br>CPU_CLK=48MHz<br>RAM X0/X1 retained<br>FRO16K enabled|0.90uA|0.82uA|1.44ms|N/A|

## 5. FAQs<a name="step5"></a>
*No FAQs have been identified for this project.*

## 6. Support<a name="step6"></a>
*Please contact NXP for additional support.*

#### Project Metadata

<!----- Boards ----->
[![Board badge](https://img.shields.io/badge/Board-FRDM&ndash;MCXA156-blue)]()

<!----- Categories ----->
[![Category badge](https://img.shields.io/badge/Category-LOW%20POWER-yellowgreen)](https://github.com/search?q=org%3Anxp-appcodehub+low_power+in%3Areadme&type=Repositories)

<!----- Peripherals ----->
[![Peripheral badge](https://img.shields.io/badge/Peripheral-CLOCKS-yellow)](https://github.com/search?q=org%3Anxp-appcodehub+clocks+in%3Areadme&type=Repositories)

<!----- Toolchains ----->
[![Toolchain badge](https://img.shields.io/badge/Toolchain-MCUXPRESSO%20IDE-orange)](https://github.com/search?q=org%3Anxp-appcodehub+mcux+in%3Areadme&type=Repositories)

Questions regarding the content/correctness of this example can be entered as Issues within this GitHub repository.

>**Warning**: For more general technical questions regarding NXP Microcontrollers and the difference in expected functionality, enter your questions on the [NXP Community Forum](https://community.nxp.com/)

[![Follow us on Youtube](https://img.shields.io/badge/Youtube-Follow%20us%20on%20Youtube-red.svg)](https://www.youtube.com/NXP_Semiconductors)
[![Follow us on LinkedIn](https://img.shields.io/badge/LinkedIn-Follow%20us%20on%20LinkedIn-blue.svg)](https://www.linkedin.com/company/nxp-semiconductors)
[![Follow us on Facebook](https://img.shields.io/badge/Facebook-Follow%20us%20on%20Facebook-blue.svg)](https://www.facebook.com/nxpsemi/)
[![Follow us on Twitter](https://img.shields.io/badge/X-Follow%20us%20on%20X-black.svg)](https://x.com/NXP)

## 7. Release Notes<a name="step7"></a>
| Version | Description / Update                           | Date                        |
|:-------:|------------------------------------------------|----------------------------:|
| 1.0     | Initial release on Application Code Hub        | September 2<sup>nd</sup> 2024 |