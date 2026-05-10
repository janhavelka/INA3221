# INA3221 26V, Triple Channel, 13-Bit, I2C Output Current and Voltage Monitor With Alerts datasheet (Rev. C)

- Source PDF: `INA3221_datasheet.pdf`
- Extraction tool: pdfplumber
- Page count: 50
- SHA256: `01dd1c52a76756380c345a9dc8c2372399f6abfb09e1e52405b36f7f1d6567a3`

## Page 1

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
INA3221 26V, Triple Channel, 13-Bit, I2C Output Current and Voltage Monitor With
Alerts
1 Features 3 Description
• Senses bus voltages from 0V to 26V The INA3221 is a three-channel, high-side current
• Reports shunt and bus voltage and bus voltage monitor with an I2C- and SMBUS-
• High accuracy: compatible interface. The INA3221 monitors both
– Offset voltage: ±80μV (maximum) shunt voltage drops and bus supply voltages, in
– Gain error: 0.25% (maximum) addition to having programmable conversion times
• Configurable averaging options and averaging modes for these signals. The INA3221
• Four programmable addresses offers both critical and warning alerts to detect
• Programmable alert and warning outputs multiple programmable out-of-range conditions for
• Power-supply operation: 2.7V to 5.5V each channel.
2 Applications The INA3221 senses current on buses that can vary
from 0V to 26V. The device is powered from a
• Computers single 2.7V to 5.5V supply, and draws 350μA (typ)
• Power management of supply current. The INA3221 is specified over the
• Telecom equipment operating temperature range of –40°C to +125°C. The
• Battery chargers I2C- and SMBUS-compatible interface features four
• Power supplies programmable addresses.
• Test equipment
Package Information
• Data Centers
PART NUMBER PACKAGE(1) PACKAGE SIZE(2)
INA3221 RGV (VQFN, 16) 4.00mm × 4.00mm
(1) For all available packages, see the package option
addendum at the end of the data sheet.
(2) The package size (length × width) is a nominal value and
includes pins, where applicable.
Power Supply
(0V to 26V) CBYPASS
0.1μF
Load 1
VS (Supply
VIN+1 VIN–1 Voltage) 10kΩ
Power Supply
(0V to 26V) SDA
I2C-
CH 1 and SCL
Bus SMBus-
VIN+2 CH 2 Volt
S
a
h
g
u
e
n
s
t
1-3 C
I
o
n
m
te
p
rf
a
a
t
c
ib
e
le A0
VPU VS
ADC
VIN–2 Voltages 1-3
Critical Limit
10kΩ
Alerts 1-3 VPU
Power Valid (PV)
Load 2 CH 3 Shunt Voltage Critical
Sum Alerts
Warning
Timing Control (TC)
GND
VIN+3 VIN–3
Power Supply
(0V to 26V) Load 3
Typical Application
An IMPORTANT NOTICE at the end of this data sheet addresses availability, warranty, changes, use in safety-critical applications,
intellectual property matters and other important disclaimers. PRODUCTION DATA.

## Page 2

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
Table of Contents
1 Features............................................................................1 7.6 Register Maps...........................................................24
2 Applications..................................................................... 1 8 Application and Implementation.................................. 37
3 Description.......................................................................1 8.1 Application Information............................................. 37
4 Device Comparison Table...............................................3 8.2 Typical Application.................................................... 37
5 Pin Configuration and Functions...................................3 8.3 Power Supply Recommendations.............................38
6 Specifications.................................................................. 4 8.4 Layout....................................................................... 39
6.1 Absolute Maximum Ratings........................................ 4 9 Device and Documentation Support............................40
6.2 ESD Ratings............................................................... 4 9.1 Device Support......................................................... 40
6.3 Recommended Operating Conditions.........................4 9.2 Documentation Support............................................ 40
6.4 Thermal Information....................................................4 9.3 Receiving Notification of Documentation Updates....40
6.5 Electrical Characteristics.............................................5 9.4 Support Resources................................................... 40
6.6 Typical Characteristics................................................ 7 9.5 Trademarks...............................................................40
7 Detailed Description......................................................10 9.6 Electrostatic Discharge Caution................................40
7.1 Overview................................................................... 10 9.7 Glossary....................................................................40
7.2 Functional Block Diagram......................................... 10 10 Revision History.......................................................... 40
7.3 Feature Description...................................................11 11 Mechanical, Packaging, and Orderable
7.4 Device Functional Modes..........................................16 Information.................................................................... 41
7.5 Programming............................................................ 20
2 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 3

4 Device Comparison Table
DEVICE DESCRIPTION
INA226 36V, Bidirectional, Ultrahigh Accuracy, Low- or High-Side, I2C Out, Current and Power Monitor With Alert
INA219 26V, Bidirectional, Zero-Drift, High-Side, I2C Out, Current and Power Monitor
INA209 26V, Bidirectional, Low- or High-Side, I2C Out, Current and Power Monitor and High-Speed Comparator
INA210, INA211, INA212,
26V, Bidirectional, Zero-Drift, High-Accuracy, Low- or High-Side, Voltage Out, Current Shunt Monitor
INA213, INA214
5 Pin Configuration and Functions
IN(cid:16)3 1 12 IN+1
IN+3 2 11 IN(cid:16)1
GND 3 10 PV
VS 4 9 Critical
UPV
61
5
0A
2+NI
51
6
LCS
(cid:16)
2
NI
41
7
ADS
CT
31
8
gninraW
INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
Figure 5-1. RGV Package 16-Pin VQFN Top View
Table 5-1. Pin Functions
PIN
Type DESCRIPTION
NAME NO.
Address pin. Connect to GND, SCL, SDA, or V . Table 7-1 shows pin settings and
A0 5 Digital input S
corresponding addresses.
Critical 9 Digital output Conversion-triggered critical alert; open-drain output.
GND 3 Analog Ground
Connect to load side of the channel 1 shunt resistor. Bus voltage is the measurement
IN–1 11 Analog input
from this pin to ground.
IN+1 12 Analog input Connect to supply side of the channel 1 shunt resistor.
Connect to load side of the channel 2 shunt resistor. Bus voltage is the measurement
IN–2 14 Analog input
from this pin to ground.
IN+2 15 Analog input Connect to supply side of the channel 2 shunt resistor.
Connect to load side of the channel 3 shunt resistor. Bus voltage is the measurement
IN–3 1 Analog input
from this pin to ground.
IN+3 2 Analog input Connect to supply side of the channel 3 shunt resistor.
PV 10 Digital output Power valid alert; open-drain output.
SCL 6 Digital input Serial bus clock line; open-drain input.
SDA 7 Digital I/O Serial bus data line; open-drain input/output.
TC 13 Digital output Timing control alert; open-drain output.
VPU 16 Analog input Pull-up supply voltage used to bias power valid output circuitry.
VS 4 Analog Power supply, 2.7V to 5.5V.
Warning 8 Digital output Averaged measurement warning alert; open-drain output.
Thermal - - This pad must be connected to ground or left floating.
Pad
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 3
Product Folder Links: INA3221

## Page 4

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
6 Specifications
6.1 Absolute Maximum Ratings
over operating free-air temperature range (unless otherwise noted)(1)
MIN MAX UNIT
Voltage Supply, V 6 V
S
Differential (V ) – (V )(2) –26 26
IN+ IN–
IN+, IN–
Analog inputs Common-mode (V ) + (V ) / 2 –0.3 26 V
IN+ IN–
VPU 26
Critical, warning, power valid 6
Digital outputs V
Timing control 26
Data line, SDA (GND – 0.3) 6
Serial bus V
Clock line, SCL (GND – 0.3) (V + 0.3)
S
Input, into any pin 5
Current mA
Open-drain, digital output 10
Operating, T –40 125
A
Temperature Junction, T 150 °C
J
Storage, T –65 150
stg
(1) Stresses beyond those listed under Absolute Maximum Ratings may cause permanent damage to the device. These are stress
ratings only, which do not imply functional operation of the device at these or any other conditions beyond those indicated under
Recommended Operating Conditions. Exposure to absolute-maximum-rated conditions for extended periods may affect device
reliability.
(2) V and V can have a differential voltage of –26V to +26V; however, the voltage at these pins must not exceed the range of
IN+ IN–
–0.3V to +26V.
6.2 ESD Ratings
VALUE UNIT
Human-body model (HBM), per ANSI/ESDA/JEDEC JS-001(1) ±2500
V
V Electrostatic discharge Charged-device model (CDM), per JEDEC specification JESD22-C101(2) ±1000
(ESD)
Machine model ±200
(1) JEDEC document JEP155 states that 500V HBM allows safe manufacturing with a standard ESD control process.
(2) JEDEC document JEP157 states that 250V CDM allows safe manufacturing with a standard ESD control process.
6.3 Recommended Operating Conditions
over operating free-air temperature range (unless otherwise noted)
MIN NOM MAX UNIT
Operating supply voltage 2.7 5.5 V
Operating temperature, T –40 125 °C
A
6.4 Thermal Information
INA3221
THERMAL METRIC(1) RGV (VQFN) UNIT
16 PINS
R Junction-to-ambient thermal resistance 36.5 °C/W
θJA
R Junction-to-case (top) thermal resistance 42.7 °C/W
θJC(top)
R Junction-to-board thermal resistance 14.7 °C/W
θJB
ψ Junction-to-top characterization parameter 0.5 °C/W
JT
ψ Junction-to-board characterization parameter 14.8 °C/W
JB
4 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 5

INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
INA3221
THERMAL METRIC(1) RGV (VQFN) UNIT
16 PINS
R Junction-to-case (bottom) thermal resistance 3.3 °C/W
θJC(bot)
(1) For more information about traditional and new thermal metrics, see the Semiconductor and IC Package Thermal Metrics application
report (SPRA953).
6.5 Electrical Characteristics
at T = 25°C, V = 3.3V, V = 12V, V = (V ) – (V ) = 0mV, and V = V = 12V (unless otherwise noted)
A S IN+ SHUNT IN+ IN– BUS IN–
PARAMETER TEST CONDITIONS MIN TYP MAX UNIT
INPUT
VSHUNT Shunt voltage input –163.84 163.8 mV
VBUS Bus voltage input 0 26 V
CMR Common-mode rejection VIN+ = 0V to +26V 110 120 dB
±40 ±80 μV
VOS
Shunt offset voltage, RTI(1) TA = –40°C to +125°C 0.1 0.5 μV/°C
PSRR vs power supply, VS = 2.7V to 5.5V 15 μV/V
±8 ±16 mV
VOS
Bus offset voltage, RTI(1) TA = –40°C to +125°C 80 μV/°C
PSRR vs power supply 0.5 mV/V
IIN+ Input bias current at IN+ 10 μA
IIN– Input bias current at IN– 10 || 670 μA || kΩ
Input leakage(2) (IN+ pin) + (IN– pin), power-down mode 0.1 0.5 μA
DC ACCURACY
ADC native resolution 13 Bits
Shunt voltage 40 μV
1LSB step size
Bus voltage 8 mV
0.1% 0.25%
Shunt voltage gain error
TA = –40°C to +125°C 10 50 ppm/°C
0.1% 0.25%
Bus voltage gain error
TA = –40°C to +125°C 10 50 ppm/°C
DNL Differential nonlinearity ±0.1 LSB
CT bit = 000 140 154
CT bit = 001 204 224
μs
CT bit = 010 332 365
CT bit = 011 588 646
tCONVERT ADC conversion time
CT bit = 100 1.1 1.21
CT bit = 101 2.116 2.328
ms
CT bit = 110 4.156 4.572
CT bit = 111 8.244 9.068
SMBus
SMBus timeout(3) 28 35 ms
DIGITAL INPUT/OUTPUT
CI Input capacitance 3 pF
Leakage input current 0V ≤ VIN ≤ VS 0.1 1 μA
VIH High-level input voltage 0.7 (VS) 6 V
VIL Low-level input voltage –0.5 0.3 (VS) V
Low-level output SDA, critical, warning, PV VS > +2.7V, IOL = 3mA 0 0.4
VOL
voltage TC VS > +2.7V, IOL = 1.2mA 0 0.4
V
Vhys Hysteresis voltage 500 mV
POWER SUPPLY
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 5
Product Folder Links: INA3221

## Page 6

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
at T = 25°C, V = 3.3V, V = 12V, V = (V ) – (V ) = 0mV, and V = V = 12V (unless otherwise noted)
A S IN+ SHUNT IN+ IN– BUS IN–
PARAMETER TEST CONDITIONS MIN TYP MAX UNIT
350 450
Quiescent current μA
Power-down mode 0.5 2
Power-on reset threshold 2 V
(1) RTI = Referred-to-input.
(2) Input leakage is positive (current flows into the pin) for the conditions shown at the top of this table. Negative leakage currents can
occur under different input conditions.
(3) SMBus timeouts in the INA3221 reset the interface whenever SCL is low for more than 28ms.
6 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 7

6.6 Typical Characteristics
at T = 25°C, V = 3.3V,V = 12V, V = (V ) – (V ) = 0mV, and V = V = 12V (unless otherwise noted)
A S IN+ SHUNT IN+ IN– BUS IN–
0
−10
−20
n
ai n ( d
B)
−30 P o p ul
ati o
G
−40
−50
−60 0 0 0 0 0 0 0 0 0 0
1 10 100 1k 10k 100k 6 2 8 4 4 8 2 6 0
1 1 − − 1 1 2
Frequency (Hz) − − Input Offset Voltage (μV)
G001
G003
Figure 6-1. Frequency Response Figure 6-2. Shunt Input Offset Voltage Production Distribution
50
45
40
35
30
−50 −25 0 25 50 75 100 125 150
Temperature (°C)
)Vμ(
egatloV
tesffO
tupnI
130
125
120
115
−50 −25 0 25 50 75 100 125 150
Temperature (°C)
G004
Figure 6-3. Shunt Input Offset Voltage vs. Temperature
)Bd(
noitcejeR
edoM−nommoC
G005
Figure 6-4. Shunt Input Common-Mode Rejection Ratio vs.
Temperature
400
350
300
o n 250 ati
p
ul
200
o
P
150
100
50
3 2 1 0 1 2 3 4 0
0. 0. 0. 0. 0. 0. 0. −50 −25 0 25 50 75 100 125 150
− − − Temperature (°C)
Input Gain Error (%) G006
Figure 6-5. Shunt Input Gain Error Production Distribution
)%m(
rorrE
niaG
tupnI
INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
G007
Figure 6-6. Shunt Input Gain Error vs. Temperature
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 7
Product Folder Links: INA3221

## Page 8

6.6 Typical Characteristics (continued)
at T = 25°C, V = 3.3V,V = 12V, V = (V ) – (V ) = 0mV, and V = V = 12V (unless otherwise noted)
A S IN+ SHUNT IN+ IN– BUS IN–
200
150
100
50
0 2 4 6 8 10 12 14 16 18 20 22 24 26
Common−Mode Input Voltage (V)
)%m(
rorrE
niaG
tupnI
n
o ati
ul
p o
P
2 4 6 8 0 8 6 4 2 4 3 2 1 − 1 2 3 3
− − −
G008 Input Offset Voltage (mV)
Figure 6-7. Shunt Input Gain Error vs. Common-Mode Voltage G009
Figure 6-8. Bus Input Offset Voltage Production Distribution
8
4
0
−4
−8
−12
−16
−50 −25 0 25 50 75 100 125 150
Temperature (°C)
)Vm(
egatloV
tesffO
tupnI
n
o
ati
ul p
o
P
−
0. 3
−
0. 2
−
0. 1 0 0. 1 0. 2 0. 3 0. 4
G010 Input Gain Error (%)
Figure 6-9. Bus Input Offset Voltage vs. Temperature G011
Figure 6-10. Bus Input Gain Error Production Distribution
400
350
300
250
200
150
100
50
0
−50 −25 0 25 50 75 100 125 150
Temperature (°C)
)%m(
rorrE
niaG
tupnI
50
45
40
35
30
25
20
15
10
5
0
0 4 8 12 16 20 24 28
Common−Mode Input Voltage (V)
G012
Figure 6-11. Bus Input Gain Error vs. Temperature
)Aμ(
tnerruC
saiB
tupnI
INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
IB−
IB+
G013
Figure 6-12. Input Bias Current vs. Common-Mode Voltage
8 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 9

6.6 Typical Characteristics (continued)
at T = 25°C, V = 3.3V,V = 12V, V = (V ) – (V ) = 0mV, and V = V = 12V (unless otherwise noted)
A S IN+ SHUNT IN+ IN– BUS IN–
30
25
20
15
10
5
0
−50 −25 0 25 50 75 100 125 150
Temperature (°C)
)Aμ(
tnerruC
saiB
tupnI
450
400
IB− 350
300
250
200
IB+
150
100
50
0
−50 −25 0 25 50 75 100 125 150
Temperature (°C)
G014
Figure 6-13. Input Bias Current vs. Temperature
)An(
tnerruC
saiB
tupnI
IB+, IB−
G015
Figure 6-14. Input Bias Current vs. Temperature (Shutdown)
500
450
400
350
300
250
200
−50 −25 0 25 50 75 100 125 150
Temperature (°C)
)Aμ(
tnerruC
tnecseiuQ
3.5
3
2.5
2
1.5
1
0.5
0
−50 −25 0 25 50 75 100 125 150
Temperature (°C)
G016
Figure 6-15. Active I vs. Temperature
Q
)Aμ(
tnerruC
tnecseiuQ
G017
Figure 6-16. Shutdown I vs. Temperature
Q
700
650
600
550
500
450
400
350
300
0.01 0.1 1 4
Frequency (MHz)
)Aμ(
tnerruC
tnecseiuQ
350
300
250
200
150
100
50
0
0.01 0.1 1 4
Frequency (MHz)
G018
Figure 6-17. Active I vs. I2C Clock Frequency
Q
)Aμ(
tnerruC
tnecseiuQ
INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
G019
Figure 6-18. Shutdown I vs. I2C Clock Frequency
Q
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 9
Product Folder Links: INA3221

## Page 10

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
7 Detailed Description
7.1 Overview
The INA3221 is a current-shunt and bus voltage monitor that communicates over an I2C- and SMBus-compatible
interface. The INA3221 provides digital shunt and bus voltage readings necessary for accurate decision making
in precisely-controlled systems, and also monitors multiple rails to maintain compliance voltages. Programmable
registers offer flexible configuration for measurement precision, and continuous versus single-shot operation.
The Register Maps section provides details of the INA3221 registers, beginning with Table 7-3.
7.2 Functional Block Diagram
Bus Voltage(1) X
Shu C n h t a V n o n l e ta l ge Channel 2 U P p o p w e e r r L V im a i l t i d (2 )
Channel 3
ADC
Bus Voltage
Channel Power Valid
Lower Limit(2)
Shunt Voltage(1) X
Critical Limit(2)
Warning Limit(2)
Channel 2 Summation(1)
Channel 3
Summation Limit(2)
A. Read-only.
B. Read/write.
10 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 11

INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
7.3 Feature Description
7.3.1 Basic ADC Functions
The INA3221 performs two measurements on up to three power supplies of interest. The voltage developed from
the load current passing through a shunt resistor creates a shunt voltage that is measured between the IN+ and
IN– pins. The device also internally measures the power-supply bus voltage at the IN– pin for each channel. The
differential shunt voltage is measured with respect to the IN– pin, and the bus voltage is measured with respect
to ground.
The INA3221 is typically powered by a separate power supply that ranges from 2.7V to 5.5V. The monitored
supply buses range from 0V to 26V.
CAUTION
Based on the fixed 8mV bus-voltage register LSB (for any channel), a full-scale register value results
in 32.76V. However, the actual voltage applied to the INA3221 input pins must not exceed 26V.
There are no special power-supply sequencing considerations between the common-mode input ranges and the
device power-supply voltage because each are independent of the other; therefore, the bus voltages can be
present with the supply voltage off and reciprocally.
The INA3221 takes two measurements for each channel: one for shunt voltage and one for bus voltage.
Each measurement can be independently or sequentially measured, based on the mode setting (bits 2-0 in
the Configuration register). When the INA3221 is in normal operating mode (that is, the MODE bits of the
Configuration register are set to 111), the device continuously converts a shunt-voltage reading followed by a
bus-voltage reading. This procedure converts one channel, and then continues to the shunt voltage reading of
the next enabled channel, followed by the bus-voltage reading for that channel, and so on, until all enabled
channels have been measured. The programmed Configuration register mode setting applies to all channels.
Any channels that are not enabled are bypassed in the measurement sequence, regardless of mode setting.
The INA3221 has two operating modes, continuous and single-shot, that determine the internal ADC operation
after these conversions complete. When the INA3221 is set to continuous mode (using the MODE bit settings),
the device continues to cycle through all enabled channels until a new configuration setting is programmed.
The Configuration register MODE control bits also enable modes to be selected that convert only the shunt or
bus voltage. This feature further allows the device to fit specific application requirements.
In single-shot (triggered) mode, setting any single-shot convert mode to the Configuration register (that is, the
Configuration register MODE bits set to 001, 010, or 011) triggers a single-shot conversion. This action produces
a single set of measurements for all enabled channels. To trigger another single-shot conversion, write to the
Configuration register a second time, even if the mode does not change. When a single-shot conversion is
initiated, all enabled channels are measured one time and then the device enters a power-down state. The
INA3221 registers can be read at any time, even while in power-down. The data present in these registers are
from the last completed conversion results for the corresponding register. The conversion ready flag bit (Mask/
Enable register, CVRF bit) helps coordinate single-shot conversions, and is especially helpful during longer
conversion time settings. The CVRF bit is set after all conversions are complete. The CVRF bit clears under the
following conditions:
1. Writing to the Configuration register, except when configuring the MODE bits for power-down mode; or
2. Reading the Mask/Enable register.
In addition to the two operating modes (continuous and single-shot), the INA3221 also has a separate selectable
power-down mode that reduces the quiescent current and turns off current into the INA3221 inputs. Power-down
mode reduces the impact of supply drain when the device is not used. Full recovery from power-down mode
requires 40μs. The INA3221 registers can be written to and read from while the device is in power-down mode.
The device remains in power-down mode until one of the active MODE settings are written to the Configuration
register.
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 11
Product Folder Links: INA3221

## Page 12

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
7.3.2 Alert Monitoring
The INA3221 allows programmable thresholds that make sure the intended application operates within the
desired operating conditions. Multiple monitoring functions are available using four alert pins: Critical, Warning,
PV (power valid), and TC (timing control). These alert pins are open-drain connections.
7.3.2.1 Critical Alert
The critical-alert feature monitors functions based on individual conversions of each shunt-voltage channel. The
critical-alert limit feature compares the shunt-voltage conversion for each shunt-voltage channel to the value
programmed into the corresponding limit register, to determine if the measured value exceeds the intended limit.
Exceeding the programmed limit indicates that the current through the shunt resistor is too high.
At power-up, the default critical-alert limit value for each channel is set to the positive full-scale value, effectively
disabling the alert. Program the corresponding limit registers at any time to begin monitoring for out-of-range
conditions. The Critical alert pin pulls low if any channel measurement exceeds the limit present in the
corresponding-channel critical-alert limit register. When the Critical alert pulls low, read the Mask/Enable register
to determine which channel caused the critical alert flag indicator bit (CF1-3) to assert (= 1).
7.3.2.1.1 Summation Control Function
The INA3221 also allows the Critical alert pin to be controlled by the summation control function. This function
adds the single shunt-voltage conversions for the desired channels (set by SCC1-3 in the Mask/Enable register)
to compare the combined sum to the programmed limit.
The SCC bits either disable the summation control function or allow the summation control function to switch
between including two or three channels in the Shunt-Voltage Sum register. The Shunt-Voltage Sum Limit
register contains the programmed value that is compared to the value in the Shunt-Voltage Sum register to
determine if the total summed limit is exceeded. If the shunt-voltage sum limit value is exceeded, the Critical
alert pin pulls low. Either the summation alert flag indicator bit (SF) or the individual critical alert limit bits (CF1-3)
in the Mask/Enable register determine the source of the alert when the Critical alert pin pulls low.
For the summation limit to have a meaningful value, use the same shunt-resistor value on all included channels.
Unless equal shunt-resistor values are used for each channel, do not use this function to add the individual
conversion values directly together in the Shunt-Voltage Sum register to report the total current.
7.3.2.2 Warning Alert
The warning alert monitors the averaged value of each shunt-voltage channel. The averaged value of each
shunt-voltage channel is based on the number of averages set with the averaging mode bits (AVG1-3) in the
Configuration register. The average value updates in the shunt-voltage output register each time there is a
conversion on the corresponding channel. The device compares the averaged value to the value programmed in
the corresponding-channel Warning Alert Limit register to determine if the averaged value has been exceeded,
indicating whether the average current is too high. At power-up, the default warning-alert limit value for each
channel is set to the positive full-scale value, effectively disabling the alert. The corresponding limit registers can
be programmed at any time to begin monitoring for out-of-range conditions. The Warning alert pin pulls low if
any channel measurements exceed the limit present in the corresponding-channel Warning Alert Limit register.
When the Warning alert pin pulls low, read the Mask/Enable register to determine which channel warning alert
flag indicator bit (WF1-3) is asserted (= 1).
12 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 13

INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
7.3.2.3 Power-Valid Alert
The power-valid alert verifies if all power rails are above the required levels. This feature manages power
sequencing, and validates the reported measurements based on system configuration. Power-valid mode starts
at power-up, and detects when each channel exceeds a 10V threshold. This 10V level is the default value
programmed into the Power-Valid Upper-Limit register. This value can be reprogrammed when the INA3221 is
powered up to a valid supply-voltage level of at least 2.7V. When all three bus-voltage measurements reach
the programmed value loaded to the Power-Valid Upper-Limit register, the power-valid (PV) alert pin pulls high.
PV powers up in a low state, and does not pull high until the power-valid conditions are met, indicating all
bus-voltage rails are above the power-valid upper-limit value. This sequence is shown in Figure 7-1.
All enabled channel bus voltages are All enabled channel bus voltages are
above the power-valid upper limit. above the power-valid upper limit.
High
Power
Valid
Output
Low
All enabled channel bus voltages are At least one bus voltage channel has
not above the power-valid upper limit. dropped below the power-valid lower limit.
Figure 7-1. Power-Valid State Diagram
When the power-valid conditions are met, and the PV pin pulls high, the INA3221 monitors if any bus-voltage
measurements drop below 9V. This 9V level is the default value programmed into the Power-Valid Lower-Limit
register. This value can also be reprogrammed when the INA3221 powers up to a supply voltage of at least
2.7V. If any bus-voltage measurement on the three channels drops below the Power-Valid Lower-Limit register
value, the PV pin goes low, indicating that the power-valid condition is no longer met. At this point, the INA3221
resumes monitoring the power rails for a power-valid condition set in the Power-Valid Upper-Limit register.
The power-valid alert function is based on the power-valid conditions requirement that all three channels reach
the intended Power-Valid Upper-Limit register value. If all three channels are not used, connect the unused-
channel IN– pin externally to one of the used channels to use the power-valid alert function. If the unused
channel is not connected to a valid rail, the power-valid alert function cannot detect if all three channels reach
the power-valid level. Float the unused channel IN+ pin.
The power-valid function also requires that bus-voltage measurements are monitored. To detect changes in
the power-valid state, enable bus-voltage measurements through one of the corresponding MODE-bit settings
in the Configuration register. The single-shot bus-voltage mode periodically cycles between the bus-voltage
measurements to make sure that the power-valid conditions are met.
When all three bus-voltage measurements are completed, the device compares the results to the power-valid
threshold values to determine the power-valid state. The bus-voltage measurement values remain in the
corresponding channel output registers until the bus-voltage measurements are taken again, thus updating
the output registers. When the output registers are updated, the values are again compared to the power-valid
thresholds. Without taking periodic bus-voltage measurements, the INA3221 is unable to determine if the power-
valid conditions are maintained.
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 13
Product Folder Links: INA3221

## Page 14

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
The PV pin allows for a 0V output that indicates a power-invalid condition. An output equal to the pull-up supply
voltage connected to the VPU pin indicates a power-valid condition, as shown in Figure 7-2. Dividing down the
high power-valid pull-up voltage by adding a resistor to ground at the PV output is also possible, thus allowing
this function to interface with lower-voltage circuitry, if needed.
VS
INA3221
VPU
RPU_ext
PV
RPU
Power-Valid RDIV (1)
Detection
A. R can be used to level-shift the PV output high.
DIV
Figure 7-2. Power-Valid Output Structure
7.3.2.4 Timing-Control Alert
The INA3221 timing-control alert function helps verify proper power-supply sequencing. At power-up, the default
INA3221 setting is continuous shunt- and bus-voltage conversion mode, and the INA3221 internally begins
comparing the channel-1 bus voltage to determine when a 1.2V level is reached. This comparison is made each
time the sequence returns to the channel-1 bus-voltage measurement. When a 1.2V level is detected on the
channel-1 bus-voltage measurement, the INA3221 begins checking for a 1.2V level present on the channel-2
bus-voltage measurement. After a 1.2V level is detected on channel 1, if the INA3221 does not detect a 1.2V
value or greater on the bus voltage measurement following four complete cycles of all three channels, the timing
control (TC) alert pin pulls low to indicate that the INA3221 has not detected a valid power rail on channel 2. As
shown in Figure 7-3, this sequence allows for approximately 28.6ms from the time 1.2V is detected on channel 1
for a valid voltage to be detected on channel 2. Figure 7-4 illustrates the state diagram for the TC alert pin.
1.2V Detected on Channel-1 Measure For 1.2V on
Bus-Voltage Measurement Channel-2 Bus Voltage
Signal SB SB SB SB SB SB SB SB SB SB SB SB SB SB SB
Channel Ch Ch Ch Ch Ch Ch Ch Ch Ch Ch Ch Ch Ch Ch Ch
1 2 3 1 2 3 1 2 3 1 2 3 1 2 3
2.2ms
28.6ms
NOTE: The signal refers to the corresponding shunt (S) and bus (B) voltage measurement for each channel.
Figure 7-3. Timing Control Timing Diagram
1.2V Detected on Channel-1 Measure for 1.2V
Bus-Voltage Measurement on Channel-2 Bus Voltage
1.2V on Channel 2 Detected
High
Timing
Control
Output 28.6ms
1.2V on Channel 2 Not Detected
Low
Figure 7-4. Timing Control State Diagram
14 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 15

INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
The timing control alert function is only monitored at power-up or when a software reset is issued by setting
the reset bit (RST, bit 15) in the Configuration register. The timing control alert function timing is based on the
default device settings at power-up. Writing to the Configuration register before the timing control alert function
completes the full sequence results in disabling the timing control alert until power is cycled or a software reset is
issued.
7.3.2.5 Default Settings
The default register power-up states are listed in the Register Maps section. These registers are volatile; if
programmed to a value other than the default values shown in Table 7-3, the registers must be reprogrammed
every time the device powers up.
7.3.3 Software Reset
The INA3221 features a software reset that reinitializes the device and register settings to default power-up
values without having to cycle power to the device. Use bit 15 (RST) of the Configuration register to perform a
software reset. Setting RST reinitializes all registers and settings to the default power state with the exception of
the power-valid output state.
If a software reset is issued, the INA3221 holds the output of the PV pin until the power-valid detection sequence
completes. The Power-Valid Upper Limit and Power-Valid Lower limit registers return to the default state when
the software reset has been issued. Therefore, any reprogrammed limit registers are reset, resulting in the
original power-valid thresholds validating the power-valid conditions. This architecture prevents interruption to
circuitry connected to the power valid output during a software reset event.
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 15
Product Folder Links: INA3221

## Page 16

7.4 Device Functional Modes
7.4.1 Averaging Function
The INA3221 includes three channels to monitor up to three independent supply buses; however, multichannel
monitoring sometimes results in poor shunt-resistor placement. Ideally, shunt resistors are placed as close as
possible to the corresponding channel input pins. However, because of system layout and multiple power-supply
rails, one or more shunt resistors can require being located further away, thus presenting potentially larger
measurement errors. These errors result from additional trace inductance and other parasitic impedances
between the shunt resistor and input pins. Longer traces also create an additional potential for coupling noise
into the signal if the traces are routed near noise-generating sections of the board.
The INA3221 averaging function mitigates this potential problem by limiting the impact that any single
measurement has on the averaged value of each measured signal. This limitation reduces the influence that
noise has on the averaged value, thereby effectively creating an input-signal filter.
The averaging function is illustrated in Figure 7-5. Operation begins by first measuring the shunt input signal
on channel 1. This value is then subtracted from the previous value that was present in the corresponding data
output register. This difference is then divided by the value programmed by the averaging mode setting (AVG2-0,
Configuration register bits 11-9) and stored in an internal accumulation register. The computed result is then
added to the previously-loaded data output register value, and the resulting value is loaded to the corresponding
data output register. After the update, the next signal to be measured follows the same process. The larger
the value selected for the averaging mode setting, the less impact or influence any new conversion has on the
average value, as shown in Figure 7-6. This averaging feature functions as a filter to reduce input noise from the
averaged measurement value.
+ +
New (cid:8) ÷ (cid:8) Output
Sample AVG # Register
±
+
Figure 7-5. Averaging Function Block Diagram
26
25
24
23
22
21
20
1000 2000 3000 4000 5000 6000 7000
Samples
)Vm(
edutilpmA
INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
1 Average
16 Averages
1024 Averages
G020
Figure 7-6. Average Setting Example
16 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 17

INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
7.4.2 Multiple Channel Monitoring
The INA3221 monitors shunt and voltage measurements for up to three unique power-supply rails, and
measures up to six different signals. Adjust the number of channels and signals being measured by setting
the channel enable (CH1 to CH3 ) and mode (MODE3-1) bits in the Configuration register. This adjustment
en en
allows the device to be optimized based on application requirements for the system in use.
7.4.2.1 Channel Configuration
If all three channels must be monitored at power-up, but only one channel must be monitored after the system
has stabilized, disable the other two channels after power-up. This configuration allows the INA3221 to only
monitor the power-supply rail of interest. Disable unused channels to help improve system response time by
more quickly returning to sampling the channel of interest. The INA3221 linearly monitors the enabled channels.
That is, if all three channels are enabled for both shunt- and bus-voltage measurements, an additional five
conversions complete after a signal is measured before the device returns to that particular signal to begin
another conversion. To reduce this requirement down to two conversions before the device begins a new
conversion on a particular channel again, change the operating mode to monitor only the shunt voltage.
A timing aspect is also involved in reducing the measured signals. The amount of time to complete an all-
channel, shunt- and bus-voltage sequence is equal to the sum of the shunt-voltage conversion time and the
bus-voltage conversion time (programmed by the CT bits in the Configuration register) multiplied by the three
channels. The conversion times for the shunt- and bus-voltage measurements are programmed independently;
however, the selected shunt- and bus-voltage conversion times apply to all channels.
Enable a single channel with only one signal measured to allow for that particular signal to be monitored solely.
This setting enables the fastest response over time to changes in that specific input signal because there is no
delay from the end of one conversion before the next conversion begins on that channel. Conversion time is not
affected by enabling or disabling other channels. Selecting both the shunt- and bus-voltage settings, as well as
enabling additional channels, extends the time from the end of one conversion on a signal before the beginning
of the next conversion of that signal.
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 17
Product Folder Links: INA3221

## Page 18

7.4.2.2 Averaging and Conversion-Time Considerations
The INA3221 has programmable conversion times for both the shunt- and bus-voltage measurements. The
selectable conversion times for these measurements range from 140μs to 8.244ms. The conversion-time
settings, along with the programmable-averaging mode, enable the INA3221 to optimize available timing
requirements in a given application. For example, if a system requires data to be read every 2ms with all
three channels monitored, configure the INA3221 with the conversion times for the shunt- and bus-voltage
measurements set to 332μs.
The INA3221 can also be configured with a different conversion-time setting for the shunt- and bus-voltage
measurements. This approach is common in applications where the bus voltage tends to be relatively stable,
and allows for the time focused on the bus voltage measurement to be reduced relative to the shunt-voltage
measurement. For example, the shunt-voltage conversion time can be set to 4.156ms with the bus-voltage
conversion time set to 588μs for a 5ms update time.
There are trade-offs associated with the conversion-time and averaging-mode settings. The averaging feature
significantly improves the measurement accuracy by effectively filtering the signal. This approach allows the
INA3221 to reduce the amount of noise in the measurement caused by noise coupling into the signal. A greater
number of averages allows the INA3221 to be more effective in reducing the measurement noise component.
The trade-off to this noise reduction is that the averaged value has a longer response time to input-signal
changes. This aspect of the averaging feature is mitigated to some extent with the critical-alert feature that
compares each single conversion to determine if a measured signal (with noise component) has exceeded the
maximum acceptable level.
The selected conversion times also have an impact on measurement accuracy. This effect can seen in
Figure 7-7. The multiple conversion times shown in Figure 7-7 illustrate the impact of noise on measurement.
These curves shown do not use averaging. to achieve the highest-accuracy measurement possible, use a
combination of the longest allowable conversion times and highest number of averages, based on system timing
requirements.
120
Conversion Time: 140μs
80
40
Conversion Time: 332μs
0
−40
Conversion Time: 1.1ms
−80
−120
0 200 400 600 800 1000
)Vμ(
egatloV
INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
Number of Conversions
Figure 7-7. Noise Versus Conversion Time
18 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 19

7.4.3 Filtering and Input Considerations
Measuring current is often noisy, and such noise can be difficult to define. The INA3221 offers several
filtering options by allowing conversion times and the number of averages to be selected independently in
the Configuration register. The conversion times can be set independently for the shunt- and bus-voltage
measurements as well, for added flexibility in configuring power-supply bus monitoring.
The internal ADC is based on a delta-sigma (ΔΣ) front-end with a 500kHz (±30%) typical sampling rate. This
architecture has good inherent noise rejection; however, transients that occur at or very close to the sampling-
rate harmonics can cause problems. These transient signals are at 1MHz and higher; therefore, the signals are
managed by incorporating filtering at the INA3221 input. High-frequency signals allow for the use of low-value
series resistors on the filter, with negligible effects on measurement accuracy. In general, filtering the INA3221
input is only necessary if there are transients at exact harmonics of the 500kHz (±30%) sampling rate that
are greater than 1MHz. Filter using the lowest-possible series resistance (typically 10Ω or less) and a ceramic
capacitor. Recommended capacitor values are 0.1μF to 1.0μF. Figure 7-8 shows the INA3221 with an additional
filter added at the input.
Power Supply
(0V to 26V)
RFILTER Ch 1
≤ 10
Ch 2
VIN+2
ADC
VIN(cid:1)2
Ch 3
Load 2
(cid:1)
CFILTER: 0.1-
(cid:2)
F to 1-
(cid:0)
RFILTER
≤ 10
F
Ceramic Capacitor
(cid:3)
INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
CFILTER
Figure 7-8. INA3221 With Input Filtering
The INA3221 inputs are specified to tolerate 26V across the inputs. However, overload conditions are another
consideration for the INA3221 inputs. For example, a large differential-input scenario can be a short to ground
on the load side of the shunt. This type of event results in the full power-supply voltage applied across the
shunt, if supported by the power supply or energy-storage capacitors. Keep in mind that removing a short to
ground can result in inductive kickbacks that can exceed the 26V differential and common-mode rating of the
INA3221. Inductive kickback voltages are best controlled by zener-type transient-absorbing devices (commonly
called transzorbs) combined with sufficient energy-storage capacitance.
In applications that do not have large energy-storage electrolytic capacitors on one or both sides of the shunt, an
input overstress condition can result from an excessive dV/dt of the voltage applied to the input. A hard physical
short is the most likely cause of this event, particularly in applications without large electrolytic capacitors
present. This problem occurs because an excessive dV/dt can activate the INA3221 ESD protection in systems
where large currents are available. Testing has demonstrated that the addition of 10Ω resistors in series with
each INA3221 input sufficiently protects the inputs against this dV/dt failure up to the 26V device rating.
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 19
Product Folder Links: INA3221

## Page 20

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
7.5 Programming
7.5.1 Bus Overview
The INA3221 offers compatibility with both I2C and SMBus interfaces. The I2C and SMBus protocols are
essentially compatible with one another.
The I2C interface is used throughout this data sheet as the primary example, with the SMBus protocol specified
only when a difference between the two systems is discussed. Two I/O lines, the serial clock (SCL) and data
signal line (SDA), connect the INA3221 to the bus. Both SCL and SDA are open-drain connections.
The device that initiates a data transfer is called a controller, and the devices controlled by the controller are
targets. The bus must be controlled by the controller device that generates the SCL, controls the bus access,
and generates start and stop conditions.
To address a specific device, the controller initiates a start condition by pulling SDA from a high to a low logic
level while SCL is high. All targets on the bus shift in the target address byte on the SCL rising edge, with the
last bit indicating whether a read or write operation is intended. During the ninth clock pulse, the target being
addressed responds to the controller by generating an acknowledge bit and pulling SDA low.
Data transfer is then initiated and eight bits of data are sent, followed by an acknowledge bit. During data
transfer, SDA must remain stable while SCL is high. Any change in SDA while SCL is high is interpreted as a
start or stop condition.
After all data are transferred, the controller generates a stop condition by pulling SDA from low to high while SCL
is high. The INA3221 includes a 28ms timeout on the interface to prevent locking up the bus.
7.5.1.1 Serial Bus Address
To communicate with the INA3221, the controller must first address target devices with a target address byte.
This byte consists of seven address bits and a direction bit to indicate whether the intended action is a read or
write operation.
The INA3221 has one address pin, A0. Table 7-1 describes the pin logic levels for each of the four possible
addresses. The state of the A0 pin is sampled on every bus communication and must be set before any activity
on the interface occurs.
Table 7-1. Address Pins and Target Addresses
A0 TARGET ADDRESS
GND 1000000
VS 1000001
SDA 1000010
SCL 1000011
7.5.1.2 Serial Interface
The INA3221 only operates as a target device on the I2C bus and SMBus. Bus connections are made using
the open-drain I/O lines, SDA and SCL. The SDA and SCL pins feature integrated spike-suppression filters
and Schmitt triggers to minimize the effects of input spikes and bus noise. While there is spike suppression
integrated into the digital I/O lines, use proper layout to minimize the amount of coupling into the communication
lines. Noise introduction occurs from capacitively coupling signal edges between the two communication lines
themselves, or from other switching noise sources present in the system. Routing traces in parallel with
ground between layers on a printed circuit board (PCB) typically reduces the effects of coupling between the
communication lines. Shield communication lines to reduce the possibility of unintended noise coupling into the
digital I/O lines that can be incorrectly interpreted as start or stop commands.
The INA3221 supports a transmission protocol for Fast (1kHz to 400kHz) and High-speed (1kHz to 2.44MHz)
modes. All data bytes are transmitted MSB first.
20 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 21

INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
7.5.2 Writing To and Reading From the INA3221
To access a specific INA3221 register, write the appropriate value to the register pointer. See Table 7-3 for a
complete list of registers and corresponding addresses. The value for the register pointer, as shown in Figure
7-9, is the first byte transferred after the target address byte with the R/ W bit low. Every write operation to the
INA3221 requires a register pointer value.
1 9 1 9
SCL
SDA 1 0 0 0 0 0 A0 R/W D7 D6 D5 D4 D3 D2 D1 D0
Start By ACK By ACK By Stop By
Controller Device Device Controller
Frame 1: Two-Wire Target Address Byte(1) Frame 2: Register Pointer Byte
A. The value of the Target Address Byte is determined by the A0 pin setting; see Table 7-1.
Figure 7-9. Typical Register Pointer Set
Register writes begin with the first byte transmitted by the controller. This byte is the target address, with the
R/ W bit low. The INA3221 then acknowledges receipt of a valid address. The next byte transmitted by the
controller is the register address that data are written to. This register address value updates the register pointer
to the desired register. The next two bytes are written to the register addressed by the register pointer. The
INA3221 acknowledges receipt of each data byte. The controller terminates data transfer by generating a start or
stop condition.
When reading from the INA3221, the last value stored in the register pointer by a write operation determines
which register is read during a read operation. To change the register pointer for a read operation, write a
new value to the register pointer. This write is accomplished by issuing a target address byte with the R/ W
bit low, followed by the register pointer byte. No additional data are required. The controller then generates a
start condition and sends the target address byte with the R/ W bit high to initiate the read command. The next
byte is transmitted by the target and is the most significant byte of the register indicated by the register pointer.
This byte is followed by an acknowledge from the controller; then the target transmits the least significant byte.
The controller acknowledges receipt of the data byte. The controller terminates data transfer by generating a
not-acknowledge after receiving any data byte, or generating a start or stop condition. If repeated reads from the
same register are desired, it is not necessary to continually send the register pointer bytes; the INA3221 retains
the register pointer value until the value is changed by the next write operation.
Figure 7-10 and Figure 7-11 show the write and read operation timing diagrams, respectively. Note that register
bytes are sent most-significant byte first, followed by the least significant byte.
1 9 1 9 1 9
SCL
SDA 1 0 0 0 0 0 A0 R/W D15 D14 D13 D12 D11 D10 D9 D8 D7 D6 D5 D4 D3 D2 D1 D0
Start By ACK By ACK By ACK By Stop By
Controller Device Device Device Controller
Frame 1: Two-Wire Target Address Byte (1) Frame 2: Data MSByte Frame 3: Data LSByte
A. The value of the target address byte is determined by the A0 pin setting; see Table 7-1.
Figure 7-10. Timing Diagram for Write Word Format
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 21
Product Folder Links: INA3221

## Page 22

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
1 9 1 9 1 9
SCL
SDA 1 0 0 0 0 0 A0 R/W D15 D14 D13 D12 D11 D10 D9 D8 D7 D6 D5 D4 D3 D2 D1 D0
Start By ACK By From ACK By From No ACK By Stop By
Controller Device Device Controller Device Controller(3) Controller
Frame 1: Two-Wire Target Address Byte(1) Frame 2: Data MSByte(2) Frame 3: Data LSByte(2)
A. The value of the target address byte is determined by the A0 pin setting; see Table 7-1.
B. Read data are from the last register pointer location. If a new register is desired, the register pointer must be updated. See Figure 7-9.
C. The controller can also send an ACK.
Figure 7-11. Timing Diagram for Read Word Format
Figure 7-12 shows the timing diagram for the SMBus Alert response operation.
1 9 1 9
SCL
SDA 0 0 0 1 1 0 0 R/W 1 0 0 0 0 0 A0 0
Start By ACK By From No ACK By Stop By
Controller Device Device Controller Controller
Frame 1: SMBus ALERT Response Address Byte Frame 2: Target Address Byte(1)
A. The value of the Target Address Byte is determined by the A0 pin setting; see Table 7-1.
Figure 7-12. Timing Diagram for SMBus Alert
7.5.2.1 High-Speed I2C Mode
When the bus is idle, the SDA and SCL lines are pulled high by the pull-up resistors. The controller generates
a start condition followed by a valid serial byte with the high-speed (Hs) controller code 00001XXX. This
transmission is made in fast (400kHz) or standard (100kHz) (F/S) mode at no more than 400kHz. The INA3221
does not acknowledge the Hs controller code, but does recognize the code and switches the internal filters to
support 2.44MHz operation.
The controller then generates a repeated start condition (a repeated start condition has the same timing as
the start condition). After this repeated start condition, the protocol is the same as F/S mode, except that
transmission speeds up to 2.44MHz are allowed. Instead of using a stop condition, the controller uses a
repeated start conditions to secure the bus in Hs mode. A stop condition ends the Hs mode, and switches all
internal INA3221 filters to support F/S mode.
Figure 7-13 shows the bus timing, and Table 7-2 lists the bus timing definitions.
22 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 23

INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
t(LOW)
tr tfCL
SCL
t(HDSTA) t(HIGH) t(SUSTA) t(SUSTO)
t(HDDAT) t(HDSTA)
t(VDDAT) t(SUDAT) tfDA
SDA
t(BUF)
P S S P
Figure 7-13. Bus Timing
Table 7-2. Bus Timing Definitions (1)
FAST MODE HIGH-SPEED MODE
PARAMETER MIN MAX MIN MAX UNIT
f SCL operating frequency 0.001 0.4 0.001 2.44 MHz
(SCL)
t Bus free time between stop and start conditions 1300 160 ns
(BUF)
Hold time after repeated START condition.
t 600 160 ns
(HDSTA) After this period, the first clock is generated.
t Repeated start condition setup time 600 160 ns
(SUSTA)
t STOP condition setup time 600 160 ns
(SUSTO)
t Data hold time 0 0 ns
(HDDAT)
t Data valid time 1200 260 ns
(VDDAT)
t Data setup time 100 10 ns
(SUDAT)
t SCL clock low period 1300 270 ns
(LOW)
t SCL clock high period 600 60 ns
(HIGH)
t Data fall time 500 150 ns
fDA
t Clock fall time 300 40 ns
fCL
Clock rise time 300 40 ns
t
r
Clock rise time for SCLK ≤ 100kHz 1000 ns
(1) Values based on a statistical analysis of a one-time sample of devices. Minimum and maximum values are not production tested.
A0 = A1 = 0.
7.5.3 SMBus Alert Response
The INA3221 responds to the SMBus alert response address. The SMBus alert response provides a quick fault
identification for simple target devices. When an alert occurs, the controller broadcasts the alert response target
address (0001 100) with the R/ W bit set high. Following this alert response, any target devices that generated
an alert self-identifies by acknowledging the alert response, and sending the respective address on the bus.
The alert response can activate several different target devices simultaneously, similar to the I2C general call. If
more than one target attempts to respond, bus arbitration rules apply. The losing device does not generate an
acknowledge, and continues to hold the alert line low until the interrupt is cleared. Alerts must be in Latch mode
to function.
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 23
Product Folder Links: INA3221

## Page 24

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
7.6 Register Maps
The INA3221 uses a bank of registers for holding configuration settings, measurement results, minimum and
maximum limits, and status information. Table 7-3 summarizes the INA3221 registers; see the Functional Block
Diagram section for an illustration of the registers.
7.6.1 Summary of Register Set
Table 7-3. Summary of Register Set
POINTER POWER-ON RESET
ADDRESS
(Hex) REGISTER NAME DESCRIPTION BINARY HEX TYPE(1)
All-register reset, shunt and bus voltage ADC conversion times and
0 Configuration 01110001 00100111 7127 R/ W
averaging, operating mode.
1 Channel-1 Shunt Voltage Averaged shunt voltage value. 00000000 00000000 0000 R
2 Channel-1 Bus Voltage Averaged bus voltage value. 00000000 00000000 0000 R
3 Channel-2 Shunt Voltage Averaged shunt voltage value. 00000000 00000000 0000 R
4 Channel-2 Bus Voltage Averaged bus voltage value. 00000000 00000000 0000 R
5 Channel-3 Shunt Voltage Averaged shunt voltage value. 00000000 00000000 0000 R
6 Channel-3 Bus Voltage Averaged bus voltage value. 00000000 00000000 0000 R
Channel-1 Critical Alert Contains limit value to compare each conversion value to determine
7 01111111 11111000 7FF8 R/ W
Limit if the corresponding limit has been exceeded.
Channel-1 Warning Alert Contains limit value to compare to averaged measurement to
8 01111111 11111000 7FF8 R/ W
Limit determine if the corresponding limit has been exceeded.
Channel-2 Critical Alert Contains limit value to compare each conversion value to determine
9 01111111 11111000 7FF8 R/ W
Limit if the corresponding limit has been exceeded.
Channel-2 Warning Alert Contains limit value to compare to averaged measurement to
A 01111111 11111000 7FF8 R/ W
Limit determine if the corresponding limit has been exceeded.
Channel-3 Critical Alert Contains limit value to compare each conversion value to determine
B 01111111 11111000 7FF8 R/ W
Limit if the corresponding limit has been exceeded.
Channel-3 Warning Alert Contains limit value to compare to averaged measurement to
C 01111111 11111000 7FF8 R/ W
Limit determine if the corresponding limit has been exceeded.
Contains the summed value of the each of the selected shunt
D Shunt-Voltage Sum 00000000 00000000 0000 R
voltage conversions.
Contains limit value to compare to the Shunt Voltage Sum register to
E Shunt-Voltage Sum Limit 01111111 11111110 7FFE R/ W
determine if the corresponding limit has been exceeded.
Alert configuration, alert status indication, summation control and
F Mask/Enable 00000000 00000010 0002 R/ W
status.
Contains limit value to compare all bus voltage conversions to
10 Power-Valid Upper Limit 00100111 00010000 2710 R/ W
determine if the Power Valid level has been reached.
Contains limit value to compare all bus voltage conversions to
11 Power-Valid Lower Limit determine if the any voltage rail has dropped below the Power Valid 00100011 00101000 2328 R/ W
range.
FE Manufacturer ID Contains unique manufacturer identification number. 01010100 01001001 5449 R
FF Die ID Contains unique die identification number. 00110010 00100000 3220 R
(1) Type: R = read-only, R/ W = read/write.
24 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 25

INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
7.6.2 Register Descriptions
All 16-bit INA3221 registers are two 8-bit bytes via the I2C interface. Table 7-4 shows a register map for the INA3221.
Table 7-4. Register Map
ADDRESS
REGISTER (Hex) D15 D14 D13 D12 D11 D10 D9 D8 D7 D6 D5 D4 D3 D2 D1 D0
Configuration 00 RST CH1en CH2en CH3en AVG2 AVG1 AVG0 VBUSCT2 VBUSCT1 VBUSCT0 VSHCT2 VSHCT1 VSHCT0 MODE3 MODE2 MODE1
Channel-1 Shunt Voltage 01 SIGN SD11 SD10 SD9 SD8 SD7 SD6 SD5 SD4 SD3 SD2 SD1 SD0 — — —
Channel-1 Bus Voltage 02 SIGN BD11 BD10 BD9 BD8 BD7 BD6 BD5 BD4 BD3 BD2 BD1 BD0 — — —
Channel-2 Shunt Voltage 03 SIGN SD11 SD10 SD9 SD8 SD7 SD6 SD5 SD4 SD3 SD2 SD1 SD0 — — —
Channel-2 Bus Voltage 04 SIGN BD11 BD10 BD9 BD8 BD7 BD6 BD5 BD4 BD3 BD2 BD1 BD0 — — —
Channel-3 Shunt Voltage 05 SIGN SD11 SD10 SD9 SD8 SD7 SD6 SD5 SD4 SD3 SD2 SD1 SD0 — — —
Channel-3 Bus Voltage 06 SIGN BD11 BD10 BD9 BD8 BD7 BD6 BD5 BD4 BD3 BD2 BD1 BD0 — — —
Channel-1 Critical-Alert
07 C1L12 C1L11 C1L10 C1L9 C1L8 C1L7 C1L6 C1L5 C1L4 C1L3 C1L2 C1L1 C1L0 — — —
Limit
Channel-1 Warning-Alert
08 W1L12 W1L11 W1L10 W1L9 W1L8 W1L7 W1L6 W1L5 W1L4 W1L3 W1L2 W1L1 W1L0 — — —
Limit
Channel-2 Critical-Alert
09 C2L12 C2L11 C2L10 C2L9 C2L8 C2L7 C2L6 C2L5 C2L4 C2L3 C2L2 C2L1 C2L0 — — —
Limit
Channel-2 Warning-Alert
0A W2L12 W2L11 W2L10 W2L9 W2L8 W2L7 W2L6 W2L5 W2L4 W2L3 W2L2 W2L1 W2L0 — — —
Limit
Channel-3 Critical-Alert
0B C3L12 C3L11 C3L10 C3L9 C3L8 C3L7 C3L6 C3L5 C3L4 C3L3 C3L2 C3L1 C3L0 — — —
Limit
Channel-3 Warning-Alert
0C W3L12 W3L11 W3L10 W3L9 W3L8 W3L7 W3L6 W3L5 W3L4 W3L3 W3L2 W3L1 W3L0 — — —
Limit
Shunt-Voltage Sum 0D SIGN SV13 SV12 SV11 SV10 SV9 SV8 SV7 SV6 SV5 SV4 SV3 SV2 SV1 SV0 —
Shunt-Voltage Sum Limit 0E SIGN SVL13 SVL12 SVL11 SVL10 SVL9 SVL8 SVL7 SVL6 SVL5 SVL4 SVL3 SVL2 SVL1 SVL0 —
Mask/Enable 0F — SCC1 SCC2 SCC3 WEN CEN CF1 CF2 CF3 SF WF1 WF2 WF3 PVF TCF CVRF
Power-Valid Upper Limit 10 SIGN PVU11 PVU10 PVU9 PVU8 PVU7 PVU6 PVU5 PVU4 PVU3 PVU2 PVU1 PVU0 — — —
Power-Valid Lower Limit 11 SIGN PVL11 PVL10 PVL9 PVL8 PVL7 PVL6 PVL5 PVL4 PVL3 PVL2 PVL1 PVL0 — — —
Manufacturer ID FE 0 1 0 1 0 1 0 0 0 1 0 0 1 0 0 1
Die ID FF 0 0 1 1 0 0 1 0 0 0 1 0 0 0 0 0
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 25
Product Folder Links: INA3221

## Page 26

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
7.6.2.1 Configuration Register (address = 00h) [reset = 7127h]
The Configuration register settings control the operating modes for the shunt- and bus-voltage measurements for
the three input channels. This register controls the conversion time settings for both the shunt- and bus-voltage
measurements and the averaging mode used. The Configuration register is used to independently enable or
disable each channel, as well as select the operating mode that controls which signals are selected to be
measured.
This register can be read from at any time without impacting or affecting either device settings or conversions in
progress. Writing to this register halts any conversion in progress until the write sequence is completed, resulting
in a new conversion starting, based on the new Configuration register contents. This architecture prevents any
uncertainty in the conditions used for the next completed conversion.
Table 7-5. Configuration Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
RST CH1en CH2en CH3en AVG2 AVG1 AVG0 V
C
B
T
U
2
S V
C
B
T
U
1
S V
C
B
T
U
0
S
C
V
T
SH
2 C
V
T
SH
1 C
V
T
SH
0
MO
3
DE MO
2
DE MO
1
DE
RW-0 RW-1 RW-1 RW-1 RW-0 RW-0 RW-0 RW-1 RW-0 RW-0 RW-1 RW-0 RW-0 RW-1 RW-1 RW-1
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-6. Configuration Register Field Descriptions
Bit Field Type Reset Description
15 RST R/W 0h Reset bit. Set this bit = 1 to generate a system reset that is the
same as a power-on reset (POR). This bit resets all registers to
default values and self-clears.
14 CH1 R/W 7h Channel enable mode. These bits allow each channel to be
en
independently enabled or disabled.
13 CH2
en 0 = Channel disable
12 CH3 en 1 = Channel enable (default)
11-9 AVG2-0 R/W 0h Averaging mode. These bits set the number of samples that are
collected and averaged together.
000 = 1 (default)
001 = 4
010 = 16
011 = 64
100 = 128
101 = 256
110 = 512
111 = 1024
8-6 V CT2-0 R/W 4h Bus-voltage conversion time. These bits set the conversion time
BUS
for the bus-voltage measurement.
000 = 140μs
001 = 204μs
010 = 332μs
011 = 588μs
100 = 1.1ms (default)
101 = 2.116ms
110 = 4.156ms
111 = 8.244ms
5-3 V CT2-0 R/W 4h Shunt-voltage conversion time. These bits set the conversion
SH
time for the shunt-voltage measurement.
The conversion-time bit settings for V CT2-0 are the same as
SH
V CT2-0 (bits 8-6) listed in the previous row.
BUS
26 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 27

INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
Table 7-6. Configuration Register Field Descriptions (continued)
Bit Field Type Reset Description
2-0 MODE3-1 R/W 7h Operating mode. These bits select continuous, single-shot
(triggered), or power-down mode of operation. These bits default
to continuous shunt and bus mode.
000 = Power-down
001 = Shunt voltage, single-shot (triggered)
010 = Bus voltage, single-shot (triggered)
011 = Shunt and bus, single-shot (triggered)
100 = Power-down
101 = Shunt voltage, continuous
110 = Bus voltage, continuous
111 = Shunt and bus, continuous (default)
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 27
Product Folder Links: INA3221

## Page 28

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
7.6.2.2 Channel-1 Shunt-Voltage Register (address = 01h), [reset = 00h]
This register contains the averaged shunt-voltage measurement for channel 1. This register stores the current
shunt-voltage reading, V , for channel 1. Negative numbers are represented in twos complement format.
SHUNT
Generate the twos complement of a negative number by complementing the absolute value binary number and
adding 1. Extend the sign, denoting a negative number by setting MSB = 1.
Full-scale range = 163.8mV (decimal = 7FF8); LSB (SD0): 40μV.
Example: For a value of V = –80mV:
SHUNT
1. Take the absolute value: 80mV
2. Translate this number to a whole decimal number (80mV / 40μV) = 2000
3. Convert this number to binary = 011 1110 1000 0_ _ _ (last three bits are set to 0)
4. Complement the binary result = 100 0001 0111 1111
5. Add 1 to the complement to create the twos complement result = 100 0001 1000 0000
6. Extend the sign and create the 16-bit word: 1100 0001 1000 0000 = C180h
Table 7-7. Channel-1 Shunt-Voltage Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
SIGN SD11 SD10 SD9 SD8 SD7 SD6 SD5 SD4 SD3 SD2 SD1 SD0 — — —
R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-8. Channel-1 Shunt-Voltage Register Field Descriptions
Bit Field Type Reset Description
15 SIGN R 0h Sign bit.
0 = positive number
1 = negative number in twos complement format
14-3 SD11-0 R 0h Channel-1 shunt-voltage data bits
2-0 Reserved R 0h Reserved
7.6.2.3 Channel-1 Bus-Voltage Register (address = 02h) [reset = 00h]
This register stores the bus voltage reading, V , for channel 1. Full-scale range = 32.76V (decimal = 7FF8);
BUS
LSB (BD0) = 8mV. Although the input range is 26V, the full-scale range of the ADC scaling is 32.76V. Do not
apply more than 26V.
Table 7-9. Channel-1 Bus-Voltage Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
SIGN BD11 BD10 BD9 BD8 BD7 BD6 BD5 BD4 BD3 BD2 BD1 BD0 — — —
R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-10. Channel-1 Bus-Voltage Register Field Descriptions
Bit Field Type Reset Description
15 SIGN R 0h Sign bit.
0 = positive number
1 = negative number in twos complement format.
14-3 BD11-0 R 0h Channel-1 bus-voltage data bits
2-0 Reserved R 0h Reserved
28 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 29

INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
7.6.2.4 Channel-2 Shunt-Voltage Register (address = 03h) [reset = 00h]
This register contains the averaged shunt voltage measurement for channel 2. Full-scale range = 163.8mV
(decimal = 7FF8); LSB (SD0): 40μV. Although the input range is 26V, the full-scale range of the ADC scaling is
32.76V. Do not apply more than 26V.
Table 7-11. Channel-2 Shunt-Voltage Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
SIGN SD11 SD10 SD9 SD8 SD7 SD6 SD5 SD4 SD3 SD2 SD1 SD0 — — —
R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-12. Channel-2 Shunt-Voltage Register Field Descriptions
Bit Field Type Reset Description
15 SIGN R 0h Sign bit.
0 = positive number
1 = negative number in twos complement format
14-3 SD11-0 R 0h Channel-2 shunt-voltage data bits
2-0 Reserved R 0h Reserved
7.6.2.5 Channel-2 Bus-Voltage Register (address = 04h) [reset = 00h]
This register stores the bus voltage reading, V , for channel 2. Full-scale range = 32.76V (decimal = 7FF8);
BUS
LSB (BD0) = 8mV. Although the input range is 26V, the full-scale range of the ADC scaling is 32.76V. Do not
apply more than 26V.
Table 7-13. Channel-2 Bus-Voltage Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
SIGN BD11 BD10 BD9 BD8 BD7 BD6 BD5 BD4 BD3 BD2 BD1 BD0 — — —
R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-14. Channel-2 Bus-Voltage Register Field Descriptions
Bit Field Type Reset Description
15 SIGN R 0h Sign bit.
0 = positive number
1 = negative number in twos complement format
14-3 BD11-0 R 0h Channel-2 bus-voltage data bits
2-0 Reserved R 0h Reserved
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 29
Product Folder Links: INA3221

## Page 30

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
7.6.2.6 Channel-3 Shunt-Voltage Register (address = 05h) [reset = 00h]
This register contains the averaged shunt voltage measurement for channel 3. Full-scale range = 163.8mV
(decimal = 7FF8); LSB (SD0): 40μV.
Table 7-15. Channel-3 Shunt-Voltage Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
SIGN SD11 SD10 SD9 SD8 SD7 SD6 SD5 SD4 SD3 SD2 SD1 SD0 — — —
R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-16. Channel-3 Shunt-Voltage Register Field Descriptions
Bit Field Type Reset Description
15 SIGN R 0h Sign bit.
0 = positive number
1 = negative number in twos complement format
14-3 SD11-0 R 0h Channel-3 shunt-voltage data bits
2-0 Reserved R 0h Reserved
7.6.2.7 Channel-3 Bus-Voltage Register (address = 06h) [reset = 00h]
This register stores the bus voltage reading, V , for channel 3. Full-scale range = 32.76V (decimal = 7FF8);
BUS
LSB (BD0) = 8mV. Although the input range is 26V, the full-scale range of the ADC scaling is 32.76V. Do not
apply more than 26V.
Table 7-17. Channel-3 Bus-Voltage Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
SIGN BD11 BD10 BD9 BD8 BD7 BD6 BD5 BD4 BD3 BD2 BD1 BD0 — — —
R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-18. Channel-3 Bus-Voltage Register Field Descriptions
Bit Field Type Reset Description
15 SIGN R 0h Sign bit.
0 = positive number
1 = negative number in twos complement format
14-3 BD11-0 R 0h Channel-3 bus-voltage data bits
2-0 Reserved R 0h Reserved
30 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 31

INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
7.6.2.8 Channel-1 Critical-Alert Limit Register (address = 07h) [reset = 7FF8h]
This register contains the value used to compare to each shunt voltage conversion on channel 1 to detect fast
overcurrent events.
Table 7-19. Channel-1 Critical-Alert Limit Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
C1L12 C1L11 C1L10 C1L9 C1L8 C1L7 C1L6 C1L5 C1L4 C1L3 C1L2 C1L1 C1L0 — — —
RW-0 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-0 RW-0 RW-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-20. Channel-1 Critical-Alert Limit Register Field Descriptions
Bit Field Type Reset Description
15-3 C1L12-0 R/W FFFh Channel-1 critical-alert-limit data bits
2-0 Reserved R/W 0h Reserved
7.6.2.9 Warning-Alert Channel-1 Limit Register (address = 08h) [reset = 7FF8h]
This register contains the value used to compare to the averaged shunt voltage value of channel 1 to detect a
longer duration overcurrent event.
Table 7-21. Channel-1 Warning-Alert Limit Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
W1L12 W1L11 W1L10 W1L9 W1L8 W1L7 W1L6 W1L5 W1L4 W1L3 W1L2 W1L1 W1L0 — — —
RW-0 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-0 RW-0 RW-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-22. Channel-1 Warning-Alert Limit Register Field Descriptions
Bit Field Type Reset Description
15-3 W1L12-0 R/W FFFh Channel-1 warning-alert-limit data bits
2-0 Reserved R/W 0h Reserved
7.6.2.10 Channel-2 Critical-Alert Limit Register (address = 09h) [reset = 7FF8h]
This register contains the value used to compare to each shunt voltage conversion on channel 2 to detect fast
overcurrent events.
Table 7-23. Channel-2 Critical-Alert Limit Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
C2L12 C2L11 C2L10 C2L9 C2L8 C2L7 C2L6 C2L5 C2L4 C2L3 C2L2 C2L1 C2L0 — — —
RW-0 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-0 RW-0 RW-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-24. Channel-2 Critical-Alert Limit Register Field Descriptions
Bit Field Type Reset Description
15-3 C2L12-0 R/W FFFh Channel-2 critical-alert-limit data bits
2-0 Reserved R/W 0h Reserved
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 31
Product Folder Links: INA3221

## Page 32

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
7.6.2.11 Channel-2 Warning-Alert Limit Register (address = 0Ah) [reset = 7FF8h]
This register contains the value used to compare to the averaged shunt voltage value of channel 2 to detect a
longer duration overcurrent event.
Table 7-25. Channel-2 Warning-Alert Limit Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
W2L12 W2L11 W2L10 W2L9 W2L8 W2L7 W2L6 W2L5 W2L4 W2L3 W2L2 W2L1 W2L0 — — —
RW-0 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-0 RW-0 RW-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-26. Channel-2 Warning-Alert Limit Register Field Descriptions
Bit Field Type Reset Description
15-3 W2L12-0 R/W FFFh Channel-2 warning-alert-limit data bits
2-0 Reserved R/W 0h Reserved
7.6.2.12 Channel-3 Critical-Alert Limit Register (address = 0Bh) [reset = 7FF8h]
This register contains the value used to compare to each shunt voltage conversion on channel 3 to detect fast
overcurrent events.
Table 7-27. Channel-3 Critical-Alert Limit Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
C3L12 C3L11 C3L10 C3L9 C3L8 C3L7 C3L6 C3L5 C3L4 C3L3 C3L2 C3L1 C3L0 — — —
RW-0 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-0 RW-0 RW-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-28. Channel-3 Critical-Alert Limit Register Field Descriptions
Bit Field Type Reset Description
15-3 C3L12-0 R/W FFFh Channel-3 critical-alert-limit data bits
2-0 Reserved R/W 0h Reserved
7.6.2.13 Channel-3 Warning-Alert Limit Register (address = 0Ch) [reset = 7FF8h]
This register contains the value used to compare to the averaged shunt voltage value of channel 3 to detect a
longer duration overcurrent event.
Table 7-29. Channel-3 Warning-Alert Limit Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
W3L12 W3L11 W3L10 W3L9 W3L8 W3L7 W3L6 W3L5 W3L4 W3L3 W3L2 W3L1 W3L0 — — —
RW-0 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-0 RW-0 RW-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-30. Channel-3 Warning-Alert Limit Register Field Descriptions
Bit Field Type Reset Description
15-3 W3L12-0 R/W FFFh Channel-3 warning-alert limit data bits
2-0 Reserved R/W 0h Reserved
32 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 33

INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
7.6.2.14 Shunt-Voltage Sum Register (address = 0Dh) [reset = 00h]
This register contains the sum of the single conversion shunt voltages of the selected channels based on the
summation control bits 12, 13, and 14 in the Mask/Enable register.
This register is updated with the most recent sum following each complete cycle of all selected channels. The
Shunt-Voltage Sum register LSB value is 40μV.
Table 7-31. Shunt-Voltage Sum Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
SIGN SV13 SV12 SV11 SV10 SV9 SV8 SV7 SV6 SV5 SV4 SV3 SV2 SV1 SV0 —
R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0 R-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-32. Shunt-Voltage Sum Register Field Descriptions
Bit Field Type Reset Description
15 SIGN R 0h Sign bit.
0 = positive number
1 = negative number in twos complement format
14-1 SV13-0 R 0h Shunt-voltage sum data bits
0 Reserved R 0h Reserved
7.6.2.15 Shunt-Voltage Sum-Limit Register (address = 0Eh) [reset = 7FFEh]
This register contains the value that is compared to the Shunt-Voltage Sum register value following each
completed cycle of all selected channels to detect for system overcurrent events. The Shunt-Voltage Sum-Limit
register LSB value is 40μV.
Table 7-33. Shunt-Voltage Sum-Limit Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
SIGN SVL13 SVL12 SVL11 SVL10 SVL9 SVL8 SVL7 SVL6 SVL5 SVL4 SVL3 SVL2 SVL1 SVL0 —
RW-0 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-1 RW-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-34. Shunt-Voltage Sum-Limit Register Field Descriptions
Bit Field Type Reset Description
15 SIGN R 0h Sign bit.
0 = positive number
1 = negative number in twos complement format
14-1 SVL13-0 R 0h Shunt-voltage sum-limit data bits
0 Reserved R 0h Reserved
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 33
Product Folder Links: INA3221

## Page 34

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
7.6.2.16 Mask/Enable Register (address = 0Fh) [reset = 0002h]
This register selects which function is enabled to control the Critical alert and Warning alert pins, and how each
warning alert responds to the corresponding channel. Read the Mask/Enable register to clear any flag results
present. Writing to this register does not clear the flag bit status. To make sure that there is no uncertainty in the
warning function setting that resulted in a flag bit being set, the Mask/Enable register must be read from to clear
the flag bit status before changing the warning function setting.
Table 7-35. Mask/Enable Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
— SCC1 SCC2 SCC3 WEN CEN CF1 CF2 CF3 SF WF1 WF2 WF3 PVF TCF CVRF
RW-0 RW-0 RW-0 RW-0 RW-0 RW-0 RW-0 RW-0 RW-0 RW-0 RW-0 RW-0 RW-0 RW-0 RW-1 RW-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-36. Mask/Enable Register Field Descriptions
Bit Field Type Reset Description
15 Reserved R/W 0h Reserved
14-12 SCC1-3 R/W 0h Summation channel control. These bits determine which shunt voltage measurement
channels are enabled to fill the Shunt-Voltage Sum register. The selection of these bits
does not impact the individual channel enable or disable status, or the corresponding
channel measurements. The corresponding bit is used to select if the channel is used
to fill the Shunt-Voltage Sum register.
0 = Disabled (default)
1 = Enabled
11 WEN R/W 0h Warning alert latch enable. These bits configure the latching feature of the Warning
alert pin.
0 = Transparent (default)
1 = Latch enabled
10 CEN R/W 0h Critical alert latch enable. These bits configure the latching feature of the Critical alert
pin.
0 = Transparent (default)
1 = Latch enabled
9-7 CF1-3 R/W 0h Critical-alert flag indicator. These bits are asserted if the corresponding channel
measurement has exceeded the critical alert limit resulting in the Critical alert pin being
asserted. Read these bits to determine which channel caused the critical alert. The
critical alert flag bits are cleared when the Mask/Enable register is read back.
6 SF R/W 0h Summation-alert flag indicator. This bit is asserted if the Shunt Voltage Sum register
exceeds the Shunt Voltage Sum Limit register. If the summation alert flag is asserted,
the Critical alert pin is also asserted. The Summation Alert Flag bit is cleared when the
Mask/Enable register is read back.
5-3 WF1-3 R/W 0h Warning-alert flag indicator. These bits are asserted if the corresponding channel
averaged measurement has exceeded the warning alert limit, resulting in the Warning
alert pin being asserted. Read these bits to determine which channel caused the
warning alert. The Warning Alert Flag bits clear when the Mask/Enable register is read
back.
2 PVF R/W 0h Power-valid-alert flag indicator. This bit can be used to be able to determine if the
power valid (PV) alert pin has been asserted through software rather than hardware.
The bit setting corresponds to the status of the PV pin. This bit does not clear until the
condition that caused the alert is removed, and the PV pin has cleared.
1 TCF R/W 11h Timing-control-alert flag indicator. Use this bit to determine if the timing control (TC)
alert pin has been asserted through software rather than hardware. The bit setting
corresponds to the status of the TC pin. This bit does not clear after it has been
asserted unless the power is recycled or a software reset is issued. The default state
for the timing control alert flag is high.
34 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 35

INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
Table 7-36. Mask/Enable Register Field Descriptions (continued)
Bit Field Type Reset Description
0 CVRF R/W 0h Conversion-ready flag. Although the INA3221 can be read at any time, and the data
from the last conversion are available, the conversion ready bit is provided to help
coordinate single-shot conversions. The conversion bit is set after all conversions are
complete. Conversion ready clears under the following conditions:
1. Writing the Configuration register (except for power-down or disable-mode
selections).
2. Reading the Mask/Enable register.
7.6.2.17 Power-valid Upper-limit Register (address = 10h) [reset = 2710h]
This register contains the value used to determine if the power-valid conditions are met. The power-valid
condition is reached when all bus-voltage channels exceed the value set in this limit register. When the power-
valid condition is met, the PV alert pin asserts high to indicate that the INA3221 has confirmed all bus voltage
channels are above the power-valid upper-limit value. For the power-valid conditions to be monitored, the bus
measurements must be enabled through one of the corresponding MODE bits set in the Configuration register.
The power-valid upper-limit LSB value is 8mV. Power-on reset value is 2710h = 10.000V.
Table 7-37. Power-Valid Upper-Limit Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
SIGN PVU11 PVU10 PVU9 PVU8 PVU7 PVU6 PVU5 PVU4 PVU3 PVU2 PVU1 PVU0 — — —
R-0 RW-0 RW-1 RW-0 RW-0 RW-1 RW-1 RW-1 RW-0 RW-0 RW-0 RW-1 RW-0 RW-0 RW-0 RW-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-38. Power-Valid Upper-Limit Register Field Descriptions
Bit Field Type Reset Description
15 SIGN R 0h Power-valid upper-limit data bits
14-3 PVU11-0 R/W 4E2h Power-valid upper-limit data bits
2-0 Reserved R/W 0h Reserved
7.6.2.18 Power-Valid Lower-Limit Register (address = 11h) [reset = 2328h]
This register contains the value used to determine if any of the bus-voltage channels drops below the power-
valid lower-limit when the power-valid conditions are met. This limit contains the value used to compare all
bus-channel readings to verify that all channels remain above the power-valid lower-limit, thus maintaining
the power-valid condition. If any bus-voltage channel drops below the power-valid lower-limit, the PV alert pin
pulls low to indicate that the INA3221 detects a bus voltage reading below the power-valid lower-limit. For the
power-valid condition to be monitored, the bus measurements must be enabled through the mode (MODE3-1)
bits set in the Configuration register. The power-valid lower-limit LSB value is 8mV. Power-on reset value is
2328h = 9.000V.
Table 7-39. Power-Valid Lower-Limit Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
SIGN PVL11 PVL10 PVL9 PVL8 PVL7 PVL6 PVL5 PVL4 PVL3 PVL2 PVL1 PVL0 — — —
R-0 RW-0 RW-1 RW-0 RW-0 RW-0 RW-1 RW-1 RW-0 RW-0 RW-1 RW-0 RW-1 RW-0 RW-0 RW-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-40. Power-Valid Lower-Limit Register Field Descriptions
Bit Field Type Reset Description
15 SIGN R 0h Power-valid lower-limit data bits
14-3 PVL11-0 R/W 465h Power-valid lower-limit data bits
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 35
Product Folder Links: INA3221

## Page 36

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
Table 7-40. Power-Valid Lower-Limit Register Field Descriptions (continued)
Bit Field Type Reset Description
2-0 Reserved R/W 0h Reserved
7.6.2.19 Manufacturer ID Register (address = FEh) [reset = 5449h]
This register contains a factory-programmable identification value that identifies this device as being
manufactured by Texas Instruments. This register distinguishes this device from other devices that are on the
same I2C bus. The contents of this register are 5449h, or TI in ASCII.
Table 7-41. Manufacturer ID Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
D15 D14 D13 D12 D11 D10 D9 D8 D7 D6 D5 D4 D3 D2 D1 D0
R-0 R-1 R-0 R-1 R-0 R-1 R-0 R-0 R-0 R-1 R-0 R-0 R-1 R-0 R-0 R-1
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-42. Manufacturer ID Register Field Descriptions
Bit Field Type Reset Description
15-0 D15-0 R 5449h Manufacturer ID bits
7.6.2.20 Die ID Register (address = FFh) [reset = 3220]
This register contains a factory-programmable identification value that identifies this device as an INA3221. This
register distinguishes this device from other devices that are on the same I2C bus. The Die ID for the INA3221 is
3220h.
Table 7-43. Die ID Register
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
D15 D14 D13 D12 D11 D10 D9 D8 D7 D6 D5 D4 D3 D2 D1 D0
R-0 R-0 R-1 R-1 R-0 R-0 R-1 R-0 R-0 R-0 R-1 R-0 R-0 R-0 R-0 R-0
LEGEND: R/W = Read/Write; R = Read only; -n = value after reset
Table 7-44. Die ID Register Field Descriptions
Bit Field Type Reset Description
15-0 D15-0 R 3220h Die ID bits
36 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 37

INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
8 Application and Implementation
Note
Information in the following applications sections is not part of the TI component specification,
and TI does not warrant its accuracy or completeness. TI’s customers are responsible for
determining suitability of components for their purposes, as well as validating and testing their design
implementation to confirm system functionality.
8.1 Application Information
INA3221 is a three-channel current and bus voltage monitor with I2C/SMBUS-compatible interface. It features
programmable conversion times and averaging modes and offers both critical and warning alerts to detect
multiple programmable out-of-range conditions for each channel.
8.2 Typical Application
The INA3221 measures the voltage developed across a current-sensing resistor when current passes through
the resistor. The device also measures the bus supply voltage at the IN- pin. Multiple monitoring functions are
supported using four alert pins: Critical, Warning, PV, and TC. Programmable thresholds make sure operation is
within desired operating conditions. This design illustrates the ability of the Critical alert pin to respond to a set
threshold.
Figure 8-1 illustrates a typical INA3221 application circuit using all three channels. For best performance, use a
0.1μF ceramic capacitor for power-supply bypassing, placed as close as possible to the supply and ground pins.
The digital pins (SCL, SDA, Critical, Warning, TC) are connected to supply through pull-up resistors. The power
valid (PV) alert pin is connected to the VPU pin through a pull-up resistor to enable power-valid monitoring.
Power Supply
(0V to 26V) CBYPASS
0.1μF
Load 1
VS (Supply
VIN+1 VIN–1 Voltage) 10kΩ
Power Supply
(0V to 26V) SDA
I2C-
CH 1 and SCL
Bus SMBus-
VIN+2 CH 2 Volt
S
a
h
g
u
e
n
s
t
1-3 C
I
o
n
m
te
p
rf
a
a
t
c
ib
e
le A0
VPU VS
VIN–2 ADC Voltages 1-3
Critical Limit
10kΩ
Alerts 1-3 VPU
Power Valid (PV)
Load 2 CH 3 Shunt Voltage Critical
Sum Alerts
Warning
Timing Control (TC)
GND
VIN+3 VIN–3
Power Supply
(0V to 26V) Load 3
Figure 8-1. INA3221 as an Overcurrent Sensor
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 37
Product Folder Links: INA3221

## Page 38

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
8.2.1 Design Requirements
For this design example, use the input parameters shown in Table 8-1. All other register settings are default.
Table 8-1. Design Parameters
DESIGN PARAMETER EXAMPLE VALUE
Supply voltage, V 5V
S
Pull-up resistors 10kΩ
Input range –163.84 to +163.8
Enabled channel CH1
Operating mode Shunt voltage, continuous
Average setting 1
Critical alert limit 80mV
Critical Alert Limit register setting 7D0h
8.2.2 Detailed Design Procedure
This design shows two shunt voltage conversion times to demonstrate the difference in the alert response times.
This design generates a critical-alert response when the input voltage exceeds 80mV on channel 1. See Table
8-1 for all design parameters.
For the first example the shunt voltage conversion time is set to 1.1ms. When the input signal exceeds 80mV,
the Critical alert pin pulls low after the conversion cycle completes, indicating an overcurrent condition, as shown
in Figure 8-2.
For the second example, the conversion time is set to 588μs, and the response is shown in Figure 8-3.
8.2.3 Application Curves
Figure 8-2 shows the Critical alert pin response to a shunt voltage overlimit of 80mV for a conversion time of
1.1ms. Figure 8-3 shows the response for the same limit, but with the conversion time reduced to 588μs.
Criti c
al
Al
ert
( 2
V/
di
v)
CRITICAL ALERT
Criti c
al
Al
ert
( 2
V/
di
v)
CRITICAL ALERT
INPUT INPUT
LIMIT LIMIT
mit
di
v) mit
di
v)
ut/
Li
m
V/
ut/
Li
m
V/
p 0 p 0
n 5 n 5
I ( I (
Time (200 s/div) Time (100 s/div)
Configuration register = 4125h, conversion time = 1.1ms Configuration register = 40DDh, conversion time = 588μs
Figure 8-2. Critical Alert Response for 1.1ms Figure 8-3. Critical Alert Response for 588μs
Conversion Time Conversion Time
8.3 Power Supply Recommendations
The input circuitry of the device can accurately measure signals on common-mode voltages beyond the power
supply voltage, V . For example, the voltage applied to the V power supply terminal can be 5V, whereas the
S S
load power-supply voltage being monitored (the common-mode voltage) on any one of the three channels can
be as high as 26V. Note also that the device can withstand the full 0V to 26V range at the input terminals,
38 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 39

regardless of whether the device has power applied or not. Place the required power-supply bypass capacitors
as close as possible to the supply and ground terminals of the device to provide stability. A typical value for
this supply bypass capacitor is 0.1μF. Applications with noisy or high-impedance power supplies can require
additional decoupling capacitors to reject power-supply noise.
8.4 Layout
8.4.1 Layout Guidelines
Connect the input pins (IN+ and IN–) of all the used channels to the sensing resistor using a Kelvin connection
or a 4-wire connection. These connection techniques verify that only the current-sensing resistor impedance
is detected between the input pins. Poor routing of the current-sensing resistor commonly results in additional
resistance present between the input pins. Given the very low ohmic value of the current-sensing resistor,
any additional high-current carrying impedance causes significant measurement errors. Place the power-supply
bypass capacitor as close as possible to the supply and ground pins.
8.4.2 Layout Example
IN(cid:16)3 IN+1
IN+3 IN(cid:16)1
GND PV
VS Critical
UPV
0A
2+NI
LCS
(cid:16)
ADS
2
NI
gninraW
CT
To Load Connect to To Bus Power Supply
bus power-
supply rail
Connect
Supply to VPU
Bypass
Capacitor
To Bus Power To Load
Supply
Via to Ground Plane
Via to Power Plane
I2C- and SMBUS-
Compatible Interface Warning Output
ylppuS
rewoP
suB
oT
daoL
oT
INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
Timing Control Output
Critical
Output
Figure 8-4. Layout Example
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 39
Product Folder Links: INA3221

## Page 40

INA3221
SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025 www.ti.com
9 Device and Documentation Support
9.1 Device Support
9.1.1 Development Support
• For INA3221 evaluation module (EVM), go to www.ti.com/tool/INA3221EVM
9.2 Documentation Support
9.2.1 Related Documentation
• Texas Instruments, INA3221EVM User Guide
9.3 Receiving Notification of Documentation Updates
To receive notification of documentation updates, navigate to the device product folder on ti.com. Click on
Notifications to register and receive a weekly digest of any product information that has changed. For change
details, review the revision history included in any revised document.
9.4 Support Resources
TI E2ETM support forums are an engineer's go-to source for fast, verified answers and design help — straight
from the experts. Search existing answers or ask your own question to get the quick design help you need.
Linked content is provided "AS IS" by the respective contributors. They do not constitute TI specifications and do
not necessarily reflect TI's views; see TI's Terms of Use.
9.5 Trademarks
TI E2ETM is a trademark of Texas Instruments.
All trademarks are the property of their respective owners.
9.6 Electrostatic Discharge Caution
This integrated circuit can be damaged by ESD. Texas Instruments recommends that all integrated circuits be handled
with appropriate precautions. Failure to observe proper handling and installation procedures can cause damage.
ESD damage can range from subtle performance degradation to complete device failure. Precision integrated circuits may
be more susceptible to damage because very small parametric changes could cause the device not to meet its published
specifications.
9.7 Glossary
TI Glossary This glossary lists and explains terms, acronyms, and definitions.
10 Revision History
NOTE: Page numbers for previous revisions may differ from page numbers in the current version.
Changes from Revision B (September 2014) to Revision C (September 2025) Page
Changes from Revision A (June 2013) to Revision B (September 2014) Page
• Added Device Informationand ESD Ratings tables, and Feature Description, Device Functional Modes,
Application and Implementation, Power Supply Recommendations, Layout, Device and Documentation
Support, and Mechanical, Packaging, and Orderable Information sections.......................................................1
• Deleted trace from SDA to SCL, and added missing connector dot to V in front-page diagram ..................... 1
S
• Added (V ) + (V ) / 2 to common-mode analog inputs in the Absolute Maximum Ratings table ................. 4
IN+ IN–
• Deleted VBUS from analog inputs in Absolute Maximum Ratings table ........................................................... 4
• Added operating temperature to Absolute Maximum Ratings table................................................................... 4
• Changed all V to V throughout data sheet for consistency.............................................................. 5
SENSE SHUNT
• Changed "Status register" to "Mask/Enable register" to clarify register name in Basic ADC Functions section...
.......................................................................................................................................................................... 11
40 Submit Document Feedback Copyright © 2025 Texas Instruments Incorporated
Product Folder Links: INA3221

## Page 41

INA3221
www.ti.com SBOS576C – MAY 2012 – REVISED SEPTEMBER 2025
• Changed Critical Alert section text for clarity. .................................................................................................. 12
• Added Summation Control Function section.................................................................................................... 12
• Changed external "R " to "R " in Figure 20............................................................................................ 13
PU PU_ext
• Changed Multiple Channel Monitoring section text for clarity. .........................................................................17
• Added X and Y axis labels to Figure 25........................................................................................................... 18
• Changed "bidirectional" to "I/O" in second paragraph of Bus Overview section.............................................. 20
• Changed V to VS in Table 1..........................................................................................................................20
S+
• Changed references in Figure 30 to point to correct notes.............................................................................. 21
• Changed Figure 31...........................................................................................................................................22
• Changed values in Table 2, Bus Timing Definitions ........................................................................................ 22
• Added data valid time to Table 2, Bus Timing Definitions ................................................................................22
• Changed fall time to split data and clock times in Table 2, Bus Timing Definitions ......................................... 22
• Deleted rise time for data in Table 2, Bus Timing Definitions .......................................................................... 22
• Deleted trace from SDA to SCL, and added missing connector dot to V in Figure 52 .................................. 37
S
Changes from Revision * (May 2012) to Revision A (June 2013) Page
• Changed Shunt voltage input range parameter values in Electrical Characteristics table................................. 5
• Updated Figure 19............................................................................................................................................13
• Changed second paragraph of Serial Bus Address section............................................................................. 20
• Updated Figure 27 and note (1)....................................................................................................................... 21
• Updated Figure 28 and note (1)....................................................................................................................... 21
• Updated Figure 29 and note (1)....................................................................................................................... 21
• Updated Figure 30 and note (1)....................................................................................................................... 21
• Changed bit D15 in Power Valid Upper Limit Register..................................................................................... 35
• Changed bit D15 in Power Valid Lower Limit Register..................................................................................... 35
11 Mechanical, Packaging, and Orderable Information
The following pages include mechanical, packaging, and orderable information. This information is the most
current data available for the designated devices. This data is subject to change without notice and revision of
this document. For browser-based versions of this data sheet, refer to the left-hand navigation.
Copyright © 2025 Texas Instruments Incorporated Submit Document Feedback 41
Product Folder Links: INA3221

## Page 42

PACKAGE OPTION ADDENDUM
www.ti.com 5-Mar-2026
PACKAGING INFORMATION
Orderable part number Status Material type Package | Pins Package qty | Carrier RoHS Lead finish/ MSL rating/ Op temp (°C) Part marking
(1) (2) (3) Ball material Peak reflow (6)
(4) (5)
INA3221AIRGVR Active Production VQFN (RGV) | 16 2500 | LARGE T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 INA
3221
INA3221AIRGVR.B Active Production VQFN (RGV) | 16 2500 | LARGE T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 INA
3221
INA3221AIRGVRG4 Active Production VQFN (RGV) | 16 2500 | LARGE T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 INA
3221
INA3221AIRGVRG4.B Active Production VQFN (RGV) | 16 2500 | LARGE T&R Yes NIPDAU Level-1-260C-UNLIM -40 to 125 INA
3221
INA3221AIRGVT Obsolete Production VQFN (RGV) | 16 - - Call TI Call TI -40 to 125 INA
3221
(1) Status: For more details on status, see our product life cycle.
(2) Material type: When designated, preproduction parts are prototypes/experimental devices, and are not yet approved or released for full production. Testing and final process, including without limitation quality assurance,
reliability performance testing, and/or process qualification, may not yet be complete, and this item is subject to further changes or possible discontinuation. If available for ordering, purchases will be subject to an additional
waiver at checkout, and are intended for early internal evaluation purposes only. These items are sold without warranties of any kind.
(3) RoHS values: Yes, No, RoHS Exempt. See the TI RoHS Statement for additional information and value definition.
(4) Lead finish/Ball material: Parts may have multiple material finish options. Finish options are separated by a vertical ruled line. Lead finish/Ball material values may wrap to two lines if the finish value exceeds the maximum
column width.
(5) MSL rating/Peak reflow: The moisture sensitivity level ratings and peak solder (reflow) temperatures. In the event that a part has multiple moisture sensitivity ratings, only the lowest level per JEDEC standards is shown.
Refer to the shipping label for the actual reflow temperature that will be used to mount the part to the printed circuit board.
(6) Part marking: There may be an additional marking, which relates to the logo, the lot trace code information, or the environmental category of the part.
Multiple part markings will be inside parentheses. Only one part marking contained in parentheses and separated by a "~" will appear on a part. If a line is indented then it is a continuation of the previous line and the two
combined represent the entire part marking for that device.
Important Information and Disclaimer:The information provided on this page represents TI's knowledge and belief as of the date that it is provided. TI bases its knowledge and belief on information provided by third parties, and
makes no representation or warranty as to the accuracy of such information. Efforts are underway to better integrate information from third parties. TI has taken and continues to take reasonable steps to provide representative
and accurate information but may not have conducted destructive testing or chemical analysis on incoming materials and chemicals. TI and TI suppliers consider certain information to be proprietary, and thus CAS numbers
and other limited information may not be available for release.
Addendum-Page 1

## Page 43

PACKAGE OPTION ADDENDUM
www.ti.com 5-Mar-2026
In no event shall TI's liability arising out of such information exceed the total purchase price of the TI part(s) at issue in this document sold by TI to Customer on an annual basis.
OTHER QUALIFIED VERSIONS OF INA3221 :
• Automotive : INA3221-Q1
NOTE: Qualified Version Definitions:
• Automotive - Q100 devices qualified for high-reliability automotive applications targeting zero defects
Addendum-Page 2

## Page 44

PACKAGE MATERIALS INFORMATION
www.ti.com 1-Jan-2026
TAPE AND REEL INFORMATION
REEL DIMENSIONS TAPE DIMENSIONS
K0 P1
W
B0
Reel
Diameter
Cavity A0
A0 Dimension designed to accommodate the component width
B0 Dimension designed to accommodate the component length
K0 Dimension designed to accommodate the component thickness
W Overall width of the carrier tape
P1 Pitch between successive cavity centers
Reel Width (W1)
QUADRANT ASSIGNMENTS FOR PIN 1 ORIENTATION IN TAPE
Sprocket Holes
Q1 Q2 Q1 Q2
Q3 Q4 Q3 Q4 User Direction of Feed
Pocket Quadrants
*All dimensions are nominal
Device Package Package Pins SPQ Reel Reel A0 B0 K0 P1 W Pin1
Type Drawing Diameter Width (mm) (mm) (mm) (mm) (mm) Quadrant
(mm) W1 (mm)
INA3221AIRGVR VQFN RGV 16 2500 330.0 12.4 4.25 4.25 1.15 8.0 12.0 Q2
INA3221AIRGVRG4 VQFN RGV 16 2500 330.0 12.4 4.25 4.25 1.15 8.0 12.0 Q2
Pack Materials-Page 1

## Page 45

PACKAGE MATERIALS INFORMATION
www.ti.com 1-Jan-2026
TAPE AND REEL BOX DIMENSIONS
Width (mm) H
W L
*All dimensions are nominal
Device Package Type Package Drawing Pins SPQ Length (mm) Width (mm) Height (mm)
INA3221AIRGVR VQFN RGV 16 2500 346.0 346.0 33.0
INA3221AIRGVRG4 VQFN RGV 16 2500 346.0 346.0 33.0
Pack Materials-Page 2

## Page 46

GENERIC PACKAGE VIEW
RGV 16 VQFN - 1 mm max height
4 x 4, 0.65 mm pitch PLASTIC QUAD FLATPACK - NO LEAD
Images above are just a representation of the package family, actual package may vary.
Refer to the product data sheet for package details.
4224748/A
www.ti.com

## Page 47

PACKAGE OUTLINE
RGV0016A VQFN - 1 mm max height
SCALE 3.000
PLASTIC QUAD FLATPACK - NO LEAD
4.15
B A
3.85
PIN 1 INDEX AREA
4.15
3.85
C
1.0
0.8
SEATING PLANE
0.08 C
0.05
0.00
2.16 0.1
2X 1.95
SYMM (0.2) TYP
5 8
EXPOSED (0.37) TYP
THERMAL PAD
4 9
2X 1.95 SYMM 17
2.16 0.1
12X 0.65
1
12
PIN 1 ID 0.38
16X
16 13 0.23
0.1 C A B
0.65
16X 0.45 0.05
4219037/A 06/2019
NOTES:
1. All linear dimensions are in millimeters. Any dimensions in parenthesis are for reference only. Dimensioning and tolerancing
per ASME Y14.5M.
2. This drawing is subject to change without notice.
3. The package thermal pad must be soldered to the printed circuit board for thermal and mechanical performance.
www.ti.com

## Page 48

EXAMPLE BOARD LAYOUT
RGV0016A VQFN - 1 mm max height
PLASTIC QUAD FLATPACK - NO LEAD
( 2.16)
SYMM
16 13 SEE SOLDER MASK
DETAIL
16X (0.75)
12
16X (0.305) 1
17 SYMM
12X (0.65) (3.65)
(0.83)
4
9
(R0.05) TYP
( 0.2) TYP
VIA
5 8
(0.83)
(3.65)
LAND PATTERN EXAMPLE
EXPOSED METAL SHOWN
SCALE: 20X
0.07 MIN
0.07 MAX
ALL AROUND
ALL AROUND
METAL UNDER
METAL EDGE
SOLDER MASK
EXPOSED METAL
SOLDER MASK EXPOSED SOLDER MASK
OPENING METAL OPENING
NON SOLDER MASK
DEFINED SOLDER MASK DEFINED
(PREFERRED)
SOLDER MASK DETAILS
4219037/A 06/2019
NOTES: (continued)
4. This package is designed to be soldered to a thermal pad on the board. For more information, see Texas Instruments literature
number SLUA271 (www.ti.com/lit/slua271).
5. Vias are optional depending on application, refer to device data sheet. If any vias are implemented, refer to their locations shown
on this view. It is recommended that vias under paste be filled, plugged or tented.
www.ti.com

## Page 49

EXAMPLE STENCIL DESIGN
RGV0016A VQFN - 1 mm max height
PLASTIC QUAD FLATPACK - NO LEAD
(0.58) TYP
16 13
16X (0.75)
16X (0.305) 1 12
(0.58) TYP
SYMM 17
12X (0.65) (3.65)
4X (0.96)
4
9
(R0.05) TYP
5 8
4X (0.96)
SYMM
(3.65)
SOLDER PASTE EXAMPLE
BASED ON 0.125 MM THICK STENCIL
SCALE: 20X
EXPOSED PAD 17
79% PRINTED SOLDER COVERAGE BY AREA UNDER PACKAGE
4219037/A 06/2019
NOTES: (continued)
6. Laser cutting apertures with trapezoidal walls and rounded corners may offer better paste release. IPC-7525 may have alternate
design recommendations.
www.ti.com

## Page 50

IMPORTANT NOTICE AND DISCLAIMER
TI PROVIDES TECHNICAL AND RELIABILITY DATA (INCLUDING DATASHEETS), DESIGN RESOURCES (INCLUDING REFERENCE
DESIGNS), APPLICATION OR OTHER DESIGN ADVICE, WEB TOOLS, SAFETY INFORMATION, AND OTHER RESOURCES “AS IS”
AND WITH ALL FAULTS, AND DISCLAIMS ALL WARRANTIES, EXPRESS AND IMPLIED, INCLUDING WITHOUT LIMITATION ANY
IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT OF THIRD
PARTY INTELLECTUAL PROPERTY RIGHTS.
These resources are intended for skilled developers designing with TI products. You are solely responsible for (1) selecting the appropriate
TI products for your application, (2) designing, validating and testing your application, and (3) ensuring your application meets applicable
standards, and any other safety, security, regulatory or other requirements.
These resources are subject to change without notice. TI grants you permission to use these resources only for development of an
application that uses the TI products described in the resource. Other reproduction and display of these resources is prohibited. No license
is granted to any other TI intellectual property right or to any third party intellectual property right. TI disclaims responsibility for, and you fully
indemnify TI and its representatives against any claims, damages, costs, losses, and liabilities arising out of your use of these resources.
TI’s products are provided subject to TI’s Terms of Sale, TI’s General Quality Guidelines, or other applicable terms available either on
ti.com or provided in conjunction with such TI products. TI’s provision of these resources does not expand or otherwise alter TI’s applicable
warranties or warranty disclaimers for TI products. Unless TI explicitly designates a product as custom or customer-specified, TI products
are standard, catalog, general purpose devices.
TI objects to and rejects any additional or different terms you may propose.
IMPORTANT NOTICE
Copyright © 2026, Texas Instruments Incorporated
Last updated 10/2025
