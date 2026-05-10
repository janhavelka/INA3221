# An Engineer's Guide to Current Sensing (Rev. B)

- Source PDF: `../reference/ebook_current_sensing.pdf`
- Extraction tool: pdfplumber
- Page count: 64
- SHA256: `7db26ad631a8f27a58b55411f6e14f54c4e941eb463ef658912603debac62b58`

## Page 1

e-book
An Engineer’s Guide
to Current Sensing
A collection of technical content on current sensing designs
TTII..ccoomm//cteucrrhennotlsoegnisees/current-sensing-solutions.html

## Page 2

Table of contents
Introduction 3 with PWM Rejection 38
Current Mode Control in Switching Power
1. Current sense amplifier overview Systems 40
Introduction to Current Sense Amplifiers 4 Switching Power Supply Current Measurements 42
Integrating the Current Sensing Signal Path 6 Increase Measurement Accuracy using
Integrated, Current Sensing Analog-to-Digital High-Speed Amplifiers for Low-Side Shunt
Converter 8 Current Monitoring 44
Integrating the Current Sensing Resistor 11
5. Extending common-mode range and isolated
Common uses for Multi-Channel Current
current measurements
Monitoring 13
Extending Beyond the Max Common-Mode
2. O ut-of-range current measurements with Range of Discrete Current-Sense Amplifiers 46
current-sense amplifiers Interfacing a Differential-Output (Isolated)
Measuring Current to Detect Out-of-Range Amp to a Single-Ended Input ADC 48
Conditions 15 Low-Drift, Precision, In-Line Isolated Magnetic
Monitoring Current for Multiple Out-of-Range Motor Current Measurements 50
Conditions 17
High-Side Motor Current Monitoring for 6. Current Sensing in Mobile Robots
Overcurrent Protection 19
7. Current Sensing in Collaborative and Industrial
3. Current sense amplifier application examples
Robotic Arms
Shunt-Based Current-Sensing Solutions for
BMS Applications in HEVS and EVS 21
8. How to optimize PoE systems using discrete
12-V Battery Monitoring in an Automotive Module 23
current sensing
Simplify Voltage and Current Measurement
in Battery Test Equipment 25 Additional resources 52
Current Sensing Applications in Communication
Infrastructure Equpiment 28
Safety and Protection for Discrete Digital Outputs
in a PLC System using Current Sense Amplifiers 30
Current Sensing in High-Power USB Type-C®
Applications 32
4. Current sense amplifiers in switching systems
Low-Drift, Precision, In-Line Motor Current
Measurements with PWM Rejection 35
High-Side Drive, High-Side Solenoid Monitor
An Engineer’s Guide to Current Sensing 2 Texas Instruments Incorporated

## Page 3

Introduction
Designers have many options when addressing the Authors:
challenges associated with designing an accurate current- Scott Hill Maka Luo
measurement circuit for cost-optimized applications. Dennis Hudgins Raphael Puzio
Approaches range from using general-purpose operational Kurt Eckles Ben Damkroger
amplifiers to analog-to-digital converters, whether stand- Mubina Toa Kyle Stone
alone or embedded in a microcontroller. This expansive Zhao Tang Arjun Prakash
variety of approaches provides flexibility for designers of Guang Zhou Leaphar Castro
Dan Harmon Steven Loveless
current sensing applications – allowing them specifically to
Peter Iliya Tom Hendrick
address their unique design challenges.
Kevin Zhang
This e-book was created to further simplify the current
sensing design process by helping you quickly and efficiently
narrow down the list of potential devices that align best
with your particular system’s requirements. The current
sensing information featured in this e-book address specific
applications, focusing on identifying the most optimized
device to best serve the challenges faced in that particular
application and offer alternative solutions that may be
beneficial for other circuit optimizations.
Although this e-book is not an exhaustive collection of
current-sensing challenges, it does address many of the
more common and challenging functional circuits seen
today. If you have any questions about the topics covered
here or any other current-sensing questions, submit them to
the TI E2ETM design support forums Amplifiers forum.
An Engineer’s Guide to Current Sensing 3 Texas Instruments Incorporated

## Page 4

www.ti.com Introduction
Product Overview
Introduction to Current Sense Amplifiers
Introduction
What are Current Sense Amplifiers[unreadable glyph] Analog Output
Current sense amplifiers, also called current shunt monitors, are specialized differential
amplifiers with a precisely matched resistive gain network with the following characteristics:
+++
• Designed to monitor the current flow by measuring the voltage drop across a sense
element, typically a shunt resistor. 
• Tend to be easier to use, more precise, and less prone to noise.
• Support currents from 10s of μA to 100s of A.
Integrates the full analog signal
• Natively-support common-mode voltages from –16 to +80 V and with additional circuitry up processing and provides a voltage or
to 100s of volts. current output.
white
System Benefits Addressed by Using Current Sense Amplifiers
Digital Output
• Real-time overcurrent protection
• Current and power monitoring for system optimization
• Current measurement for closed-loop feedback
Integrates the full signal conditioning path
Key Parameters
and utilizes a standard two-wire digital
Common Mode Range This specification defines the DC voltage range at the input of interface.
an amplifier with respect to ground. Current sense amplifiers white
are typically designed to support common-mode voltages well
Comparator Output
beyond the chip supply voltage. For example, the INA240 is
capable of supporting a common-mode voltage between –4 V to 

+80 V while running on a supply as low as 2.7 V.white +++
+++
Offset Voltage This is a differential DC error at the input of the amplifier.
Historically, to reduce the impact of amplifiers with high
offsets, larger value shunt resistors were used to increase the
measured voltage drop. Today, TI is able to offer current sense
Provides a simple ALERT signal when the
amplifiers with offsets as low as 10 μV, enabling higher-precision
load current exceeds a threshold.
measurements at low currents and allowing the use of smaller
white
value shunt resistors for improved system efficiency.white
Integrated Shunt
Gain Current sense amplifiers come with various gain options
that have robust performance over temperature and process
+++
variations by integrating a precisely-matched resistive gain
network. The gain options for fixed gain amplifiers vary from

0.125 V/V to 1000 V/V with gain errors as low as 0.01%.white
Temperature Stability Current sense amplifiers integrate the amplifier along with
Offers a low-drift, precision-integrated
all the gain-setting resistors which enables small and unified
sense element.
temperature drift. This allows for robust current measurements
white
across the whole specified temperature range. The achieved
temperature stability is one of the key advantages current sense
amplifiers have over discrete implementations.
SBOA534 – FEBRUARY 2022 Introduction to Current Sense Amplifiers 1
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 5

Key Design Considerations www.ti.com
Key Design Considerations
High-Side Measurements
Sensing between supply bus and load.
System Advantages:
• Able to detect load short to ground
Bus
• Current is monitored directly from the source voltage
+5 V
• High immunity to ground disturbance
INA290
System Challenges: +5 V
+
• High bus voltage limits the availability of high input common-mode voltage devices  ADS114 I2C
Advantages Over Discrete Current Sense Circuit:
LOAD
• Integrated gain resistors provide excellent matching to enable higher performance
• Reduction in board space requirements
• Unique input architecture allows for the common-mode voltage to greatly exceed the device
supply voltage
Low-Side Measurements
Sensing between the load and ground.
System Advantages:
• Simple to implement and low-cost solution
• Wide range of available options Bus
voltage
System Challenges: LOAD
INA181
• Difficult to detect load short to ground
+5 V
• System ground disturbance by the shunt resistor +
 ADS114 I2C
Advantages Over Discrete Current Sense Circuit:
• Integrated gain resistors provide excellent matching enable higher performance
• Reduction in board space requirements
• Sense a true differential measurement across the shunt resistor
• Lower VOFFSET saves system power by enabling the use of smaller value shunt resistors to
achieve the same error level
In-line Measurements
Sense current in-line to the load.
System Advantages:
• True phase current at all times reduces phase-to-phase errors Bus
• Best current feedback for greatest accuracy
System Challenges: Supply
(2.7 V to 5.5 V)
• PWM common-mode voltage seen by amplifier M
INA240
• High common-mode voltage combined with high dV/dT poses steep challenge to many IN-
OUT
amplifiers IN+ REF2
REF1
INA240 Advantages Over Discrete Current-Sense Circuit:
• Enhanced PWM rejection provides high levels of suppression for large common-mode
transients (dV/dT) in systems that use PWM signals
2 Introduction to Current Sense Amplifiers SBOA534 – FEBRUARY 2022
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 6

Application Brief
Integrating the Current Sensing Signal Path
Scott Hill Current Sensing Products
Current measurements are used in electronic systems To optimize the current sensing signal chain, the
to provide feedback verifying operation is within shunt resistor value and amplifier gain must be
acceptable margins and to detect potential fault appropriately selected for the current range and full-
conditions. Analyzing a system’s current level can scale input range of the ADC. The selection of the
diagnose unintended or unexpected operating modes shunt resistor is based on a compromise between
allowing for adjustments to be made to improve measurement accuracy and power dissipation across
reliability or to protect the system components from the shunt resistor. A large value resistor will develop
damage. a larger differential voltage as the current passes
through. The measurement errors will be smaller due
Current is a signal that is difficult to measure directly.
to the fixed amplifier offset voltage. However, the
However, there are several measurement methods
larger signal creates a larger power dissipation across
that are capable of measuring the effect of flowing
the shunt resistor (P = I2R). A smaller shunt resistor
current. Current passing through a wire produces
develops a smaller drop across the shunt resistor
a magnetic field that can be detected by magnetic
reducing the power dissipation requirements but also
sensors (hall-effect and fluxgate for example). Current
increases the measurement errors as the amplifier’s
measurements can also be made by measuring the
fixed offset errors become a larger percentage of the
voltage developed across a resistor as current passes
signal.
through. This type of resistor is called a current
sensing, or shunt, resistor. The amplifier gain is selected to ensure that the
amplifier’s output signal will not exceed the ADCs full-
For current ranges reaching up to 100 amps on
scale input range at the full-scale input current level.
voltage rails below 100 volts, measuring current with
shunt resistors are typically preferred. The shunt The INA210 is a dedicated current sense amplifier
resistor approach commonly provides a physically that integrates the external gain setting resistors as
smaller, more accurate and temperature stable shown in Figure 2. Bringing these gain resistors
measurement compared to a magnetic solution. internal to the device allows for increased matching
and temperature drift stability compared to typical
For the system’s current information to be evaluated
external gain setting resistors. Space saving QFN
and analyzed, it must be digitized and sent to
packages significantly reduce the board space
the system controller. There are many methods
requirements of an operational amplifier and external
for measuring and converting the signal developed
gain resistors. Current sense amplifiers are commonly
across the shunt resistor. The most common
available in multiple fixed gain levels to better
approach involves using an analog front-end to
optimize the pairing with shunt resistor values based
convert the current sensing resistor’s differential
on the input current and ADC full-scale input ranges.
signal to a single-ended signal. This single-ended
signal is then connected to an analog to VCM = VS =
0V to 26V 2.7V to 26V
digital converter (ADC) that is connected to a
microcontroller. Figure 1 illustrates the current sensing
signal chain.
VOUT
SUPPLY POWER
SUPPLY
+ LOAD
RSHUNT ADC CONTROLLER
-
LOAD REF
REF
Figure 1. Current Sensing Signal Path
R
TNUHS
www.ti.com
INA210
+
-
Figure 2. INA210: Current Sensing Amplifier
SBOA167B – DECEMBER 2016 – REVISED SEPTEMBER 2021 Integrating the Current Sensing Signal Path 6
Submit Document Feedback
Copyright © 2021 Texas Instruments Incorporated

## Page 7

Figure 1 shows the operational amplifier measuring
the differential voltage developed across the shunt
resistor and sending the amplified signal to the
single ended ADC. A fully differential input ADC can
monitor the differential voltage directly across the
shunt resistor. One drawback to using a typical ADC
is reduced input range used. The signal developed
LOAD
across a shunt resistor will be small to limit the power
dissipation requirements of this component. Lower
ADC resolutions will also impact the small signal
measurement accuracy.
The ADC reference will also be an additional error
source that must be evaluated in this signal path. A
typical ADC will feature an input range that is based
on the converter's reference voltage. The actual
reference voltage range varies from device to device
but is typically in the 2 V to 5 V range. The LSB
(least significant bit) is based on the full-scale range
and resolution of the converter. For example, a 16-bit
converter with a full-scale input range of 2.5 V, the
LSB value is roughly 38 μV.
The INA226 is a specialized ADC designed
specifically for bi-directional current sensing
applications. Unlike typical ADCs, this 16-bit converter
features a full-scale input range of +/- 80 mV
eliminating the need to amplify the input signal
to maximize the ADC's full-scale input range. The
INA226 is able to accurately measure small shunt
voltages based on the device's maximum input offset
voltage of 10 μV and an LSB size of 2.5 μV. The
INA226 provides 15 times more resolution than the
equivalent standard 16-bit ADC with a full-scale input
range of 2.5 V. The specialization of the INA226
makes this device ideal for directly monitoring the
voltage drop across the current sensing resistor as
shown in Figure 3.
R
TNUHS
Alert GPIO
I2C
Interface
INA226
RELlORTNOC
www.ti.com
Supply
2.7V to 5.5V
VCM =
0V to 36V
16-Bit SDA
ADC
SCL
Figure 3. Digital Current/Power Monitor
In addition to the ability to directly measure voltage
developed across the shunt resistor as current passes
through, the INA226 can also measure the common-
mode voltage. The INA226 has an input multiplexer
allowing the ADC input circuitry to switch between
the differential shunt voltage measurement and the
single-ended bus voltage measurement.
The current sensing resistor value present in the
system can be programmed into a configuration
register on the INA226. Based on this current
sensing resistor value and the measured shunt
voltage, on-chip calculations convert of the shunt
voltage back to current and can provide a direct
readout of the corresponding power level of
the system. Performing these calculations on-chip
reduces processor resources that would normally be
required to convert this information.
Alternate Device Recommendations
For applications with lower performance
requirements, using the INA199 still takes advantage
of the benefits of the dedicated current sense
amplifier. For applications implementing over-current
detection, the INA301 features an integrated
comparator to allow for on-chip over-current detection
as fast as 1μs. For applications with lower
performance requirements, using the INA219 is able
to take advantage of the specialized current sensing
ADC.
Table 1. Alternative Device Recommendations
Device Optimized Parameter Performance Trade-Off
INA199 Lower Cost Higher VOS and Gain Error
INA301 Signal Bandwidth, On-Board Comparator Larger Package: MSOP-8
INA219 Smaller Package Digital Monitor, Lower Cost Higher VOS and Gain Error
INA190 More Accurate N/A
Table 2. Related TI Application Briefs
SBOA162 Measuring Current To Detect Out-of-Range Conditions
SBOA165 Precision Current Measurement On High Voltage Power Rail
SBOA160 High Precision, Low-Drift In-Line Motor Current Measurements
SBOA161 Low-Drift, Low-Side Current Measurements for Three-Phase Systems
7 Integrating the Current Sensing Signal Path SBOA167B – DECEMBER 2016 – REVISED SEPTEMBER 2021
Submit Document Feedback
Copyright © 2021 Texas Instruments Incorporated

## Page 8

www.ti.com
Application Brief
Integrated, Current Sensing Analog-to-Digital Converter
Scott Hill Current Sensing Products
The signal chain path for measuring current is configurations are common for this functional
typically consistent from system to system. Whether requirement. Dedicated current sense amplifiers,
current is measured in a computer, automobile or INA210 for example, feature integrated gain setting
motor, the common functional blocks used are found components and are designed specifically for this
in nearly all equipment. type of application. The INA210 has the capability to
accurately measure very small signals reducing the
The interface to a real world element such as light,
power dissipation requirement of the sensing resistor.
temperature or current in this case, requires a sensor
to convert the signal to a proportional value (voltage The next signal chain block is the ADC which is
or current) that can be more easily measured. There present to digitize the amplified sensor signal. This
are a several sensors that use magnetic field sensing device can require additional external components
for detecting the effects of current flow. These can (reference, oscillator) for more precise measurement
be very effective for detecting very large currents or capability. Similarly to the AFE, there are various
when isolated measurements are required. The most options available for the ADC block. Stand-alone
common sensor for measuring current is a current converters with the onboard references and oscillators
sensing, or shunt, resistor. Placing this component in are available as well as processors featuring onboard
series with the current being measured develops a ADC channels.
proportional differential voltage as the current passes
Both integrated and discrete ADC blocks have their
through the resistor.
benefits as well as limitations. Fewer components
The remaining blocks in the signal path are selected on the board is one obvious advantage with the
based on how this measured current information is ADC being integrated into the processor. Existing
to be used by the system. There are several blocks instruction sets for the onboard ADC channels further
that are common and found in most applications as reduces the requirement for additional software
shown in Figure 1. These blocks consist of an analog to be written to support a stand-alone ADC.
front end (AFE) to amplify a small signal from the However, silicon process nodes for digital controllers
sensor, an analog-to-digital converter (ADC) to digitize frequently are less optimized for precision analog,
the amplified signal from the sensor, and a processor limiting the performance capability of the onboard
to allow for the sensor information to be analyzed so converter. Discrete analog-to-digital converters have
the system can respond accordingly to the measured an advantage of allowing device selection based on
current level. optimized performance attributes such as resolution,
noise or conversion speed.
Power
Supply A variation in this signal chain is to use an ADC
AFE to measure directly across the current sensing
+ resistor eliminating the current sense amplifier
R SHUNT ADC Controller entirely. A standard converter would have challenges
-
in replacing the AFE and measuring the shunt
voltage directly. One challenge is the large full-scale
Load
OSC REF range of the ADC. Without the amplification of the
sense resistor's voltage drop, either the full-range
Figure 1. Current Sensing Signal Chain of the ADC cannot be fully utilized or a larger
voltage drop will be needed across the resistor.
One requirement for the AFE is allow for direct A large voltage drop will result in a larger power
interface to the differential signal developed across dissipation across the sensing resistor. There are
the sense resistor. A single-ended output for ADCs available with modified input ranges designed
the AFE simplifies the interface to the following for measuring smaller signals directly which can
ADC. Operational amplifiers in differential amplifier allow for direct measurement of shunt voltages.
SBOA179A – DECEMBER 2016 – REVISED SEPTEMBER 2021 Integrated, Current Sensing Analog-to-Digital Converter 8
Submit Document Feedback
Copyright © 2021 Texas Instruments Incorporated

## Page 9

www.ti.com
An internal programmable gain amplifier (PGA) is While the INA226 is able to accurately measure small
typically integrated in these devices to leverage the shunt voltages this component has been designed
full-scale range of the ADC. with additional functionality useful for current sensing
applications. This device features an internal register
A limitation these small signal converters have is their
that is user-programmable with the specific value
limited common-mode input voltage range. These
of the current sensing resistor that is present on
ADCs have input voltage ranges that are limited
the PCB. Having the value of the current sensing
by their supply voltage which typically range from
resistor allows the INA226 to directly convert the
3 V to 5.5 V based on the core processor voltage
shunt voltage measured every conversion to the
being supported. The INA226, shown in Figure 2, is
corresponding current value and stores this to an
a current sensing specific analog-to-digital converter
additional output register. The INA226 also features
that solves this common-mode limitation. This device
an internal multiplexer allowing the device to switch
features a 16-bit delta-sigma core and can monitor
from a differential input measurement to a single
small differential shunt voltages on common-mode
ended voltage configuration to allow for measurement
voltage rails as high as 36 V while being powered off
of the common-mode voltage directly. The voltage
a supply voltage that can range from 2.7 V to 5.5 V.
measurement, along with the previously measured
Power Supply shunt voltage and corresponding current calculation,
(0V to 36V)
allows the device the capability of computing power.
INA226 The device stores this power calculation and provides
Bus
Voltage Shunt this value along with the shunt voltage, current and
16-Bit Bus I2C common-mode voltage information to the processor
Current
RSHUNT
V
S
o
h
lt
u
a
n
g
t
e
ADC Power Interface over the two-wire serial bus.
Alert
In addition to the on-chip calculations of current and
Load power, the INA226 features a programmable alert
register that allows the device to compare each
conversion value to a defined limit to determine
Figure 2. INA226, Precision Current, Voltage,
if an out-of-range condition has occurred. This
Power Sensing ADC
alert monitor can be configured to measure out-of-
Similar to the ADCs with modified small input range range conditions such as overcurrent, overvoltage, or
capability, the INA226 has a full-scale input range of overpower. The device also includes programmable
about 80 mV enabling the device to measure directly signal averaging to further improve measurement
across the current sensing resistor. The INA226 has accuracy.
the ability to very accurately resolve small current
The INA226 is optimized to support precision current
variations with an LSB step size of 2.5 μV and
measurements. The additional features included in
a maximum input offset voltage of 10 μV. A 0.1-
this device provide the capability of supporting the
μV/°C offset drift ensures the measurement accuracy
signal management and monitoring needed in this
remains high with only an additional 12.5 μV of offset
current measurement function reducing the burden on
induced at temperatures as high as 125°C. A 0.1%
the system processor.
maximum gain error also enables the measurement
accuracy to remain high at the full-scale signal levels
as well.
9 Integrated, Current Sensing Analog-to-Digital Converter SBOA179A – DECEMBER 2016 – REVISED SEPTEMBER 2021
Submit Document Feedback
Copyright © 2021 Texas Instruments Incorporated

## Page 10

www.ti.com
Alternate Device Recommendations Table 1. Alternative Device Recommendations
Device Optimized Parameters Performance Trade-Off
For applications with lower performance
requirements, using the INA234 still leverages INA234 Lower Cost Higher VOS & Gain Error,
the benefits of the dedicated current sensing
Lower VCM Range
analog-to-digital converter. For additional precision INA260 Lower System Level Gain Larger Package:
Error & Offset TSSOP-16
measurement capability where currents being
measured are less than 15 A, the INA260 provides AMC1305
Isolated Measurement, Higher Cost, Higher VOS
Higher Signal Bandwidth & Gain Error
similar functionality to the INA226 while also
featuring a precision 2-mΩ integrated current sensing
INA210 Lower Cost Higher VOS & Gain Error
resistor inside the package. For applications requiring
Table 2. Adjacent Tech Notes
significantly higher common-mode voltage capability,
Measuring Current To Detect Out-of-
the AMC1305 provides onboard isolation and is SBOA162
Range Conditions
capable of supporting working voltages as high as 1.5
Precision Current Measurement On High
kV DC and handling peak transients as high as 7 kV. SBOA165
Voltage Power Rail
For applications with lower performance requirements
Integrating The Current Sensing Signal
for the AFE, use the INA210 to take advantage of the SBOA167
Path
benefits of a dedicated current sense amplifier.
SBOA170 Integrating the Current Sensing Resistor
SBOA179A – DECEMBER 2016 – REVISED SEPTEMBER 2021 Integrated, Current Sensing Analog-to-Digital Converter 10
Submit Document Feedback
Copyright © 2021 Texas Instruments Incorporated

## Page 11

www.ti.com
Application Brief
Integrating the Current Sensing Resistor
Scott Hill, Current Sensing Products
Current is one of the most common Amplifiers have fixed inherent errors associated with
signals used for evaluating and diagnosing them, input offset voltage for example, that impact
the operational effectiveness of an electronic the measurement accuracy. As the input signal
system. However, measuring this signal directly is increases, the influence of these internal errors on
very challenging. Instead, many types of sensors are the total measurement accuracy decreases. When
used to measure the proportional effects that occur the input signal decreases the corresponding
due to current flowing throughout the system. measurement error is a higher . This relationship
between the signal level and the acceptable
The most common sensing element used for
measurement accuracy provides general lower limits
detecting current flowing in a system is a
for the current sensing resistor selection. The upper
resistor. Placing a resistor, called a shunt, in series
limit value for the current sensing resistor should be
with the current path develops a differential voltage
limited based on an application’s acceptable power
across the resistor as current passes through it.
loss for this component.
One common signal chain configuration for monitoring
One benefit of using resistors for current
a current signal involves an analog front-end (AFE),
measurement is the availability of accurate
an analog to digital converter (ADC), and a system
components that provide both high precision
controller as shown in Figure 1. An AFE, such
and temperature stable measurements. Precision
as an operational amplifier or dedicated current
current sensing amplifiers are available featuring
sense amplifier, converts the small differential voltage
measurement capabilities optimized for interfacing
developed across the shunt resistor to a larger output
with very small signals to accommodate small value
voltage that the ADC can digitize before sending
resistors and low power losses.
the information to a controller. The system controller
uses the current information to optimize the system's There are two trends for resistors as the ohmic
operational performance or reduce functionality in the value decreases into the single digit milliohm level
event of an out-of-range condition to prevent potential and below. One trend for this segment of resistors
damaging conditions from occurring. is the reduced package availability and resistor
value combinations. The other trend is the increased
cost for precision and low temperature coefficient
components. Pairing a low ohmic, low temperature
coefficient current sensing resistors with precision
+
ADC CONTROLLER tolerance levels (~0.1%) result in solution costs in
the several dollar range without including the cost
associated with the precision amplifier.
A component such as the INA250, shown in
Figure 2, helps reduce the challenges of selecting
these increased accuracy, higher cost resistors for
Figure 1. Current Sensing Signal Chain
applications needing precise and temperature stable
measurements. This device pairs a precision, zero-
The proper resistance value selection is critical in
drift, voltage output current sense amplifier with
optimizing the signal chain path. The resistance
a 2mΩ integrated current sensing resistor with a
value and corresponding voltage developed across
0.1% maximum tolerance and a temperature drift of
the shunt results in a system power loss. To limit
15ppm/°C over the device's entire temperature range
the power loss, it is preferred to minimize the shunt
of -40°C to +125°C. This device can accommodate
resistance. The resistor value is directly proportional
continuous currents flowing through the on-board
to the signal developed and sent to the current
resistor of up to 15A.
sensing amplifier.
SBOA170C – JULY 2018 – REVISED MARCH 2022 Integrating the Current Sensing Resistor 11
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 12

Supply
Voltage
VS INA250
Load
REF OUT
+ -
www.ti.com
0V to 36V Supply
0V to 36V IN+ VBUS INA260 SDA
Supply SDA
IN+ IN- V SCL SCL
I2C
GPIO
ADC Interface Alert
A0
I A1
ADC Controller
Reference IN- GND
Voltage
ADC Load
GND
Controller
Figure 3. Integrated Signal Path
Figure 2. Integrating The Current Sensing Resistor Pairing the precision, low-drift current sensing with
these precision current sensing devices provides
In addition to the integrated precision resistor inside
measurement solutions that are challenging to
this device, the INA250 also addresses one of the
accomplish using discrete amplifier and resistor
most common issues associated with implementing a
combinations. There are few catalog current sensing
current sensing solution. A low-ohmic shunt resistor is
resistors available that are capable of enabling the
used to reduce the current sensing power dissipation.
combination of precision and temperature stable
A challenge in accommodating this low resistance
measurements but achieving this level of accuracy in
value is the potential impact of parasitic resistance on
a solution size comparable to TSSOP-16 packaged
the PCB. Parasitic resistance in series with the shunt
integrated solutions doesn't exist.
resistor can cause additional measurement errors as
current flows through the resistance to create the Alternate Device Recommendations
shunt voltage. The most common source for these
For additional design flexibility, many stand-alone
measurement errors is poor layout techniques. A
current sensing amplifiers and digital power monitors
Kelvin connection, also known as a four terminal
are also available. For lower performance applications
connection or a force-sense, is required to ensure that
with higher current requirements than the integrated
minimal additional resistance is present to alter the
solutions support, use the INA210 stand-alone current
differential voltage developed between the amplifier's
sensing amplifier. For applications requiring a stand-
input pins. There are PCB layout techniques to
alone digital power monitor, use the INA226. For
reduce the effect of parasitic resistance, however, this
applications implementing over-current detection, the
concern is removed with the INA250.
INA301 features an integrated comparator for on-chip
For applications that require measuring current in a over-current detection as fast as 1μs.
high dv/dt common mode transients like motor control
Table 1. Alternative Device Recommendations
and solenoid control, the INA253 is specifically design
Device Optimized Parameter Performance Trade-Off
to reject PWM signals with a settling time of <10μs.
INA210
35μV VOS, Package: No on-boad current
As previously described, the typical current sensing SC70-6, QFN-10 sensing resistor
signal chain path includes the current sensing resistor,
INA226
10μV VOS, Package: No on-boad current
the analog front-end, ADC and system controller. The MSOP-10 sensing resistor
INA250 combines the shunt resistor and the current Signal Bandwidth, On- No on-boad current
INA301
sensing amplifier. The INA260 combines the current Board Comparator sensing resistor
sensing resistor, measurement front-end and the ADC
into one single device.
Related Documentation
Figure 3 shows the INA260 featuring the same
1. Integrated-Resistor Current Sensors Simplify PCB
precision, integrated sensing resistor, pairing it with
Design
a 16-bit, precision ADC optimized for current sensing
2. Precision Current Measurements on High-Voltage
applications. This combination provides an even
Power Supply Rails
higher performance measurement capability than the
3. Integrating the Current Sensing Signal Path
INA250 resulting in a maximum measurement gain
4. Precision, Low-Side Current Measurement
error of 0.5% over the entire temperature range and a
maximum input offset current of 5mA.
12 Integrating the Current Sensing Resistor SBOA170C – JULY 2018 – REVISED MARCH 2022
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 13

www.ti.com
Application Brief
Common Uses for Multi-Channel Current Monitoring
Dennis Hudgins, Current Sensing Products
As the need for system intelligence and power devices are also more flexible in that they can monitor
efficiency continues to grow, the need for better voltage drops across resistors that have voltages
monitoring of critical system currents is increasingly greater than the supply voltage.
paramount. In the past this may have been
In addition to simplifying the design process and
done with multiple operational amplifiers configured
reducing the number of external components,
as difference amplifiers or multiple current sense
having multiple current monitoring devices in a
amplifiers distributed within the system. As the
single package facilities several common application
number of current monitor channels increases so
solutions.
does the amount of external components needed to
realize a solution. The added components increase For example, consider the application shown in Figure
the design complexity and solution size, and can 2, where the total current drawn by the memory and
degrade the overall current sensing accuracy. processor is monitored by an external ADC.
For example, consider the case where two currents +12V
need to be measured as shown in Figure 1.
DC/DC IN+ INA180
Power Power
Supply Supply
RSENSE1 + OUT

LOAD1 LOAD1
OP-AMP1 INA2180 Dual Current
IN
Sense Amplifier Memory
OUT1
+  OUT1 RSENSE1 M 2 U :1 X ADC
DC/DC IN+ INA180
Control
LOAD2 LOAD2
RSENSE1 + OUT
OP-AMP2 
IN
RSENSE2  + OUT2 RSENSE2 OUT2 L O o t a h d e s r Processor
Figure 2. Monitoring Total Current in Two Supply
Rails
Figure 1. Discrete vs. Integrated Current Sensing
Solutions One approach would be to monitor both the CPU
and memory current, multiplex the current to an
In this case, the operational amplifier-based solution
ADC and then and add the resulting values together
requires 8 resistors to set the gain, 2 bypass
in a microprocessor. This approach requires some
capacitors and 2 current sense resistors. The same
mathematical processing as well as an ADC to
circuit implemented with an INA2180 only requires
continually sample outputs at a fast enough rate to
the 2 current sense resistors and a single bypass
be effective. A better approach would be to use the
capacitor. Since the integrated gain-set resistors are
REF pin of the INA2181 to add the current drawn by
well matched, the accuracy of the INA2180 solution
the memory to the current drawn by the CPU. This
is much better than what can be achieved in a
can be done by connecting the output of channel 1
cost effective discrete implementation. The integrated
that monitors the memory current to the REF2 pin as
gain-set resistors permit higher accuracy monitoring
shown in Figure 3.
or allow use of a cheaper wider tolerance current
sense resistor. The INA2180, INA2181, and INA2290
SLYA024A – DECEMBER 2017 – REVISED SEPTEMBER 2021 Common Uses for Multi-Channel Current Monitoring 13
Submit Document Feedback
Copyright © 2021 Texas Instruments Incorporated

## Page 14

www.ti.com
+12V Power
Supply
INA2181
INA2181 IN+1 REF1 VREF1
IN+1 REF1
DC/DC + OUT1
RSENSE

RSENSE +

OUT1
IN1
Memory IN1 LOAD
IN+2 REF2
DC/DC IN+2 REF2
+ OUT2
RSENSE

ADC
RSENSE + OUT2 ADC
 IN2
L O o t a h d e s r GND
Processor
IN2
GND VOUT2 = VREF1 if there is no leakage current
Figure 4. Current Subtraction Using the INA2181,
VOUT2 = (ILOAD1 + ILOAD2) × RSENSE × GAIN
for Leakage Current Detection
Figure 3. Analog Current Summing With INA2181
If the voltage at OUT2 is equal to the applied
The channel 2 output will be the amplified sum of reference voltage then no leakage path exists. If
the currents from the CPU and memory. The current V is higher than the applied reference voltage
OUT2
from the memory and the current from the total can then there is unexpected current leaving the load.
be monitored when desired by an ADC. However, Likewise if V is below the reference voltage then
OUT2
since the channel 2 output is an analog signal, a unexpected leakage current is entering the load. As
comparator with an appropriately set reference can before, for this circuit to function properly the values of
be used to interrupt the system when an over current the current sense resistors must be the same.
condition occurs. For this circuit to function properly
Texas Instruments offers several solutions for
the values of the two sense resistors must be the
multichannel current monitoring. To monitor 4
same.
channels the INA4180, INA4181, and INA4290
Another convenient use for multi-channel current devices are available with an analog voltage output.
monitors is to detect unexpected leakage paths. The INA3221 provides the ability to accurately
These leakage paths could be caused by unintended measure both system current and bus voltages for up
shorts to ground or some other potential not in the to 3 independent channels. The values of the currents
current measurement path. One technique to detect and voltages are reported through an I2C interface.
leakage current paths is to monitor all current going
Table 1. Alternative Device Recommendations
into and coming out of a circuit. As long as there
are no unexpected leakage paths, the current into the DEVICE OPTIMIZED PARAMETERS PERFORMANCE TRADE­
OFF
load must equal the current coming out. To detect
4-channel analog current
leakage currents all current in and out of a circuit INA4180 Unidirectional Measurement
monitor
should be monitored. If the currents in and out are
Bidirectional 4-channel
INA4181 Larger package
equal, no unexpected current leakage path will be current monitor
detected. Use of the dual current monitor provides 3-channel digital current/
INA3221 No analog output
a simple technique to detect leakage current paths voltage monitor
without the need for multiple devices or the need to INA2290 2-channel, 120-V analog Unidirectional Measurement
current monitor
externally add or subtract currents. The circuit shown
4-channel, 120-V analog
in Figure 4, uses the INA2181 to monitor the current INA4290 Unidirectional Measurement
current monitor
into and out of a load.
Table 2. Adjacent Tech Notes
By reversing the polarity of the resistor connections of
LITNUMBER TITLE
the second amplifier and connecting the output of the
Measuring Current To Detect Out-of-Range
first amplifier to the second amplifier, the current going SBOA162
Conditions
in to the load is subtracted from the current going out.
SBOA169 Precision, Low-Side Current Measurement
SBOA190 Low-Side Current Sense Circuit Integration
14 Common Uses for Multi-Channel Current Monitoring SLYA024A – DECEMBER 2017 – REVISED SEPTEMBER 2021
Submit Document Feedback
Copyright © 2021 Texas Instruments Incorporated

## Page 15

Application Brief
Measuring Current To Detect Out-of-Range Conditions
Scott Hill, Current Sensing Products
The amount of current flowing throughout a system
provides insight in determining how effectively the
system is operating. A basic insight into the system's
operation is a comparison between the current being
pulled from a power supply and a pre-defined target
range for that particular operating condition. Current
levels exceeding the expected current level indicate
that an element in the system is consuming more
power than expected. Likewise, if the current is lower
than expected it may indicate some part of the system
is not powered correctly or possibly disconnected.
There are multiple methods available to diagnose fault
conditions in a system depending on how the out-of-
range indication is intended to be used. One method
is to monitor an entire system's current consumption
to identify potentially damaging excursions for the
power supply. In this case, measurement accuracy
is typically not critical and requires a simple alert to
indicate an out-of-range condition.
Fuses are commonly used for short-circuit protection
preventing damaging levels of current from flowing
in the system. In an out-of-range event the fuse will
blow and break the circuit path. The fuse must be
replaced for the system to operate correctly again. In
worst case situations the system requires delivery to a
repair facility if the fuse is not easily accessible.
There is a time-current dependency that limits the
effectiveness of a fuse in responding to a specific
current threshold. An example time-current response
of a fuse is shown in Figure 1.
10,000
1,000
100
10
1
0.1
0.01
1 10 100 1,000 10,000
CURRENT (A)
)s(
EMIT
Another overcurrent protection scheme becoming
more common is to allow the system to protect
itself when an excursion is detected but then enable
the system to return to normal operation once the
fault condition has been cleared. This protection
method uses a comparator comparing the monitored
operating current levels to defined thresholds, looking
for out-of-range conditions. Creating the necessary
level of detection for a particular application relies
on system specific variables such as the adjustability
of the desired over-range threshold, the amount of
margin acceptable in the threshold level and how
quickly the excursion must be detected.
The INA300 is a specialized current sensing
comparator with the ability to perform the basic
comparison to expected operating thresholds required
for out-of-range detection.
Power Supply +2.7 V to 5.5 V (0 V to 36 V)
VS RPULL-UP
IN+
CMP
IN-
LIMIT
Load
20A Fuse
Rating
45A Fuse
Rating
100A Fuse
Rating
Figure 1. Typical Time-Current Fuse Curve
TUPNI
TUPTUO
THRESHOLD
RLIMIT
R
TNUHS
www.ti.com Application Brief
INPUT SIGNAL
ALERT
ALERT
Figure 2. INA300 Over-Current Comparator
Figure 2 shows the INA300 measuring the differential
voltage developed across a current sensing resistor
and the comparison to a user-adjustable threshold
level. The alert output is pulled low when the
threshold level is exceeded. The INA300’s alert
response is issued following a current excursion in as
short as 10μs.
There may also be a need to provide information
on how much current is actually being pulled by the
supply or a particular load in addition to the fault
indication. For these requirements a typical approach
is to utilize a combination of a current sense amplifier
and a stand-alone comparator as shown in Figure 3.
SBOA162D – MAY 2018 – REVISED MARCH 2022 Measuring Current To Detect Out-of-Range Conditions 15
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 16

SUPPLY
POWER
SUPPLY
+ RPULL-UP
-
RSHUNT - + GPIO
LOAD REF
RELlORTNOC
Figure 3. Discrete Over-Current Detection
The current sense amplifier measures the differential
voltage developed across the sense resistor and
sends the output to both the comparator input and
analog to digital converter (ADC).
The INA301 and INA381 combines both the current
sense amplifier (providing a voltage output signal
proportional to the measured input current) and an
on-board comparator (for over-current detection) into
one device as shown in Figure 4.
Power Supply 0V to 36V 2.7V to 5.5V
IN+ VS RPULL-UP
IN- VOUT Load CMP ALERT
LIMIT
TUPNI THRESHOLD
ALERT
TUPTUO
RLIMIT
VOUT
R
TNUHS +
Power Supply
0V to 26V 2.7V to 5.5V
IN+ VS RPULL-UP IN- VOUT
Load CMP ALERT CMPREF
R TNUHS
Application Brief www.ti.com
The INA301 provides a combination of performance
capability in addition to the integration of both
the current sensing amplifier as well as on-board
comparator. The INA301 amplifier's has a small
signal bandwidth of 450 kHz at a fixed gain of
100 (gains of 20 and 50 are also available) and a
maximum input offset voltage of 35 μV. In addition
to the maximum gain error specification of 0.2%, the
amplifier's ability to detect the out-of-range condition
is fast. The INA301 is able to achieve accurate input
measurements and quickly respond to over-current
events with a response time including the input signal
measurement, comparison to the user-selected alert
threshold, and assertion of the comparators output in
less than 1μs.
Alternate Device Recommendations
For applications needing to monitor current on
voltage rails that are higher than the INA301's range
INA301 of 36V with the on-board over-current detection,
use the INA200. The INA180 is a current sense
amplifier that is commonly used in the discrete over-
current detection circuit using an external comparator.
INA381 + The INA381 provides a cost effective solution for
over current detection in applications that feature a +5 V
common mode range less than 26 V. For applications
requiring monitoring of a second fault threshold
Figure 4. Integrated Over-Current Detection with
level, the INA302 features an additional out-of-range
INA301 and INA381
comparator with dedicated adjustable threshold level.
With both the current information and an out-of-range Table 1. Alternate Device Recommendations
indicator the system may utilize multiple monitoring
Device Optimized Parameter Performance Trade-off
and protection schemes based on the operating
Package: SC70-5, Reduced bandwidth,
conditions. One scheme used with this device is to INA180
SOT23-5 analog output only
initially monitor only the alert indicator as a fault
Reduced bandwidth,
indicator. Once an out-of-range condition is detected INA381 Cost, Flexibility Common mode voltage
and the alert pin is asserted, the system then begins
Common-mode Voltage
actively monitoring the analog output voltage signal INA200 Range: -16 V to +80 V Reduced accuracy
allowing the system to respond accordingly. The
Two Independent Alert Larger Package:
system response typically will be to reduce system INA302 Comparators TSSOP-14
performance level, shut down entirely or to continue
monitoring to determine if the excursion continues to
become a more significant system concern. Having Related Documentation
both the proportional output voltage as well as the
1. Low-Drift, Low-Side Current Measurements for
on-board over-current detection function allows the
Three Phase Systems
system to only actively monitor the current information
2. High-Side Motor Current Monitoring for Over-
when necessary optimizing system resources. The
Current Protection
INA381 is similar in function to the INA301 except
3. Integrating The Current Sensing Signal Path
both inputs to the over-current comparator are directly
4. Monitoring Current for Multiple Out-of-Range
accessible for greater flexibility when setting the over
Conditions
current trip threshold.
5. Safety and Protection for Discrete Digital Outputs
in a PLC System Using Current Sense Amplifiers
16 Measuring Current To Detect Out-of-Range Conditions SBOA162D – MAY 2018 – REVISED MARCH 2022
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 17

Design Guide: INA300 INA301 INA302 INA303
Monitoring Current for Multiple Out-of-Range Conditions
Dennis Hudgins, Current Sensing Products
One of the first parameters engineers look at when
determining proper operation of a PCB design is
the operating current. By examining the operating
current, an engineer can immediately tell if something
on the board is shorted, whether any of the devices
are damaged, and in some cases detect if the
software is running as expected. The traditional
approach of using a current sense amplifier + ADC
to monitor current for out-of-range conditions does not
provide the required alert response time. Also, use
of an ADC to monitor over current alert thresholds
requires constant communication between the ADC
and host processor which can unnecessarily burden
the system.
Being able to quickly know when a current level is
out of range allows improved safely, improved system
intelligence/diagnostics and reduced downtime.
To address the response time required for out-of-
range current conditions, analog comparators are
needed to detect when the current exceeds a given
reference threshold. However, in many cases having
only one alert level is insufficient to determine
the system status and provide appropriate system
responses to out-of-range currents. To handle this
requirement the circuit shown in Figure 1-1 can
be used to monitor multiple out-of-range current
conditions.
SUPPLY
POWER SUPPLY
+
RSHUNT -
LOAD -
+ GPIO2
RELlORTNOC
too fast of a response time can trigger false alerts,
possibly resulting in system shutdown.
Figure 1-2 shows a simpler circuit that addresses the
design issues present in the discrete implementation.
REF1
+
- ALERT1 GPIO1
SUPPLY
ALERT2
REF2
Figure 1-1. Discrete Implementation to Detect
Multiple Over-current Events
This circuit is composed of 5 devices: a current sense
amplifier, two comparators, and two references. The
discrete implementation, shown in Figure 1-1 requires
careful selection of the comparators to get the desired
alert response time. Slow response times may not
allow enough time for the system to take action; while
TUPNI
TUPTUO
www.ti.com
Power Supply 0V to 36V 2.7V to 5.5V
VS Supp R ly PULL-UP Fault Threshold
CMP A L L IM ER I T T1 1 RLIMIT 1 Warning Threshold
LATCH1
IN+ + OUT INPUT SIGNAL
IN- Supply
RPULL-UP VOUT
Load CMP A LI L M ER IT T 2 2 RLIMIT 2 ALERT1
DELAY ALERT2
GND LATCH2 CDELAY
tDELAY
Figure 1-2. INA302 Multi-Alert Over-Current
Comparator
The INA302 incorporates the ability to detect two out-
of-range conditions. The lower out-of-range condition
is referred to as the over current warning threshold,
while the higher out-of-range condition is referred
to as the over current fault threshold. The over
current warning threshold allows detection when the
current is starting to get too high but has not yet
reached the fault threshold where system shutdown
may be initiated. When the current exceeds the
warning threshold, the system may opt to reduce the
system power consumption by disabling sub-circuits,
controlling supply voltages, or reducing clocking
frequencies to lower the total system current to
prevent a fault condition. If an over-current fault
condition does occur, it is important to respond quickly
to prevent further system damage or malfunctioning
behavior.
To minimize the component count and facilitate ease
of use, the alert thresholds of the INA302 are set
with single external resistors. The fault threshold
should be set higher than the worst case current
the system could be expected to consume. When
the current exceeds this threshold, the alert pin of
the INA302 will respond within 1 μs. The value for
the warning threshold is application dependent, but is
usually higher than the nominal operating current. The
response time of the warning threshold is adjustable
with an external capacitor from 3 μs to 10 s.
By setting the warning threshold delay time
appropriately, it is possible to set the over-current
SBOA168C – MAY 2018 – REVISED OCTOBER 2021 Monitoring Current for Multiple Out-of-Range Conditions 17
Submit Document Feedback
Copyright © 2021 Texas Instruments Incorporated

## Page 18

warning threshold closer to the maximum DC
operating current while still avoiding false trips due
to brief current spikes or noise. Wider separation
between the fault and warning thresholds provides the
system additional time for preventative action before
the fault threshold is exceeded.
Some systems allow operation above the warning
threshold for a period of time before triggering an
alert. One such application is monitoring the supply
current to a processor. The processor may be
allowed to operate above the normal maximum
current level for a brief period of time to maximize
computing throughput during critical operations. If the
current is above the warning threshold when the set
delay expires the alert output will pull low to notify the
host processor so the voltage or clocking frequency
can be decreased before overheating occurs.
In some systems it is beneficial to detect when the
current is too low. For these applications, the INA303,
shown in Figure 1-3 provides both over and under
current detection.
TUPNI
TUPTUO
In this case, the alert output can notify the system of
this condition and fault handling procedures can be
implemented before system failure.
Another use of under-current detection is to provide
confirmation of proper system status. Some systems
go into low power modes where the current is below
the normal operating range. In this case, the under-
current alert output can be used to notify the host that
the system has indeed entered the shutdown state.
Some designs may only care to get notified if the
current is outside of expected operating bounds. For
these cases, the INA303 can be configured to run
in window mode by connecting the two alert outputs
together as shown in Figure 1-4. In this mode, the
single alert output will be high as long as the current is
with-in the normal operating window.
Power Supply 0V to 36V 2.7V V to S 5.5V Supp R ly PULL-UP O T v h er r - e C s u h r o r l e d nt CMP A L L IM ER I T T1 1 RLIMIT 1 Un T d h e r r e - s C h u o r l r d ent
LATCH1 INPUT SIGNAL
IN+ + OUT
IN- Supply VOUT
RPULL-UP
Load CMP A LI L M ER IT T 2 2 RLIMIT 2 ALERT1
DELAY ALERT2
GND LATCH2 CDELAY
tDELAY
Figure 1-3. INA303 Over and Under Current
Detection
When the current exceeds the over current fault
threshold, the ALERT1 output will respond within 1
μs. However, if the current goes below the under
current threshold, the ALERT2 response time is set
by the delay capacitor. Undercurrent situations may
briefly occur in normal operation; however, if this
condition persists, it could be due to a damaged
device or system that is about to fail.
TUPNI
TUPTUO
www.ti.com
Power Supply
0V to 36V 2.7V to 5.5V
Supply
VS RPULL-UP O N p o er rm at a in l g
CMP A L L IM ER IT T 1 1 RLIMIT 1 ALERT Region
LATCH1 INPUT SIGNAL
IN+ + OUT
IN- VOUT
Load CMP A LI L M ER IT T 2 2 RLIMIT 2 ALERT DELAY GND LATCH2
Figure 1-4. INA303 Window mode Operation
Alternate Device Recommendations
The INA226 can be used in applications that require
digital current monitoring. If only a single digital alert
output is needed, the INA300 provides an optimal
solution in a tiny 2mm x 2mm QFN package. For
applications that only require a single alert output in
additional to the analog current signal, the INA301
provides excellent current monitoring accuracy with
an alert response less than 1 μs. For cost sensitive
applications and added flexibility, the INA381 provides
an current sense amplifier with an internal comparator
with accessible inputs.
Table 1-1. Alternate Device Recommendations
Device Optimized Parameter Performance Trade-off
INA301 MSOP-8 package, single alert with analog monitor. Single alert
INA300 2mm x 2mm QFN package Alert only
INA381 Cost effective, accessible comparator inputs Single alert
INA226 Digital current monitor N/A
Table 1-2. Related TI Application Briefs
SBOA162 Measuring Current To Detect Out-of-Range Conditions
SBOA163 High-Side Motor Current Monitoring for Over-Current Protection
SBOA167 Integrating The Current Sensing Signal Path
SBOA193 Safety and Protection for Discrete Digital Outputs in a PLC System
18 Monitoring Current for Multiple Out-of-Range Conditions SBOA168C – MAY 2018 – REVISED OCTOBER 2021
Submit Document Feedback
Copyright © 2021 Texas Instruments Incorporated

## Page 19

www.ti.com Tech Note
Application Brief
High-Side Motor Current Monitoring for Overcurrent
Protection
Scott Hill, Mubina Toa, Current Sensing Products
High power, precision motor systems commonly measurement function to support this common-mode
require detailed feedback such as speed, torque and input voltage range.
position to be sent back to the motor control circuitry
For larger voltage motors (24-V and 48-V, for
to precisely and efficiently control the motor’s
example), the available options reduces to dedicated
operation. Other motor control applications, such as
current sense and differential amplifiers. As the
fixed motion tasks, do not require the same level of
voltage requirements continue to increase,
system complexity to carry out their jobs. Information
measurement errors begin to impact the ability to
alerts that the motor had stalled, an unintended object
effectively identify out-of-range conditions. One
was found in the motor's path, or that a short was
specification that describes an amplifier's
detected in the motor's winding can be sent back to
effectiveness at operating at high input voltage levels
the motor control circuitry. More complex motor
is the common-mode rejection (CMR) term. This
control systems implementing dynamic control and
specification directly describes how well an amplifier's
active monitoring can also benefit from adding simple
input circuitry can reject the influence of large input
out-of-range detection function because of the faster
voltages.
indication of out-of-range events.
Ideally, an amplifier is able to completely reject and
By placing a current sense amplifier in series with the
cancel out any voltage common to both input pins and
DC power supply driving the high side of the motor
amplify only the differential voltage seen between
drive circuitry as shown in Figure 1, the overall current
them. However, as the common-mode voltage is
to the motor can be measured easily detecting out-of-
increased, leakage currents in the amplifier's input
range conditions. To detect small leakages the low-
stage result in additional input offset voltage. Larger
side return current can also be measured. A
input range levels being monitored will create
difference between the high-side and low-side current
proportionally larger measurement errors.
levels indicates a leakage path exists within the motor
or motor control circuitry. For example, an amplifier (difference amplifier or
current sense amplifier) that has a CMR (Common-
R SHUNT Mode Rejection) specification of 80 dB will have a
significant offset voltage introduced in the
measurement based on the input voltage level. An 80-
dB CMR specification corresponds to an additional
100 μV of offset voltage induced into the
measurement for every volt applied to the input.
R SHUNT Many devices are specified under defined conditions
(V = 12 V and V = 5 V, for example) which
CM S
Figure 1. Low & High-Side Current Sensing establishes the base-line for the default specifications
(CMR and PSRR, specifically). In this example
The DC voltage level varies depending on the voltage operating at 60-V common-mode voltage creates a
rating of the motor leading to multiple current change in V of 48 V (60 V – 12 V). A 48-V change
CM
measurement solutions to accommodate the with a 80-db CMR results in an additional 4.8 mV of
corresponding voltage levels. For low-voltage motors offset voltage in addition to the specified input offset
(approximately 5-V), the selection of circuitry to voltage found in the device’s data sheet.
monitor this current is much simpler with multiple
Applications employing calibration schemes are less
amplifier types (current sense, operational, difference,
concerned by this additional induced offset voltage.
instrumentation) and can perform the current
However, for applications where system calibration
SBOA163B – JULY 2016 – REVISED FEBRUARY 2021 High-Side Motor Current Monitoring for Overcurrent Protection 19
Submit Document Feedback
Copyright © 2021 Texas Instruments Incorporated

## Page 20

cannot account for this shift in offset, selection of an range conditions. A fast signal bandwidth device, such
amplifier with better common-mode voltage rejection as the INA290, enables speedy response to a control
is required. unit to ensure other critical system components are
not damaged by the unintended excess current
The INA240 is a dedicated current sense amplifier
flowing in the system. The INA290 is a 1.1-MHz part
with a common-mode input voltage range of –4 V to
which can measure on the high-side between 2.7-V to
+80 V and a worst-case CMR (Common-Mode
120-V common mode.
Rejection) specification of 120 dB over the entire input
and temperature range of the device. 120 dB of CMR SUPPLY
corresponds to an additional 1 μV of input offset VCM =
voltage induced for every 1-V change in common- -4V to +80V
RPULL-UP
mode voltage. The temperature influence on the +
R -
amplifier's ability to rejection common-mode voltages SHUNT - GPIO
+
is not well documented in many product data sheets
so it should be evaluated in addition to the room REF
temperature specification. The INA240 maintains a
ensured 120-dB CMR specification over the entire –
40°C to +125°C temperature range. The typical CMR
performance for the INA240 over the entire
temperature range is 135 dB (less than 0.2 μV for
every 1-V change) as shown in Figure 2.
Figure 2. Common-Mode Rejection vs.
Temperature
A system controller has the ability to use the current
sense amplifier's measurement to evaluate the
operation of the system. Comparing the current
information to pre-defined operating threshold allows
for detection of out-of-range events. A comparator
following the high-side current sense amplifier can
easily detect and provide alerts quickly to the system
allowing for corrective actions to be taken.
Figure 3 illustrates the signal chain path for monitoring
and detecting out-of-range excursions when
measuring currents on a high-voltage rail driving the
motor drive circuitry. The output signal proportional to
the measured input current is directed to the ADC in
addition to be sent to the comparator to detect
overcurrent events. The comparator alert will assert if
the input current level exceeds the predefined
threshold connected as the comparators reference
voltage.
A key requirement for overcurrent detection circuitry is
the ability to detect and respond quickly to out-of-
RELlORTNOC
Tech Note www.ti.com
Figure 3. High-Side Overcurrent Detection
Alternate Device Recommendations
If a comparator is required, the INA301 is a good
option as it is a precision current sense amplifier with
an onboard comparator that is ideal for detecting
overcurrent events on common-mode voltages up to
36 V. For applications requiring higher voltage
capability, the INA149 is a high performance
difference amplifier capable of interfacing with
common-mode voltages up to ±275 V off of a ±15-V
supply and has a ensured CMR of 90 dB (or 31.6 μV
for every 1-V input change). If isolation or high voltage
common-mode capability is required, the TMCS1100,
hall-effect current sensor, has ±600 V of basic
isolation.
Table 1. Alternate Device Recommendations
OPTIMIZED PERFORMANCE
DEVICE
PARAMETER TRADE-OFFS
Package: SC-70, Signal High-side only (2.7 Vcm
INA290
Bandwidth (1.1 MHz) to 120 Vcm)
INA149 VCM Range: ±275 V CMR, Gain
Onboard Comparator;
INA301
35 μV VOS
VCM: 0 V to 36 V
TMCS1100 ±600-V basic isolation Not as accurate as a
shunt-based solution
Table 2. Related TI Application Briefs
LITERATURE
TITLE
NUMBER
High Precision, Low-Drift In-Line Motor
SBOA160
Current Measurements
Low-Drift, Low-Side Current Measurements
SBOA161 for Three-Phase Systems
Measuring Current To Detect Out-of-Range
SBOA162 Conditions
Precision Current Measurement On High
SBOA165
Voltage Power Rail
20 High-Side Motor Current Monitoring for Overcurrent Protection SBOA163B – JULY 2016 – REVISED FEBRUARY 2021
Submit Document Feedback
Copyright © 2021 Texas Instruments Incorporated

## Page 21

Application Brief
Shunt-Based Current-Sensing Solutions for BMS
Applications in HEVs and EVs
Guang Zhou and Dan Harmon
BMS Topologies and Current Measurement
Methodologies
Hybrid electric vehicles (HEV) and electric vehicles
(EV) continue to gain share in the overall global
automotive market. The battery management system
(BMS) for these vehicles carries out the important
tasks of keeping the battery inside the safe operating
area (SOA), monitoring power distribution, and
tracking the state of charge (SoC). In a typical HEV
and EV, both high- and low-voltage subsystems are
present. The high-voltage subsystem operates at
several hundred volts, and interfaces directly with
utility grid or high-voltage DC sources. The low-
voltage subsystem generally operates at 48 V and 12
V.
400-V and 800-V Charger
Non-isolated
CSA
NOITALOSI
Isolated CSA
daoL
egatloV-hgiH
48-V Conversion
Non-isolated
CSA
daoL
V-84
12-V Conversion
Non-isolated CSA
Non-isolated
CSA
daoL
V-21
www.ti.com TI Tech Note
The focus in this document is on non-isolated, shunt-
based current-sensing amplifiers (CSA), also called
current shunt monitors (CSM), and digital power
monitors (DPM) for bottom-of-stack or top-of-stack in
12-V to 48-V BMS subsystems. The advantages of
non-isolated shunt-based current sensing include
simplicity, low cost, excellent linearity, and accuracy. A
drawback of shunt-based current sensing is the power
dissipation requirements for the shunt resistor at the
maximum current levels.
Current Sense Amplifiers in an HEV or EV Charger
Battery array is an important component of any HEV
or EV. There are mainly two types of rechargeable
batteries: the lead acid battery that has been around
for over 100 years, and the Li-Ion battery that has only
been put into practical use since the 1980s. Both lead
acid and Li-Ion batteries follow a certain constant
voltage-constant current charging profile. The CSA
plays an important role in making sure the battery
Non C -is S o A lated remains within the SOA.
Operational current for the traction inverter and
charging current can be greater than 1000 A in many
systems. However, these BMS systems must also be
able to measure currents equal to or less than 1 A
when the vehicle is off as many systems continue to
function such as keyless entry or vehicle-to-world
communications. BMS systems must monitor the
power distribution as accurately as possible during
Figure 1. Topologies of Current Sensing in BMS both operation and vehicle off-state to provide overall
system health and safety information. State of charge
High-voltage battery, top-of-stack measurements (SoC), which is the equivalent of a fuel gauge for the
require an isolated solution. Magnetic solutions battery pack in an HEV or EV, correlates to driving
enable the require isolation, but typically will not be range. Current sensing is one of the important
able to support the full current range. TI offers isolated methods to determine SoC. In addition to the
shunt-based current sensing solutions, such as the precision monitoring of the battery, most automotive
AMC3301-Q1. A summary of other examples of BMS systems will require a redundant measurement
isolated current sensing technology can be found in with relaxed accuracy requirements to enable system
the Comparing Shunt- and Hall-Based Isolated level functional safety goals.
Current-Sensing Solutions in HEV/EV application
The extreme difference between the traction motor
brief. Isolation is not typically required for top-of-the-
current (>1000 A) and the off-vehicle communications
stack 48-V or 12-V battery systems nor in bottom-of-
current (<1 A) creates a multi-decade, high-precision,
the-stack implementations.
bidirectional (charging versus vehicle operation)
current measurement challenge.
SBAA324B – DECEMBER 2018 – REVISED FEBRUARY 2021 Shunt-Based Current-Sensing Solutions for BMS Applications in HEVs and 21
Submit Document Feedback EVs
Copyright © 2021 Texas Instruments Incorporated

## Page 22

TI Tech Note www.ti.com
Sizing the Shunt Resistor mA on a 100-μΩ shunt resistor, well below the target
minimum current level of 1 A.
Historically, measuring the high current with shunt-
based topologies has been challenging. However, If even lower current levels are needed to be
with the availability of ultra-low resistance shunts, the accurately measured, system calibration may become
option is now viable. A typical analog current sense necessary. Zero-drift devices enable single-point
amplifier will have a fixed gain between 20 V/V and calibration, and make such challenging designs
200 V/V and operate from a 5-V supply. This 5-V possible by offering stable performance over
supply determines the maximum output voltage temperature.
(ignoring swing to supply limitations), and when we
For current sensing in HEV and EV BMS subsystems,
divide by the two gain extremes, we get a full-scale
the INA229-Q1 or INA228-Q1 are excellent choices
input voltage range of 250 mV to 25 mV. Assuming a
for any bottom-of-stack implementations or top-of-
bidirectional maximum current measurement of ±1000
stack implementations in 48-V or 12-V systems with
A, we can calculate a maximum shunt value of 125
an 85-V common-mode specification and ultra-low
μΩ to 12.5 μΩ. As discussed in the TI Precision Labs
offset of ±1 μV. The industry standard digital interface
- Current sense amplifiers videos, the amplifier offset
can further simplify the design by taking advantage of
will dominate the offset error at the low current range.
an existing communication bus. The INA240-Q1 with
If we use the ultra-precise INA240-Q1 with an offset of
its 80-V common mode voltage range could be used
25 μV, we get an error of 20% and 200% respectively
in 48-V system top-of-stack measurements for either
on the two shunt resistors. Table 1 summarizes these
the redundancy implementation or in applications
calculations along with the power dissipation across
requiring less total dynamic range. All three devices
these shunts at 1000 A.
are manufactured with TI proprietary Zero-Drift
Table 1. INA240-Q1 Shunt Resistor Value, Offset technology, which enable single temperature
Error, and Power Dissipation Calculations for calibration, if needed, to address lower current
±1000-A BMS Application accuracy.
GAIN OPTION INA240A1: 20 V/V INA240A4: 200 V/V
Automotive Device Recommendations
Full scale input 250 mV 25 mV
In addition to the INA229-Q1 and INA228-Q1, TI
Maximum shunt 125 μΩ 12.5 μΩ
offers other digital output current, voltage, and power
Offset error at 1 A 20% 200%
monitors. Some example products and adjacent
PDIS at 1000 A 125 W 12.5 W technical documents are compiled in Table 2 and
Table 3.
Solving the Multi-Decade Challenge
Table 2. Alternative Device Recommendations
This is where an ultra-precise, low-offset solution is DIGITAL
DEVICE DESCRIPTION
required. TI’s DPMs are specialized analog-to-digital INTERFACE
converters (ADC) dedicated to measuring current. 85-V, Bidirectional, Zero-Drift, 16-Bit,
INA239-Q1 SPI Low- or High-Side, SPI Current/Voltage/
Most can also monitor bus voltage and can calculate Power Monitor
the power as well. The full-scale input range is scaled
85-V, Bidirectional, Zero-Drift, 16-Bit,
down from that of a typical ADC to accommodate the INA238-Q1 I2C, SMBUS Low- or High-Side, I2C Current/Voltage/
Power Monitor
typical small signal voltage drop across a shunt
resistor. The INA229-Q1 (SPI Interface) and INA228- INA226-Q1 I2C, SMBUS 36-V, Bidirectional, Zero-Drift, 16-Bit,
Low- or High-Side, I2C Current/Voltage/
Q1 (I2C interface) are 20-bit DPMs with V OFFSET = 1 Power Monitor
μV and a ±163.84-mV full-scale input range. Having a
defined full-scale input range makes calculating the Table 3. Adjacent Tech Notes
maximum shunt resistor value fairly straight forward, LITERATURE
LITERATURE TITLE
NUMBER
simply divide the full-scale input by the maximum
Current Sensing With INA226-Q1 in HEV/EV Low
current: 163.84 mV ÷ 1000 A = 163.84 μΩ. A more SBAA325
Voltage BMS Subsystems
commonly available 100-μΩ shunt resistor is used to
Integrated, Current Sensing Analog-to-Digital
calculate a 1% error at 1 A. SBOA170 Converter
The final error check is to verify that the integrated
ADC is capable of resolving a signal level less than
the offset error level. The INA228-Q1 and INA229-Q1
feature a 20-bit delta-sigma converter with one bit
being the sign bit. Dividing the full-scale input of
163.84 mV by 19 bits of resolution results in 312.5 nV
per least significant bit (LSB). This corresponds to 3.1
22 Shunt-Based Current-Sensing Solutions for BMS Applications in HEVs and SBAA324B – DECEMBER 2018 – REVISED FEBRUARY 2021
EVs Submit Document Feedback
Copyright © 2021 Texas Instruments Incorporated

## Page 23

www.ti.com
Application Brief
12-V Battery Monitoring in an Automotive Module
Peter Iliya
Monitoring current off an automotive 12-V battery Dedicated TI current sensors are low in power
provides critical data for a variety of applications such consumption and highly accurate (<1% error) in
as module current consumption, load diagnostics, and automotive environments even across temperature.
load feedback control. The TI current sensing portfolio
A matched internal gain network plus input offset
can address this space with analog and digital current
zeroing provides lower measurement drift across
sense amplifier (CSA) devices that come automotive
temperature compared to either discrete solutions
qualified, contain integrated features, and operate in
or ICs with supplemental integrated current sensing.
12-V environments even though powered with low-
This amplifier integration and technology can remove
voltage rails. This document provides recommended
the need for temperature and system calibrations, all
devices and architectures to address current sensing
at low cost.
in this space.
Usually, general system protection schemes do not
{1.8 V to 5 V}
fully suppress or protect against voltage surges, so
VS
(1 V 2 B A V T ) + Diagnostic these primary regulations translate into typical voltage
OUT Control / survivability requirements. Depending on the system,

ADC a current sensor may need to survive load dumps,
Load GND reverse battery protection, fast load-switching, and
inductive kickback voltages. For example, working on
GND Current Sense Amplifier (CSA) a 12-V battery rail requires at least 40-V survivability
during load dump conditions. It is important to choose
Figure 1. Current Sense Amplifier on 12-V Rail
a current sensor that has an input common-mode
There are constraints in this space that stem from voltage (V CM ) rating that complies with the worst-case
conditions such as electrical transient protection V CM condition of the system. Otherwise, input voltage
regulations ISO7637-2 and ISO16750-2, jump-starts, clamping schemes are needed to protect the device
reverse-polarity, and cold-cranking. In general, during such conditions.
system-level protection and suppression schemes can
There are multiple TI Current (Power) Sensing
be used to protect downstream circuitry from these
amplifiers that can operate on a 12-V automotive
voltage surge conditions. Types of devices included in
battery and survive crucial voltage levels up to 40
these solutions are smart high-side switches, smart
V and more. Ultimately, they provide very accurate,
diodes, or other discrete implementations. These
zero-drift, high bandwidth, and low-cost solutions.
products may come with internal integrated current
Using TI's Product selection tool online, Table 1
sensing features, but they often are not very accurate
tabulates candidates for high-side current sensing
(±3% to ±20% maximum error) and have limited
on an automotive 12-V battery rail requiring 40-V
dynamic range.
survivability. It should be noted that all devices in
Table 1 have multiple gain variants ranging from 20
V/V to 500 V/V.
Table 1. Current Sense Amplifiers for Monitoring 12-V Automotive Battery
TI Current VCM VOS_MAX
BW
Gain Error MAX IQ_MAX
Features
Sense Amplifier Survivability (25 °C) (25 °C) (25 °C)
PWM rejection (very high CMRR), AEC Q100 (temperature grades 1 and
INA240-Q1 -6 V to +90 V ±25 μV 400 kHz ±0.2% 2.4 mA
0)
INA190-Q1 -0.3 V to +42 V ±10 μV 45 kHz ±0.3% 65 μA More accurate version of INA186-Q1. Wide dynamic range.
INA186-Q1 -0.3 V to +42 V ±50 μV 45 kHz ±1% 65 μA
Low input bias current (IB = ±500 pA typical). Wide dynamic range.
Operates with supply voltage (VS) of 1.7 V.
INA180-Q1 (INA181-Q1) -0.3 V to +28 V ±500 μV 350 kHz ±1% 0.5 mA Single, dual, and quad channel. Uni- or bi-directional versions
SBOA343A – MARCH 2019 – REVISED MARCH 2022 12-V Battery Monitoring in an Automotive Module 23
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 24

www.ti.com
According to Table 1, the INA240-Q1 provides the and the internal ESD structure of the CSA, but it
best performance, but is not optimized to monitor a is usually not needed. If it is needed, R should be
2
12-V battery compared to INA186-Q1, which requires small compared to R . The power rating of diodes
1
less power, cost, and package size. The INA186- depends on the maximum expected voltage rise, but
Q1 does have high AC CMRR (140 dB) and large more importantly on the turn-on current. The diode
dynamic range (V swings to V - 40 mV over current can be reduced by increasing R resistance,
OUT S 1
temperature). Additionally, the INA186-Q1 possesses but this reduces the effective gain of the circuit and,
a unique capacitively-coupled input architecture that more critically, increases gain error variation for most
increases differential input-resistance by 3 orders of current sensors (except INA186-Q1).
magnitude compared to majority of CSAs. High input-
Given the internal resistor gain network and input
impedance allows the user to filter current noise at
differential resistance of the INA181-Q1, an engineer
the device input with minimal effect on gain. Using
can calculate the effective circuit gain with R using
the data sheet equation if R = 1 kΩ, the effective 1
1 the equation in the data sheet. Keep in mind that
gain is reduced 43.5 m% for all variants except A1
adding external resistors broaden the system gain
(25 V/V). Figure 2 shows use of INA186-Q1 in battery
error variance beyond the data sheet limits. This is
monitoring. Filtering at the input (instead of output)
due to the fact that INA181-Q1 internal resistors are
means current noise is not amplified and the INA186-
matched to be ratio metric, but are not trimmed to
Q1 can drive a cleaner signal into the ADC without an
their typical values, so their absolute values can vary
output filter loading down the ADC.
by ±20%.
VPEAK 42 V
Overall, an engineer can choose the INA181-Q1
f-3dB =
1/(4R1C) because total cost with input protection is lower
VBAT + VBAT R1 1k + and increase in gain error variation is acceptable;
R L SH o U a NT d INA  186-Q1 R N S l H o o U a i N s d T y R1 1k INA  186-Q1 s h t o ra w i e g v h e tf r o , r d w e a v r i d c e s s o l w u i t t i h o n h s ig t h h e a r t p ra ro te v d id V e C a M c c a u re ra m te o c re u rrent
GND GND sensing over temperature with less complexity and
Figure 2. INA186-Q1 On 12-V Battery With and fewer components.
Without Noise Filtering
Alternate Device Recommendations
The breadth of the current sense portfolio enables
the user to optimize tradeoffs when incorporating See Table 2 for applications that need either larger
common input protection schemes. If the chosen V ranges or integrated features such as shunt
CM
device states that the Absolute Maximum Common- resistors or comparators.
Mode Voltage rating cannot exceed your maximum
Table 2. Alternate Device Recommendations
expected voltage surge, then it needs input protection.
Performance
Along with some passives, the current sensor needs Device Optimized Parameters
Tradeoff
transient voltage suppression (TVS) or Zener diodes Integrated 2 mΩ shunt resistor (included in Gain Error
at the inputs for protection. Figure 3 shows an
INA253
spec). Enhanced PWM rejection
IQ
example using the cost-optimized current sensor INA301-Q1 BW and
t h
s
r
le
e
w
sh
r
o
a
l
t
d
e .
a
I
n
n
d
te
1
r n
μ
a
s
l
a
co
le
m
rt
p
r
a
e
r
s
a
p
t
o
o
n
r
s
w
e
it h
tim
a
e
djustable 40 V VCM max
INA181-Q1.
INA302-Q1, BW and slew rate. Dual comparator output with
INA303-Q1 adjustable thresholds and 1 μs alert response time
40 V VCM max
VPEAK > 28 V LMP
Q
82
1
78Q- -12 V to +50 V
f i
V
lte
C
r
M
in
s
g
u
.
r
B
vi
u
v
f
a
fe
b
r
i
e
lit
d
y .
o
A
u
d
tp
ju
u
s
t
table gain and VOS
D1 I I N N A A 1 1 x x 8 9 - - Q Q 1 1 , ≥60 V i C n M p . u C t u re rr s e is n t t o o r u s. t p L u o t w (a I d B j w us h t e a n b l p e o g w a e in re ). d T o ri f m f med VOS
INA181-Q1
VBAT R1 R2 optional
+
RSHUNT
 Related Documentation
R1 R2 optional
Load 1. Transient Robustness for Current Shunt Monitor
D1
2. Measuring Current To Detect Out-of-Range
GND
Conditions
Figure 3. INA181-Q1 with Input Protection for V 3. Precision current measurements on high-voltage
CM
> 28 V power-supply rails
4. Integrating the Current Sensing Signal Path
In Figure 3, diodes D1 clamp the input V of the
CM 5. Shunt-based Current-Sensing Solutions for BMS
device to less than 28 V, which is the absolute
applications in HEVs and EVs
maximum for INA181-Q1. R is optional and can
2
be included to prevent simultaneous turn-on for D1
24 12-V Battery Monitoring in an Automotive Module SBOA343A – MARCH 2019 – REVISED MARCH 2022
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 25

www.ti.com
Application Brief
Simplify Voltage and Current Measurement in Battery Test
Equipment
Kevin Zhang, Maka Luo, Raphael Puzio
Introduction Function Test
Battery test equipment is used to verify battery pack Functional testing verifies that the battery pack is
functionality and performance prior to shipment to the operational prior to shipment to the customer. This
customer. This application brief outlines three major assures that each battery cell and battery pack is
functional tests that a battery tester performs while working properly.
showing how to achieve the desired level of regulated
error.
Amplifier Usage in Battery Test Equipment
Power Grid ... ... In typical systems, a Buck converter is used
VAC
Single Cell Test Unit
as the power source for battery charging and a
Bidirectional Buck / Boost Boost converter is used for battery discharge. Both
AC/DC VDC Converter B I/V a t S te e ry nsing conventional operational amplifiers (Op Amps) and
0 instrumentation amplifiers (INAs) are used in the
Amplifier
feedback loop to control both the charging and
0
discharging voltage and current.
DC/DC
Loop Control
Amplifier To charge the battery, the buck converter is enabled
while the first-stage voltage Op Amps and current-
Control Battery sense INA are used to measure battery voltage and
I/V Monitor
charging current of the battery cell or battery pack.
MCU ADC MUX
The switch between the current-sense Op Amp and
Figure 1. Traditional Battery Test the sense resistor s that the input to the current-
Equipment Block Diagram sense Op Amp is positive regardless of the direction
of current flow across the sense resistor. These
conditioned signals serve as the input to the second
Formation and Grading of Batteries
stage error Op Amp for either the voltage loop or
After the battery cell or battery pack is assembled, current loop, respectively.
each unit must undergo at least one fully controlled
The gained up output from each error Op Amp
charge or discharge cycle to initialize the device,
serves as the input to the third-stage buffer Op
and convert it to a functional power storage device.
Amp. The output of the buffer Op Amp feeds into
Battery vendors also use this process to grade
the feedback pin of the buck converter to control
battery cells, the process of separating the cells
the output voltage or current. Depending on the
into different performance groups according to target
output current requirements, the buck-boost functions
specifications. For a more in-depth look at the battery
can be accomplished several ways; however, two
initialization circuit, view our Bi-Directional Battery
approaches are the most common.
Initialization System Power Board Reference Design.
For higher current requirements, an integrated charge
Loop and Feature Test controller and external FET can be used. However,
for lower-current requirements, which are common
The loop and feature test refers to cycling the battery
in cost-sensitive systems, this function can be
cell or battery pack through repeated charging and
implemented discretely as shown in Figure 2. The
discharging sequences. This verifies that the battery’s
design engineer can adjust V and V on the
V_ref I_ref
characteristic life and reliability parameters to assure
positive input pins of the error Op Amps to adjust
they are within the specified range of the defined
the target output voltage and current of the buck
tolerances.
converter to the optimal value.
SBOA236A – OCTOBER 2017 – REVISED MARCH 2022 Simplify Voltage and Current Measurement in Battery Test Equipment 25
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 26

www.ti.com
In a typical battery charging application, the output Since the major errors come from the voltage- and
voltage of the current-loop error Op Amp starts current-sense amplifiers, it’s important to select high
high, putting the buck converter into constant current precision amplifiers.
output.
For example, if the desired regulated output current
In the next phase, the output voltage of the voltage- target, I is 10A, and the current sensing resistor,
SET
loop error Op Amp goes high, putting the buck R is 20mΩ, the input error of amplifier will be:
SENSE
converter into constant voltage output. When the
battery is being discharged, the boost converter is V I_ERR_RTI  ERR I OUT  I SET  R SENSE 200 V (1)
enabled. The Op Amps control the battery discharge
current and voltage, functioning in the same manner V  ERR  V 21 mV
V _ERR_RTI V OUT SET (2)
as they do when the battery is being charged. The
boost converter boosts the battery voltage to V DC , If the desired regulated output voltage is set to V SET
which is usually 12 V. 4.2 V, the input error of amplifier will be:
Other Single Assuming the temperature rises from 25°C to 85°C
Cell Test Units
and the battery voltage is 4 V, we can easily calculate
Single Cell Test Unit
Power Grid TPS54335A SN74LV4053 the real world error from one of our low-offset and low
offset drift Op Amps, the TLV07.
SW
Buck
VAC Converter COMP Switch
INA125 (3)
Bid A ir C e / c D ti C onal VDC TPS6117B 0 V TLV07_ERR_RTI (cid:31) V OS_max (cid:30) dV OS / dT max (cid:29) 60 (cid:31)C (cid:30) 4 V / CMRR DC
SW (cid:31) 100 (cid:28)V (cid:30) 0.9 (cid:28)V / (cid:31)C (cid:29) 60 (cid:31)C (cid:30) 4 V / 158489
Boost
Converter COMP 0 (cid:31) 154 (cid:28)V (cid:27) V and V
TLV07 I_ERR_RTI V_ERR_RTI
To Monitoring Circuit -
Using the calculated numbers above, it is clear that
Voltage
- - Sense a precision Op Amp similar to TLV07 is an ideal Op
Voltage Amplifier
Buffer Loop Error + Amp to meet the system output current and voltage
Amplifier Amplifier
error requirements.
+ +
TLV07 TLV07 For our next example, we use an INA which integrates
(Optional) VV_ref to set VOUT
all the feedback resistors. delivers V = 150
To Monitoring Circuit - OS_max
Current μV and dV OS /dT max = 0.5 μV/°C and is a good fit
- Sense to perform the current-shunt amplifier function in a
Current Amplifier
Loop Error + system with a simplified design.
Amplifier
+ If the system requires even higher performance
TLV07 TLV07
specifications, the current and voltage error can be
INA188
VV_ref to set IOUT INA826 changed to 0.05% and 0.1%, respectively. In this
INA125
case, precision INAs such as the zero-drift INA188,
can be used. Assuming the same conditions from the
Figure 2. Battery Test Equipment
above example, with a 60°C temperature rise and
Typical Amplifier Configuration
VBAT of 4 V, the real-world error from the INA188 is:
V = 67 μV and V < = 4.2 mV
I_ERR_RTI V_ERR_RTI
System and Amplifier Requirements
Typical system requirements:
Reference Circuit of Amplifier in Each Stage
• Regulated current error ERR Iout = 0.1% Looking at the voltage and current sense reference
• Regulated voltage error ERR Vout = 0.5% circuit shown in Figure 3, I+ and I- contributions are
resultant of the current sense resistors. The B+ and
To achieve the above requirements, an Op Amp with
B- components are from the positive and negative
low offset voltage (V ), low V temperature drift
OS OS
terminals of the battery. Since the actual battery
and high CMRR, like the TLV07 is needed.
voltage might be higher than 5 V, the typical Op Amp
The Op Amps create a closed loop with the power power supply is 12 V. The TLV07, INA188 and INA125
stage, the voltage on the inverting input of the error all have 36 V max (±18 V) supply voltage, meeting
Op Amp will be very close to the reference voltage system requirements.
V and V , thus minimizing the error from the
V_ref I_ref
large loop gain.
26 Simplify Voltage and Current Measurement in Battery Test Equipment SBOA236A – OCTOBER 2017 – REVISED MARCH 2022
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 27

As battery current can be close to zero during
charge and discharge cycles, a bi-polar supply is
implemented in the first stage current-sensing Op
Amp to avoid clipping the current sense signal. Type-
III compensation is applied on each stage of the
error Op Amp , with R12, C3, C4 and R6, C1, C2
respectively. To assure loop stability, these values
should be fine-tuned based on the actual power
supply design.
R3 50 k + 12 V
R1 1 k ISET +
I+ +
R2 1 k – U2 TLV07 I – C1 100 n R6 1 k
R4 50 k
(cid:2)
R9 10 k + 12 V
+ 12 V
R7 20 k VSET COMP
B+ + Or
B+ R8 20 k – R11 10 k U4 TLV07 D2 Buffer
C3 100 n R12 1 k
R4 50 k U3 TLV07
C4 10 p
+ 12 V
R5 10 k D1
U1 TLV07
C2 10 p
+ 12 V
I+ ISET +
– U2 TLV07 I C1 100 n R6 1 k (cid:1)
www.ti.com
Conclusion
Voltage and current sensing are the two most
significant measurements in battery test equipment
systems. Furthermore, the most important parametric
characteristics for this application is a precision Op
Amp or INAs that feature low voltage-offset and
drift. These parameters are critical to assure high-
performance sensing while minimizing the first stage
contribution to system error.
Table 1. Three Types of Op Amp for
+ Battery Test Equipment
– Device System Benefits
TLV07 Low-offset voltage and low-drift provide
(Op Amp) sufficient regulated current and voltage error for
Device cost-sensitive systems
High CMR (common mode rejection) of (100 dB
+ 5 V INA125
Min) increases dynamic range at the output; low
(Instrumentation
offset voltage and low drift reduce the need for
amplifier)
costly and time-consuming calibration
Low-offset voltage and zero-drift provide lower INA188 regulated current error and voltage error while
(Instrumentation
a high CMR (104 dB Min) decreases common
amplifier)
mode interference
+ 12 V
+
R Q2V+
C5 10p R3 1 k R- Q1V- U RE 1 F INA188 R5 10 k D1
C6 10p - 12 V
C2 10 p
Use instrumentation amplifiers for
higher precision requirements
Figure 3. Voltage and Current Sense Circuit with
TLV07 and INA188 in High-end Application
SBOA236A – OCTOBER 2017 – REVISED MARCH 2022 Simplify Voltage and Current Measurement in Battery Test Equipment 27
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 28

www.ti.com
Application Brief
Current Sensing Applications in Communication
Infrastructure Equipment
In this article, a fresh look is taken into the Current Sensing at Point Of Load
major electronic end equipment for cellular wireless
The typical WI electronic system is powered from a
infrastructure (WI) from the perspective of current
DC bus, such as 12 V to 48 V. Lower supply rails are
sensing (CS). Several types of CS applications in
derived from the bus voltage.
such equipment are reviewed.
These lower voltage rails are called point of load
(POL) supplies, stemming from the fact that they
Current Sensing in Power Supply Block
satisfy a set of specific requirements and are
As shown in Figure 1, the power supply for the normally located in the vicinity of the loads they
WI equipment comes from the utility grid, solar serve. Depending on how critical or informative the
energy, or sometimes a combination of the two. The measurements are, sometimes it is desirable to
power supply is often backed up with battery storage monitor the current or voltage in one or more of these
for uninterrupted service during a power outage, POL supplies. The main requirements for the CSA in
especially in remote areas where solely depending on this situation may include (among others) accuracy,
grid electricity is not an option due to limitations from speed, dynamic range, and power dissipation by the
physical accessibility or economic feasibility. associated shunt resistor.
DCDC Charger AC/DC VDD
CSA
High Side I-Sense
Point of Load
Load CSA
Low Side I-Sense
Figure 1. WI Power Supply Block Diagram
Figure 2. Point of Load Current Sensing Options
The power supply block can be either integrated
into the WI end equipment, or it can be stand-
As shown in Figure 2, current can be sensed at either
alone. Regardless of the implementation, a common
side of the load, with analog or digital CSA, and
requirement is an intelligent power management
through either external or integrated shunt resistor.
system to charge batteries and ensure seamless
transitions between power sources. Current and CSA comes with a matched resistor gain network that
voltage sensing is an indispensable function in such provides value in terms of cost, board space, and
power management systems. performance. Most CSAs feature fixed-gain, ranging
from 10 to 1000. Some CSAs offer configurable
Current sensing can be implemented either on the
gain. For example, the INA225 has configurable gain
high side or on the low side. Dedicated high-voltage,
through two digital control pins, while other CSAs
shunt-based, Current Sense Amplifiers (CSA) such
have a gain that is configurable through an external
as the INA240 might be needed for fault to ground
resistor, such as INA139.
prevention. Magnetic current sensors such as the
TMCS1100 or TMCS1101 are both great choice System integration is further improved when a CSA
for high-voltage applications due to their inherent is chosen that comes with integrated analog to digital
galvanic isolation. conversion (ADC) and a shunt resistor.
SBOA366B – DECEMBER 2020 – REVISED MARCH 2022 Current Sensing Applications in Communication Infrastructure Equipment 28
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 29

www.ti.com
The key considerations in selecting a CSA and Power amplifier monitor and control systems such
associated shunt resistor for POL measurement starts as the AMC7834 are another option with integrated
with common-mode voltage, current range, accuracy, current sensing capability. Such a solution offers the
and speed. In addition, if overcurrent protection (OCP) possibility of further reducing board space.
is required, a CSA with an integrated fast-action
VDD
comparator is often an ideal choice, where system
parameters such as offset and propagation delay Current/Voltage/Power/
Energy Sense
are specified. Compared with discrete components,
ADC
such a CSA helps remove uncertainties and therefore
simplifies the design. PA Controller Temperature Sense
(w/uC or FPGA)
To monitor multiple POLs, a multi-channel CSA like
DAC d
the INA2290 or INA4180 might make sense, as
g
it offers four channels of analog output. When a s
RF Input
microcontroller or FPGA is present in the equipment,
an ADC channel is normally available, as well as a
Figure 3. PA Biasing and Current Sensing
digital bus such as I2C. In this situation, either an
analog or digital output CSA may be implemented
Alternate Device Recommendations
as a POL monitor. A multi-channel digital monitor
such as the INA3221 is another option that frees up TI offers a complete line of CSA and magnetic
controller ADC channels while taking advantage of sensors that serve well in WI end equipment, from
an existing I2C bus. This device offers a number of high-voltage supply current and PA current sensing
warning and alert signals for fast action in case of a to general purpose POL current monitoring. The
fault, as well as current, voltage, or power information high-voltage INA202 also comes with integrated
of three independent channels. comparator and reference output to facilitate the
OCP requirement. For applications where superior
accuracy is required, the INA190 family of devices
Current Sensing in Power Amplifiers
are good choices with nA input bias current.
The bias current in power amplifiers (PA) is adjusted
These devices are essential in situations where
to suit the need of an end application, modulation
the sensed current is very small. Some of these
scheme, and operation class. A typical PA with
devices come with Enable pins for further power
current sensing is shown in Figure 3.
reduction; some are offered in a WCSP package for
The PA is often constructed with silicon LDMOS board space optimization. For applications with lower
or GaN technology. Current sensing is important in common mode voltage requirements, the INA180
PA applications, both from the standpoint of the PA offers excellent speed and overall performance value.
operation and from the standpoint of overall energy The INA301 family of devices features integrated fast
efficiency management. Under the same bias voltage, comparators and high-speed amplifiers. Both outputs
the PA bias current differs due to device variation. are available that suit the need of OCP.
Further, the bias current changes with temperature.
Table 1. Alternate Device Recommendations
Consequently, in order for the PA controller to
Device Characteristics
accurately control the bias current, both the current
Common mode range -16 V to 80 V;
and temperature information must be available. The INA202
Comparator
bias current information is necessary in improving
system efficiency, where around 50% of total system
INA180 Low IQ; High bandwidth
power is consumed by the PA itself.
INA190 Low IQ; Low IB; Enable pin
INA301 High bandwidth amplifier; Fast comparator
Integrated power amplifier monitor and control
systems, such as the AMC7836, can simplify PA
circuit design. As mentioned, due to natural device
Related Documentation
variation, knowing gate voltage alone is sometimes
1. Hybrid Battery Charger With Load Control for
not sufficient in order to achieve accurate bias current
Telecom Equipment
control. When current sensing is required in the
2. Precision Current Measurements on High-Voltage
control loop, a separate high-voltage CSA, such as
Power Supply Rails
the INA290, can be used.
3. Common Uses for Multi-Channel Current
Monitoring
29 Current Sensing Applications in Communication Infrastructure Equipment SBOA366B – DECEMBER 2020 – REVISED MARCH 2022
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 30

www.ti.com
Application Brief
Safety and Protection for Discrete Digital Outputs in a
PLC System Using Current Sense Amplifiers
A Programmable Logic Controller (PLC) is the PLC digital outputs are used to turn ON-OFF the
most widely accepted computer protocol in industrial starter to engage a motor, to turn on a lamp to
control systems for factory automation application. indicate a fault, or to control the solenoid that engages
PLC system is a controller that controls, prioritizes, relay. The analog outputs include current level output
and provides status of the system. The controller and resistance level that can be used to control and
is programmed via basic binary logic much like a monitor heaters or control the speed of a motor.
modern computer. The PLC system consists of:
Input Output
1. PLC computer processor 24V 24V
2. The power supply rack ( P d u ig s i h ta B l u in t p to u n t) ( S d o ig le it n a o l i o d u R t e p l u a t y )
3. Back plane for power 24V Programmable 24V
4. Digital input and output module Thermocouple Co L n o tr g o ic ll e r (digit L a a l m ou p tput)
(analog input) (PLC)
5. Analog input and output module 24V
6. Computer software Pressure Sensor
(analog input)
7. A network interface for remote connectivity
Figure 1. PLC System Block Diagram
PLC systems are widely used in industrial applications
that is accelerating industrial 4.0 revolution.
Discrete PLC Digital Outputs Safety
PLC systems are enabling faster integration of
semiconductor devices for control and automation to Figure 2 illustrates a description of a PLC digital
increase efficiency and improve factory throughput. output circuit. PLC digital outputs are designed with
Industrial automation and integration examples high-drive strength capability that can be as high as
include controlling of temperature, turning on and off 1 A. The digital outputs can be connected to drive
a light fault indication, weighing of a package using a solenoid relay to take control of an action initiated
pressure sensors, turning on and off a solenoid relay. by the PLC controller. Discrete current sensors
connected in series with the load, as shown in Figure
As industrial systems can be noisy environments
2, monitor current flowing to the load continuously
with high-frequency signals and noise coupling
and report the presence of excessive currents to the
into low-voltage signals, the PLC system's output
controller to take an action. As PLC digital outputs can
modules are optocoupled. Its robustness to noise,
swing from –0.7 V to 24 V, a high-side current sense
simple architecture, ease of programming language,
amplifier with low-offset and low-gain error enables
industrial certifications, and safety features are the
safety for high-output drives.
reasons PLC systems are the most widely used
industrial protocol.
Current
Sense
PLC System Block Diagram Amplifier
OptoCoupler
The inputs and outputs of the PLC system are Load
categorized as digital or analog as illustrated in
Figure 1. The digital inputs provide an ON-OFF status +24V
Current Flow
to the control circuitry. Some examples of digital
input devices include limit switches, photoelectric
sensor, proximity, and pressure sensor. Analog input
devices such as thermocouples, tachometers, and PLC digital Output Ground
force sensors provide variable output responses.
Figure 2. PLC Digital Output Sinking
Current Circuit
SBOA193B – DECEMBER 2020 – REVISED MARCH 2022 Safety and Protection for Discrete Digital Outputs in a PLC System Using 30
Submit Document Feedback Current Sense Amplifiers
Copyright © 2022 Texas Instruments Incorporated

## Page 31

www.ti.com
As the PLC digital output drive can be high, the The signal throughput bandwidth of INA240 is 400
sinking current capability is one of the key safety kHz at the gain 20. The high bandwidth and high
parameters. The outputs are designed with NPN slew rate (2V/μs) enables the amplifier to be used for
transistors with a built in diodes for over voltage detecting fast over current or shorted load conditions
protection. The system ensures that when the PLC provided the sampling ADC within the PLC system is
digital outputs are engaged the sinking current from fast enough to sample currents.
the power supply is always within the PLC's specified,
over temperature operating range. A discrete current
Alternate Device Recommendations
sense amplifier can protect the digital outputs from an
overcurrent condition, provide diagnostics to address The INA293 is another device to recommend for this
faulty load conditions and address preventive action application. The INA293 is a unidirectional current
for premature system failure. sense amplifier that can support a common-mode
voltage from –4 V to 110 V and can survive –20 V to
116 V. This large negative common-mode survivability
High-Current PLC Digital Outputs Safety
is important due to inductive kickbacks seen when
PLC digital outputs can be directly tied to high-current engaging the solenoid or a motor starter. The INA293
solenoid drivers or high-current LED lamps to close also has a high-bandwidth of 1MHz which enables
relays or to indicate a fault in a factory automation fast reaction times in the event of an overcurrent
application. If the current output drive is higher than scenario.
the PLC system is rated for, a discrete FET can
The INA290 is also another device that can be used
be used to control the current flow form the 24-V
in PLC systems. INA290 is designed to operate with
supply to the load. Figure 3 illustrates the connection
a common-mode voltage range from 2.7 V to 110 V.
of a PLC digital output to an external low RDS on
Although this is a high-side sense device, the INA290
FET to further increase the output drive strength.
can survive common-mode transients of –20 V to 120
One disadvantage of this approach is the reliability
V.
of the external FET. Using a current-sensing amplifier
to monitor the load current can ensure that the PLC Table 1. Alternate Device Recommendations
system is operating safely. Performance Trade-
Device Optimized Parameter
Off
Current Bandwidth : 1.3 MHz,
Am
Se
p
n
li
s
f
e
ier INA293
VCM: –4 V to 110 V,
N/A
OptoCoupler Offset voltage: 15 μV,
Load
Gain error: 0.15%
Bandwidth : 1.3 MHz,
+24V Offset voltage: 55 μV,
Current Flow INA281 VCM: –4 V to 110 V,
gain error: 0.5
Cost Optimized
Bandwidth: 1.1 MHz,
Ground
PLC digital Output
INA290
Offset voltage: 12 μV, High-side only (VCM: -4
Gain error: 0.1%, to 110V)
Figure 3. PLC Discrete Digital Output Control Package: SC-70
Bandwidth: 1.1 MHz,
Offset Voltage: 150 μV,
INA240 is a high-precision, bidirectional current INA280 Package: SC-70,
Gain error: 0.5%
sense amplifier with low-input offset and gain drift Cost Optimized
across temperature range designed for measuring
currents on discrete PLC digital outputs. The INA240
is specifically designed to work in switched node Related Documentation
environments where the common-mode transients will 1. Current Sensing in an H-Bridge
have large dv/dt signals. The ability to reject high 2. Switching Power Supply Current Measurements
dv/dt signals enables accurate current measurements 3. High-Side Drive, High-Side Solenoid Monitor With
to ensure necessary protection and meet the required PWM Rejection
safety standards. The INA240 has low maximum 4. Measuring Current To Detect Out-of-Range
input offset voltage of 25 μV and a maximum gain Conditions
error of 0.2% allowing for smaller shunt resistance
values to be used without sacrificing measurement
accuracy. The offset drift and gain error drift is as low
as 0.25μV/°C and 2.5ppm/°C respectively enabling
accurate and stable current measurements across
temperature.
31 Safety and Protection for Discrete Digital Outputs in a PLC System Using SBOA193B – DECEMBER 2020 – REVISED MARCH 2022
Current Sense Amplifiers Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 32

Analog Design Journal Signal Chain
Current sensing in high-power USB Type-C®
applications
By Ben Damkroger
Product Marketing Engineer, Sensing-CSPS
Introduction operation. For sink devices, due to lower manufacturing
The introduction of USB Type-C,® also designated USB-C,® costs of power adapters and corners cut during construc-
tion as a result, there is no guarantee that consumers will
connectors eliminated the frustration of flipping USB
use power adapters with sufficient protection. Sometimes
cables three and four times before finding the correct
faulty power adapters may provide the 20 V before USB
orientation to plug in a device. A smaller, reversible cable
negotiation, which might overload sink devices. For source
is not the only thing new with the evolution of USB tech-
devices, cheap USB cables and USB-C PD devices may
nology, however; the standard also enables the replace-
have faulty wiring, causing a short on the load side of a
ment of various electrical connectors including USB-B and
charger and ultimately requiring some protection.
USB-A, HDMI, DisplayPort, and 3.5-mm audio cables.
When designing devices that act as power sources, such
Sadly, USB-C alone still has a power limit of only 15 W.
as docking stations or chargers, shield devices are
However, USB Power Delivery (USB PD) increased that
required downstream, especially when running near the
power limit to 100 W through the introduction of configu-
100-W limit of USB PD. Integrated current sensing in USB
rable voltage levels of 5 V, 9 V, 15 V or 20 V. This 100-W
PD controllers is usually only ±10% to ±15% accurate.
limit means that higher-power devices such as laptops,
These accuracy levels may be sufficient for some, but a
tablets and monitors can all charge and operate using a
more accurate solution for overcurrent protection maxi-
single USB PD cable.
mizes the amount of current that the USB port can
This increased power has more design considerations
provide while staying below a fixed power level.
than previous standards, however. USB-C cables have
For example, to account for current tolerance in a
24 pins, including four Vbus wires and four ground wires,
20-V/5-A (100-W) system, the maximum current limit
more than USB 3.0. These Vbus lines require overvoltage,
should be set 10% away from 5 A at 4.545 A (5 A × 1 ÷ 1.1).
overcurrent and reverse-current protection. Protections
With a maximum current of 4.545 A, the maximum possible
should be implemented that detect transient spikes
power delivered to a load device is 90.9 W. Additionally,
caused by faults in the Vbus line or faults internal to a
when the current is 10% below the maximum current at
load device. These protections should be added in both
4.090 A, a system would only deliver 81.8 W of power.
source and sink (load) devices to guarantee safe
Figure 1. USB PD power range
100 W
65 W
2.5 W 4.5 W 7.5 W 15 W 25 W
USB Type-C® USB Type-C® and USB PD
USB 2.0 USB 3.0 USB BC 1.2
Texas Instruments 3322 ADJ 1Q 2021

## Page 33

Analog Design Journal Signal Chain
To set a higher current threshold, a discrete current- In the most straightforward implementation, the analog
sense amplifier like the INA199, with its 150-μV voltage output of the comparator could be used to trip the FET.
offset and 1% gain error, can easily provide 1.5% accuracy Or digital logic could be used with an analog-to-digital
when the current is greater than 1 A in a 100-W applica- converter (ADC) and microcontroller (MCU) to decide
tion. This device enables the current limit to be set at when to switch the FET off. These overcurrent protection
4.926 A (5 A × 1 ÷ 1.015), which in turn means that the schemes provide a response time that is much faster than
USB port could deliver 98.52 W to the load. Figure 2 a thermal fuse and can be reset and reused without
shows the root-sum-square (RSS) error curves for the replacement. The INA381 current-sense amplifier, with its
INA199 and INA381. fast integrated comparator, shrinks the bill of materials by
eliminating the need for a comparator and makes the
Figure 2. INA199 and INA381 design process more straightforward. Figure 3 is a simpli-
total measurement error fied circuit using this device.
5.5
Figure 3. Simplified e-fuse
IINNAA338811AA22 VVoollttaaggee == 2200 VV
5 circuit using the INA381
INA381A2 Voltage = 5 V
INA199B1 Voltage = 20 V
4.5
INA199B1 Voltage = 5 V
%) 4
o r
(
3.5 R Shunt INA381
ALERT
r
r
E 3
S
S
R 2.5 +
V
2 Bus
–
1.5
1
2 2.5 3 3.5 4 4.5 5 5.56
Z
Load
Current (A)
For devices that sink power over a USB PD supply, it is
strongly suggested to include current sensing to protect
from internal faults. It is impossible to know whether Selecting the correct current sensor is essential, espe-
consumers will power peripheral devices with USB PD cially given their variety. Digital power monitors, for
controllers that have adequate overcurrent protection. example, provide an integrated ADC that frees up space
One easy solution is to design internal overcurrent protec- on a system’s MCU analog inputs, while also giving access
tion in the device’s Vbus input by wiring a discrete to extra features like power calculation and accumulation
current-sense amplifier, a comparator and a field-effect (energy). Analog current-sense amplifiers, on the other
transistor (FET) as an e-fuse. With an e-fuse, the current- hand, are more popular because of their low cost and
sense amplifier detects whether the current is too high versatility. The integrated gain resistors and input stages
and the comparator controls the FET triggers that turn off of analog current-sense amplifiers enable their inputs to
the FET and interrupts the current. detect common-mode voltages beyond their supply, thus
making these devices better for current sensing than stan-
dard operational amplifiers. Current-sense amplifiers can
also come integrated with comparators for faster response
times and alert signals for overcurrent protection; the latter
is especially important for protecting USB-C PD devices.
Texas Instruments 3333 ADJ 1Q 2021

## Page 34

Analog Design Journal Signal Chain
Total Error = V o 2 s + CMRR2 + PSRR2 + Gain Error2 + ShuntTolerance22 + BiasCurrent2 (1)
As previously mentioned, current-sense amplifiers are References
more accurate than the integrated current sensing of most
1. TI Precision Labs – Current Sense Amplifiers, Texas
USB-C PD controllers. To understand why, first consider
Instruments training and videos
the various error sources found on current-sensing data
sheets and use these sources to calculate errors using the 2. Maximize your system with current sense amplifiers,
root-sum-square (RSS) method. The different error Overview, Texas Instruments products and reference
sources present in current-sensing amplifiers are shown designs
by Equation 1 above.
Related Web sites
The input offset voltage (V ) is the dominant source of
OS
error when measuring small currents because it is larger Product information:
in relation to the relatively small shunt voltage—it dimin- INA199
ishes as the shunt voltage increases at higher currents. INA381
This offset error is inherent to all amplifiers and is a result INA381 Design tools and simulation
of resistor and transistor mismatching. Gain error domi-
nates the error at larger currents, as it remains present
even at high currents, and does not diminish as the shunt
voltage increases. These error sources can be manually
calculated using Equation 1. For additional information,
consult the References and Related Web sites on this
page.
Conclusion
With the advent of USB PD, the number of high-power
USB devices continues to grow. This article presented the
options available to implement current protections and
power monitoring, and also compensate for tolerance
errors that are typically encountered with integrated
current sensing in USB PD controllers. The need to maxi-
mize power to a load in a power-source device or protect a
power-sink device is also highlighted. Current-sensing
amplifiers provide an easy solution to many of these
design challenges, with higher accuracy and greater design
flexibility.
Texas Instruments 344 ADJ 1Q 2021

## Page 35

www.ti.com
Application Brief
Low-Drift, Precision, In-Line Motor Current Measurements
With PWM Rejection
Scott Hill Current Sensing Products
The demand for higher efficiency systems continues voltage steps. Because real-world amplifiers do not
to increase, leading to direct pressure for have infinite common-mode rejection potentially large,
improvement in motor operating efficiency and unwanted disturbances appear at the amplifier output
control. This focus applies to nearly all classes of corresponding to each input voltage step as shown
electric motors including those used in white goods, in Figure 2. These output disturbances, or glitches,
industrial drives and in automotive applications. The can be very large and take significant time to
operational characteristics of the motor fed back into settle following the input transition depending on the
the control algorithm are critical to ensure the motor characteristics of the amplifier.
is operating at its peak efficiency. Phase current is
one of these critical diagnostic feedback elements
used by the system controller enabling optimum motor
performance.
Due to the continuity of the measurement signal
and direct correlation to the phase currents, an ideal
location to measure the motor current is directly in-line
with each phase as shown in Figure 1. Measuring
current in other locations, such as the low-side of
each phase, requires recombination and processing
before meaningful data can be utilized by the control
algorithm.
Figure 2. Typical Output Glitch From Large Input
V Step
CM
A common approach to this measurement is to select
a current sense amplifier with a wide bandwidth. In
order to stay above the audible frequency range,
modulation frequencies ranging from 20kHz to 30kHz
are typically selected. Amplifier selection for making
in-line current measurements in these PWM-driven
applications targets amplifiers with signal bandwidths
Figure 1. In-Line Current Sensing
in the 200kHz to 500kHz range. The selection of the
amplifier was not historically based on actual signal
The drive circuitry for the motor generates pulse
bandwidth which are significantly lower than the PWM
width modulated (PWM) signals to control the
signal. The higher amplifier bandwidths were selected
motor’s operation. These modulated signal subject
to allow the output glitch to settle quickly following an
the measurement circuitry placed in-line with each
input voltage transition.
motor phase to common-mode voltage transitions
that can switch between large voltage levels over The INA240 is a high common-mode, bi-directional
very short time periods. An ideal amplifier would current sense amplifier designed specifically for
have the ability to completely reject the common- these types of PWM-driven applications. This device
mode voltage component of the measurement and approaches the problem of measuring a small
only amplifier the differential voltage corresponding differential voltage in the presence of large common-
to the current flowing through the shunt resistor. mode voltage steps using integrated enhanced PWM
Unfortunately, real-world amplifiers are not ideal rejection circuitry to significantly reduce the output
and are influenced by the large PWM-driven input disturbance and settle quickly. Standard current sense
SBOA160C – JULY 2016 – REVISED SEPTEMBER 2021 Low-Drift, Precision, In-Line Motor Current Measurements With PWM 35
Submit Document Feedback Rejection
Copyright © 2021 Texas Instruments Incorporated

## Page 36

www.ti.com
amplifiers rely on a high signal bandwidth to allow Common system-level calibration frequently reduces
the output to recover quickly following the step, the reliance on an amplifier's performance at
while the INA240 features a fast current sense room temperature to provide precise measurement
amplifier with internal PWM rejection circuitry to accuracy. However, accounting for parameter shifts
achieve an improved output response with reduced such as input offset voltage and gain error as the
output disturbance. Figure 3 illustrates the improved operating temperature varies is more challenging.
response of the INA240 output due to this internal Good temperature compensation schemes are based
enhanced PWM rejection feature. on characterization of the amplifier’s performance
variation over temperature and rely on consistent
and repeatable response to external conditions from
system to system. Improving the capability of the
amplifier to remain stable with minimal temperature
induced shifts is ideal to reduce the need for complex
compensation methods.
The INA240 features a 25μV maximum input
offset voltage and a 0.20% maximum gain
error specification at room temperature. More
importantly for applications requiring temperature
stable measurements, the device's input offset voltage
drift is 250nV/°C with a 2.5ppm/°C amplifier gain drift.
Even as the operating temperature varies over the
Figure 3. Reduced Output Glitch By Enhanced
system’s entire temperature range, the measurement
PWM Rejection
accuracy remains consistent.
For many three-phase applications there are few
The INA253 features all of the performance benefits
requirements on the accuracy of this in-line current
of INA240 amplifier with the inclusion of integrated
measurement. Limited output glitch is necessary to
shunt. The integrated shunt is a low inductive,
prevent false over-current indications in addition to
precision, 2mΩ 0.1% with a temperature drift of
having an output that quickly responds to ensure
<15ppm/°C. The INA253 is the most accurate current
sufficient control of the compensation loop. For
sense amplifier that eliminates the error introduced
other systems, such as electronic power steering
due to shunt and parasitic introduced due to the
(EPS), precise current measurements are needed to
layout.
provide the required feedback control to the torque
assist system. The primary objectives in an EPS Combining the measurement temperature stability,
system are to assist with additional torque to the wide dynamic input range, and most importantly
driver’s applied torque on the steering wheel and enhanced PWM input rejection, the INA240 is an ideal
provide a representative feel in the steering response choice for PWM-driven applications requiring accurate
corresponding to the driving conditions. Phase to and reliable measurements for precisely controlled
phase current measurement errors can become very performance.
noticeable in this tightly controlled system. Any
unaccounted for variance between phases leads
directly to increased torque ripple that is perceptible
to the driver through the steering wheel. Reducing
the measurement errors, especially those induced
by temperature, is critical to maintain the accurate
feedback control and to deliver a seamless user
experience.
36 Low-Drift, Precision, In-Line Motor Current Measurements With PWM SBOA160C – JULY 2016 – REVISED SEPTEMBER 2021
Rejection Submit Document Feedback
Copyright © 2021 Texas Instruments Incorporated

## Page 37

www.ti.com
Alternate Device Recommendations Table 1. Alternate Device Recommendations
Device Optimized Parameter Performance Trade-Off
Depending on the necessary system requirements,
there may be additional devices that provide the Common-Mode Input Low Bandwidth, Ideal For
INA282 Range: -14V to +80V;
needed performance and functionality. The INA282 is DC Applications
MSOP-8 Package
able to measure very precisely large common-mode
Common-Mode Input
voltages that do not change as quickly as what Low Power: 155uA;
LMP8481 Range: 4.5V to 76V;
typically is seen in PWM driven applications making it MSOP-8 Package Lower Accuracy
ideal for high voltage DC applications. The LMP8481
is a bi-directional current sense amplifier used for Table 2. Related TI Application Briefs
high common-mode voltages that do not require the Benefits of a Low Inductive Shunt for
SBOA202
amplifier to include ground within the input voltage Current Sensing in PWM Applications
range. Precision Brightness and Color Mixing in
SBOA189 LED Lighting Using Discrete Current Sense
Amplifiers
Low-Drift, Low-Side Current Measurements
SBOA161
for Three-Phase Systems
High-Side Motor Current Monitoring for
SBOA163
Over-Current Protection
SBOA160C – JULY 2016 – REVISED SEPTEMBER 2021 Low-Drift, Precision, In-Line Motor Current Measurements With PWM 37
Submit Document Feedback Rejection
Copyright © 2021 Texas Instruments Incorporated

## Page 38

www.ti.com TI Tech Note
Tech Note
High-Side Drive, High-Side Solenoid Monitor With PWM
Rejection
Arjun Prakash,Current Sensing Products
A solenoid is an electromechanical device that is when the high-side switch is turned off. Eliminating
made up of a coil wound around a movable iron the solenoid's continuous connection to the battery
material, also called an armature or plunger. When an voltage reduces solenoid degradation and early
electric current is passed through the coil, a magnetic lifetime failures.
field is generated, causing the armature to travel over
Battery
a fixed range. Figure 1 shows an illustration of an
Reference
electromechanical solenoid. Solenoids are often Voltage
designed for simple ON - OFF applications like relays
+
that require only two states of operation. These -
solenoids can be also be designed for linear operation
INA253
where the current is proportional to the position of the
armature. Linear solenoids are used in several
applications where pressure, fluid, or air is precisely
regulated. In automotive applications, linear solenoids
are used in fuel injectors, transmission, hydraulic
suspension, and for haptic effects. Linear solenoids Figure 2. High-Side Drive With High-Side Current
are seen in critical medical applications that require Sense
precise air flow control as well as industrial
applications that redirect and control fluid flow. The current sense amplifier shown in Figure 2 must
be able to reject high common-mode dv/dt signals as
Induced Magnetic Field Inside Coil well as support common-mode voltages that fall below
Rod Moves In
ground. In the above configuration when the high-side
S N
switch is turned on, the solenoid is energized by the
current flowing from the battery. The duty cycle of the
high-side switch determines the current flowing
Current
through the solenoid, which in turn controls the travel
range of the plunger. At the time when the high-side
switch is turned off, the current flows through the
Induced Magnetic Field Inside Coil
flyback diode forcing the common voltage to drop one
Rod Moves Out
diode drop below ground.
S N
Solenoids and valves are highly inductive in nature.
The effective impedance of solenoid can be simplified
Current as resistance and inductance. The coil is constructed
using copper (4000ppm/°C) and the effective
resistance varies on the type of solenoids from 1 Ω for
Figure 1. Electromechanical Solenoid haptic applications to 10 Ω for a linear or positional
Construction valve systems. The inductance for all of the solenoids
ranges from 1 mH to 10 mH. Figure 3 shows example
There are multiple configurations for connecting and
of current profile of a solenoid driver in open-loop
driving solenoids. One common approach to driving
mode at 25 °C and 125 °C. Over a 100 °C rise in
solenoids is to use a high-side driver configuration. In
ambient temperature without compensating for Cu
this configuration, the current sense amplifier is
resistance, the plunger travel distance accuracy is
connected between high-side switch and the solenoid
around 40%. The solenoid current flow directly
as shown in Figure 2. One benefit to this configuration
controls the plunger's travel distance. If the ambient
is the solenoid is isolated from the battery voltage
SBOA166C – OCTOBER 2016 – REVISED DECEMBER 2020 High-Side Drive, High-Side Solenoid Monitor With PWM Rejection 38
Submit Document Feedback
Copyright © 2020 Texas Instruments Incorporated

## Page 39

TI Tech Note www.ti.com
temperature changes, the plunger's travel distance Alternate Device Recommendations
changes impacting the output control which could be
The INA240 is a high-side, bidirectional current sense
regulating pressure, fluid, or air.
amplifier that can support large common-mode
PWM voltages ranging from –4 V to +80 V. The INA240 is
Current @ 25C
Current @ 125C specifically designed to operate within PWM
applications where high dv/dt transients needs to be
suppressed. The device's low offset voltage of 25 μV
(maximum), drift of 250nV/°C (maximum), gain (0.2%)
0V
and high bandwidth of 400 kHz enables accurate in-
-0.7V line current measurements for PWM applications.
Figure 3. Solenoid Current Profile Across
The INA293 is another device to recommend for this
Temperature
application. The INA293 is a unidirectional current
sense amplifier that can support a common mode
Measuring current in solenoid and valve applications
voltage from -4 V to 110 V and can survive -20 V to
provides the capability to detect changes in a
120 V. This large negative common-mode survivability
solenoid's operating characteristics. Current
is important due to inductive kickbacks seen when
measurement can identify the effects of a decrease in
engaging the solenoid. The INA293 also has a high-
the magnetic field of an aging solenoid to detect faulty
bandwidth of 1 MHz which enables fast reaction times
components before they fail. In an open-loop solenoid
in the event of an overcurrent scenario. Furthermore,
control system, the variation of effective impedance
the INA293 is an high-precision current sense
can drift 40% for a 100 °C rise in temperature from the
amplifier with a low offset voltage of 15 μV and gain
copper windings. Current measurement used in a
error of 0.15%.
current control feedback loop can reduce the
solenoid's impedance variation over temperature from The INA281 is another device to recommend, which is
40% down to 0.2% using the INA253 current sense a unidirectional current sense amplifier similar. It can
amplifier. support a common-mode voltage from -4 V to 110 V
and also survive -20 V to 120 V. The INA281 has a
The INA253 is a high-side, bidirectional current sense
high bandwidth of 1 MHz for fast in-line
amplifier with an integrated 2-mΩ low inductive shunt
measurements.
that can support large common-mode voltages
ranging from –4 V to +80 V. The integrated shunt is Table 1. Alternate Device Recommendations
factory calibrated to provide <0.1% total system
OPTIMIZED PERFORMANCE
DEVICE
accuracy with a gain error temperature drift of PARAMETER TRADE-OFF
<15ppm/°C. The INA253 is specifically designed to PWM settling time: 2
operate within PWM applications where high dv/dt INA240 μs, Package: TSSOP
transients needs to be suppressed. INA253 is Vos: 25 μV
designed with circuitry to suppress dv/dt signals. This Vcm range: -4 V to
+110 V,
features lowers blanking time enabling accurate PWM No enhanced PWM
INA293 Bandwidth: 1.3 MHz,
current measurements at lower duty cycle. The rejection
Offset voltage: 15 μV,
device's low offset voltage, drift, gain, and high Gain error: 0.15%
bandwidth of 400 kHz enables accurate in-line current
Vcm range: -4 V to No enhanced PWM
measurements for PWM applications. Valve +110 V, rejected,
INA281
applications requiring precision control of fluid, air, Bandwidth: 1.3 MHz, Offset voltage: 50 μV,
Cost Optimized Gain error: 0.5%
and pressure all benefit from the accuracy and
temperature stability in the current measurement by
Table 2. Related TI TechNotes
using the INA253. The integrated precision shunt of
DOCUMENT TITLE
the INA253 can provide a high-accuracy position
control of <0.2% across temperature, eliminating the Precision Brightness and Color Mixing in
SBOA189 LED Lighting Using Discrete Current Sense
need for current measurement calibration across
Amplifiers
temperature.
Low-Drift, Low-Side Current Measurements
SBOA161
for ThreePhase Systems
High-Side Motor Current Monitoring for
SBOA163
Over-Current Protection
SBOA193 Safety and Protection for Discrete Digital
Outputs in a PLC System Using Current
Sense Amplifiers
39 High-Side Drive, High-Side Solenoid Monitor With PWM Rejection SBOA166C – OCTOBER 2016 – REVISED DECEMBER 2020
Submit Document Feedback
Copyright © 2020 Texas Instruments Incorporated

## Page 40

www.ti.com
Application Brief
Current Mode Control in Switching Power Supplies
Most switching power supplies are designed with 2. The power supply looks like a voltage controlled
closed-loop feedback circuitry to provide stable current source. This permits a modular supply
power under various transient and load conditions. design to allow load sharing between multiple
The feedback methodology options fall into two supplies in a parallel configuration.
general categories, voltage mode control (VMC) and 3. The effects of the inductor in the control loop
current mode control (CMC). Both methodologies can be minimized since the current feedback loop
have their strengths and weakness that determine effectively reduces the compensation to a single
the appropriate selection for the end-equipment pole requirement.
application.
While current mode control addresses some of the
drawbacks of VMC, it introduces challenges that
Control Methodologies can effect the circuit performance. The addition of
Voltage mode control utilizes a scaled value the current feedback loop increases the complexity
of the output voltage as the feedback signal. of the control/feedback circuit and circuit analysis.
This methodology provides simple, straight–forward Stability across the entire range of duty cycles and
feedback architecture for the control path. However, sensitivity to noise signals are other items that need
this method has several disadvantages that should to be considered in the selection of current mode
be noted. The most significant disadvantage is output control. CMC can further be broken down into several
voltage regulation requires sensing a change in output different types of control schemes: peak, valley,
voltage and propagation through the entire feedback emulated, hysteretic, and average CMC. The below
signal and filter before the output is appropriately text discusses the two most common methodologies
compensated. This can generate a unacceptably used in circuit design — peak and average current
slow response for systems that desire high levels of mode control.
regulation. The feedback compensation of the supply
requires a higher level of analysis to address the Peak Current Mode Control
two poles introduced by the output low–pass filter.
Peak current mode control (PCMC) utilizes the current
Additionally, the feedback component values must
waveform directly as the ramp waveform into the
be adjusted since different input voltages affect the
PWM–generation comparator instead of a externally
overall loop gain.
generated sawtooth– or triangle–signal like VMC. The
Current mode control addresses the above short- up-slope portion of the inductor current or high–side
falls of voltage mode control by using the inductor transistor current waveform is used to provide a
current waveform for control. This signal is included fast response control loop in addition to the existing
with the output voltage feedback loop as a second, voltage control loop. As shown in Figure 1, the current
fast response control loop. The additional feedback signal is compared with the output of the voltage error
loop does potentially increase the circuit/feedback amplifier to generate the PWM control signal for the
complexity, so the advantages need to be evaluated power supply.
as part of the design requirements.
VIN INA240
+
By using the inductor current as part of the feedback RSENSE 
control:
1. The added current feedback loop responds faster Control and Gate Drive L C VOUT
compared with only using the output voltage for
feedback control. Additionally, with the inductor 
+
current information, the circuit can be designed PWM Generation 
+ Reference Voltage
to provide pulse by pulse current limiting allowing
Voltage Error Amplifier
rapid detection and control for current limiting
Figure 1. Block Diagram of PCMC circuit
needs.
SBOA187C – DECEMBER 2020 – REVISED MARCH 2022 Current Mode Control in Switching Power Supplies 40
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 41

Switching power supplies provide high levels of
efficiency between the input and output power rails.
To maintain the high efficiency of the converter, ideally
the sense resistor used to measure the inductor
current is as small as possible to reduce power
loss due to the measurement. This small–valued
resistor results in a small amplitude feedback signal.
Since the inductor current waveform is used directly
as the comparator input signal, PCMC is known
to be susceptible to noise and voltage transients.
Using a current sense amplifier like the INA240
with high common-mode rejection ratio (CMRR)
provides suppression of transients associated with
pulse-width modulation (PWM) signals and systems.
The gain flexibility of the INA240 allows the inductor
current waveform be amplified to provide a larger
signal for comparison without the need for additional
gain or sacrificing performance. Additionally, the low
offset and gain errors provide a reduction in design
variations and changes across temperature. To utilize
PCMC, the inductor current necessitates a high
common–mode voltage measurement. The INA240
common–mode range allows for a wide range in
supply input and output voltages.
It should be noted that PCMC most often adds slope
compensation to address stability issues with duty
cycles greater than 50%. The slope compensation is
added to the inductor current before being used as
the comparator input signal.
L VOUT
C
+ 
www.ti.com
INA240 high accuracy and low drift specifications
provide consistent measurement across temperature
and different assemblies.
The INA240 provides performance and features
for measurement accuracy which is needed to
maintain good control signal integrity. The INA240
features a 25μV maximum input offset voltage
and a 0.20% maximum gain error specification at
room temperature. Temperature stability is important
to maintain system performance and the INA240
provides input offset voltage drift of 250nV/°C with a
2.5ppm/°C amplifier gain drift. The INA240 features
enhanced PWM rejection to improve performance
with large common-mode transients and a wide
common-mode input range for maximum design
variance for supply output voltages.
Alternative Device Recommendations
Based on system requirements, alternative devices
are available that can provide the needed
performance and functionality. The LMP8601 family
provides lower performance levels than the INA240
for in-line sensing applications. The INA282
allows current measurement for high common-mode
voltages, making it ideal for high voltage DC
application that do not have PWM signals. The
INA290 is a high-voltage current sense amplifier with
high bandwidth packaged in a small SC-70 package.
VIN Table 1. Alternative Device Recommendations
Device Optimized Parameters Performance Trade-Off
Control and Gate Drive RSENSE Integrated Low inductive
+/-15A maximum
precision shunt: 2mΩ,
INA253
0.1%, Enhanced PWM
continuous current at TA
Current Error Amplifier INA240 = 85°C
+  rejection
 +  Low power, High Gain No Enhanced PWM
PWM Generation + Reference Voltage
INA282 Options, High Supply Rejection, Higher drift
Voltage Error Amplifier
Voltage specifications
Figure 2. Block Diagram of ACMC circuit
Wide Common–Mode No Enhanced PWM
Input Range, High Rejection, Unidirectional,
INA290
Average Current Mode Control bandwidth, Small SC-70 Common–mode range
package does not include ground
Average current mode control (ACMC) utilizes the
inductor current waveform and an additional gain
and integration stage before the signal is compared
Related Documentation
to a externally provided ramp waveform (similar to
1. Precision Brightness and Color Mixing in LED
VMC). This allows improved immunity to noise and
Lighting Using Discrete Current Sense Amplifiers
removes the need for slope compensation. Figure 2
2. Current Sensing in an H-Bridge
shows a block diagram of ACMC operation for a buck
3. Switching Power Supply Current Measurements
converter.
The noise sensitivity of PCMC methodology is
improved using ACMC to acceptable performance
levels with the INA240 high CMRR helping to provide
additional transient reduction. The INA240 high
common–mode range is required to make the inductor
current measurement and allows usage of the current
amplifier in a wide–range of output voltages. The
41 Current Mode Control in Switching Power Supplies SBOA187C – DECEMBER 2020 – REVISED MARCH 2022
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 42

www.ti.com
Application Brief
Switching Power Supply Current Measurements
There are many different switching power supply the current flowing through the high-side devices
topologies available to meet system power of the half H–bridge and are used primarily for
requirements. DC–DC switching converters reduce a overcurrent/short–circuit detection with a comparator.
higher voltage DC rail to a lower voltage DC rail. Any measurements being made at this node require
These converter architectures include buck, boost, high common-mode circuits with the performance to
buck-boost, and flyback topologies. DC–AC switching measure a small differential voltage.
converters convert a DC input voltage to an AC output
Node 2 is the mid-point of the half H–bridge and
voltage.
displays the pulse-width modulation (PWM) signal that
As implied by their name, switching converters switching power supplies are based around. Current
employ various switches, transistors/FETs and/or measurements at this location provide the inductor
diodes, to translate the input voltage to the desired current for system control and overcurrent/short–
output voltage at high system efficiency levels. circuit detection. The voltage transitions between
The switching nature of these converters present the upper voltage and ground (or negative supply)
challenges in trying to accurately measure the current in the PWM ratio that is averaged to produce the
waveforms. Voltage node requirements, system correct output voltage. Node 2 voltage will have sharp
control requirements, and measurement drift are common-mode transitions, so measurements here
areas to consider when selecting current sense need to be able to handle the transition voltage in
amplifiers. magnitude as well as suppressing the transient in the
output waveform.
Voltage Node Requirements Node 3 voltage is the converter output voltage, which
is a DC voltage level with a small voltage ripple
Each node in the circuit architecture has a different
when observed on oscilloscope. Measurements at this
common-mode voltage and behavior. Measuring
location will have similar requirements to Node 1 and
currents at each of these locations has different
provide the inductor current for use in system control
characteristics that the measurement circuit must take
and overcurrent/short–circuit detection. Even though
into consideration. Figure 1 illustrates the different
Node 3 voltage is less than Node 1, the desired
nodes of a buck/step-down converter. The circuit
output voltage level may still require measurement
shows a basic circuit consisting of a half H–bridge
circuitry to handle a high common-mode voltage.
output stage with a low-pass filter constructed from
an inductor and capacitor. The control circuitry, output Node 4 voltage is tied to ground of the circuit.
stage drivers, and load are not shown. This node will see low, close to ground, common-
mode levels so measurements at this location have
1
a reduced set of requirements compared with the
previously mentioned locations.
3
Other DC–DC switching architectures have similar
L behavior as the nodes described above, although they
may be at different locations in the converter circuitry.
2 C LOAD
4
Measurement Drift Requirements
Switching power supplies are highly efficient circuits
Figure 1. DC–DC Switching Power Supply – Buck for voltage level translation, but there are still power
Architecture losses in the conversion. These power losses are
system efficiency losses that manifest as thermal
Node 1 voltage is tied to the input supply of the generation or heat. Depending on the power levels of
converter. This is the high voltage the converter the converter, this can be a significant thermal source.
is “stepping-down” to the lower output voltage.
Current measurements at this node are measuring
SBOA176C – DECEMBER 2020 – REVISED MARCH 2022 Switching Power Supply Current Measurements 42
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 43

The INA240 has a-low thermal drift spec, which The current typically used is the inductor current in
means that the current measurement does not the converter (see Figure 2). This provides a much
change significantly due to heat generation. To faster internal loop to run in parallel with the voltage
further reduce the heat generated, the INA240 feedback loop. In general, one of the down sides of
comes in different gain versions, which allow current mode feedback is the susceptibility to noise/
for the decrease in value of the current sense transients on the signal.
resistor. Traditional amplifiers can have significant
decreases in performance as amplifier gain increases.
By contrast, all gains versions of the INA240
have excellent electrical specifications allowing the
achievement of high performance levels across L
C
different gain variants. Table 1 provides a comparison LOAD
of the power dissipation difference between gains.
Table 1. Power Dissipation Summary(1)
Gain
Parameter
20V/V 100V/V 200V/V
Input Voltage (mV) 150 30 15
RSENSE (mΩ) 15 3 1.5
Power dissipated (W) 1.5 0.30 0.15
(1) Full-scale output voltage = 3 V and current measurement =
10A
System Control and Monitoring Requirements
Most switching power supplies employ closed-
feedback systems to provide stable, well regulated
power. In order to provide optimized feedback control,
precision measurements are desired. Amplifier
specifications, like offset and gain errors, can
significantly influence the regulation ability of the
control system. Different feedback methods are used
depending on the system requirements and desired
complexity of the circuitry. Additionally, system power
monitoring is a growing need as designs optimize
and report the power consumption during different
operating modes of the end equipment.
Voltage mode feedback compares a scaled version
of the output voltage to a reference voltage to obtain
the error voltage. This feedback method is relatively
simple, but provides slow feedback as the system
must allow the output voltage to change before
adjustments can be made. Current measurements
for voltage mode feedback generally monitor the
load currents and determine if any short–circuits are
present. The most important current amplifier criteria
for voltage mode feedback converters is the common-
mode output voltage of the converter. The output
voltage on these converters ranges from low voltages
used for microprocessors and low voltage digital
circuitry (1.8 V to 5 V) to high voltages used for 48
V or higher systems. The output waveform, while after
the filter, may still contain noise/transients that can
disturb or cause errors in the measurement.
Current mode feedback adds a feedback loop to
the control system that utilizes the system current.
+ 
www.ti.com
RSENSE
INA240
To Feedback/Control Circuit
Figure 2. Current Sensing for Power Supply
Control Feedback
Current mode feedback is generally split into peak
current mode control and average current mode
control. Peak current mode control utilizes the
inductor current directly and therefore any noise
or transients on the signal cause disturbances in
the feedback loop. The INA240 is designed with
high CMRR, which helps to attenuate any potential
disturbances or noise due to the input signal.
Alternative Device Recommendations
Based on the system requirements, additional
devices are available that may provide the needed
performance and functionality. For applications
requiring the lower performance levels than the
INA240, use the INA290. The LMP8481 is a bi-
directional current sense amplifier used for high
common-mode voltages that do not require the
amplifier to include ground in the input voltage range.
Table 2. Alternative Device Recommendations
Device Optimized Parameters Performance Trade-Off
Integrated Low Inductive
INA253
Shunt: 2mΩ, PWM rejection
+/-15A at TA= 85°C
Wider Common-Mode Input
INA290 Range, Small Package No Enhanced PWM Rejection
(SC-70)
No Enhanced PWM Rejection,
Wide Common-Mode Input
LMP8481 Common-mode range does not
Range, Low power
include ground
Related Documentation
1. Precision Brightness and Color Mixing in LED
Lighting Using Discrete Current Sense Amplifiers
2. Current Sensing in an H-Bridge
3. Benefits of a Low Inductive Shunt for Current
Sensing in PWM Applications
43 Switching Power Supply Current Measurements SBOA176C – DECEMBER 2020 – REVISED MARCH 2022
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 44

____________________________________________________
Increase Measurement Accuracy Using High-Speed
Amplifiers for Low-Side Shunt Current Monitoring
Author: Leaphar Castro
to replicate the pulse accurately in a single gain stage.
The need to accurately and quickly detect the load
A possible solution is to use multiple gain stages with
current through a low-side shunt resistor is a critical
a lower bandwidth device, thus increasing the amount
application in systems requiring overcurrent, feedback
of components and potentially increasing the shunt
control loops, battery monitoring, and power-supply
resistance in order to use a smaller gain. By having a
monitoring. Load current is often measured using low-
large shunt-resistor, you introduce noise to your signal,
side current sensing, which is when the voltage is
increase the power dissipation, and cause ground
measured across a shunt-resistor that is placed
disturbances. Instead, an alternative solution is to use
between the load and ground. One common way to
a single high-speed amplifier. By using the high-speed
discretely implement low-side current monitoring is to
amplifier. you have more gain-bandwidth, which allows
use a current-sense amplifier in a difference
you to use a single high-gain stage with a small shunt-
configuration, as shown in Figure 1.
resistor. For current sensing applications, you want to
choose an amplifier with low offset and noise so that it
VCC VEE
does not degrade the accuracy of low-voltage
+ +
5 V 0 V measurements. Consider a widely used op-amp such
as the OPA365. This device has a maximum input
offset voltage as of 200 μV and an input voltage noise
GND GND
of 4.5 nV/√Hz at 100 kHz. Using an amplifier such as
OPA365 allows you to implement the circuit in a single
VCC 5 k
high-gain stage, save board space, keep the shunt-
resistor low, and enable driving an analog-to-digital
GND converter (ADC) with a single device. The OPA365 is
VCC
I_IN available in an AEC-Q100 version (OPA365-Q1) that
supports automotive applications.
100 
Choosing the correct amplifier simplifies detection of
high-current spikes that may cause damage to the
R_Sense 15 m OPA354 Out system or reduce motor and servo efficiency, all while
100  maximizing system efficiency. There are several
benefits to using a high-speed amplifier in current-
sensing circuit. For example, in applications such as
power-supply monitoring, the duration of the pulse
GND VEE may be as low as 1 μs. Without being able to detect
these transients, short-duration pulses may go
5 k unnoticed, and thus cause glitches or potential
damage to the rest of the system. As shown in Figure
Figure 1. Low-Side Current Sensing Test Circuit 2, with a short-duration 1-μs pulse input in a gain of
Using the OPA354 50, the OPA354 is able to reach 3-V output and is able
to replicate the original input signal much closer than a
Traditionally, this low-side current measurement is 400-kHz or a 20-MHz bandwidth op amp. As Figure 3
done with a dedicated current sense amplifier or a shows, 100-nA input pulse in a gain of 50 is
lower-speed amplifier connected to an external shunt- introduced, and again the output response of the
resistor. However, in applications that are required to OPA354 is much closer than the other two devices.
detect a small, high-speed transient pulse, these
amplifiers tend to lack the adequate bandwidth needed
SBOA298A–May 2018–Revised August 2019 Increase Measurement Accuracy Using High-Speed Amplifiers for Low-Side 44
Submit Documentation Feedback Shunt Current Monitoring Author: Leaphar Castro
Copyright © 2018–2019, Texas Instruments Incorporated

## Page 45

www.ti.com
microseconds. Use a high-speed amplifier for motor-
control applications to provide a fast and precise
current measurement for the best dynamic motor
control, minimum torque ripple, and minimal audible
noise.
For maximum system efficiency when using an op
amp to measure a small differential voltage signal from
the shunt-resistor, make sure the op amp has enough
bandwidth to make a precise and accurate
measurement without introducing error to the signal.
Measuring short duration pulses are a challenge, but
by using a high-speed amplifier, high slew rates and
plenty of bandwidth are available to track the input
Figure 2. Output Response With a 3-A, 1-μs Input signal. This article used the OPA354 as an example,
Into 15 mΩ With a Gain of 50 vs an Integrated and but there are many other available amplifiers offered
Lower-Bandwidth Device
by Texas Instruments. For automotive applications that
require AEC-Q100 devices, and applications that
require higher supply ranges or higher bandwidth, see
Table 1. The amplifiers listed inTable 1 give alternative
recommendations with optimized parameter benefits,
and what the performance trade-offs are of using each
of the devices.
Table 1. Alternative Device Recommendations
Performance
Device Optimized Parameters
Trade-Offs
IN to –V rail,
Higher bandwidth,
OPA836 slightly less output
lower power consumption
current
Figure 3. 100-nA Pulse Input Into 15 mΩ With a
Dual channel,
Gain of 50 vs an Integrated and Lower-Bandwidth higher bandwidth, IN to –V rail,
Device OPA2836-Q1 lower power consumption, slightly less output
current
automotive qualification
In another example, a three-phase inverter shunt- Higher bandwidth,
higher slew rate, Slightly higher
resistor is sensing large negative phase voltages. OPA354
higher output current, offset and power
These PWM duty cycles tend to be very small (around OPA354-Q1
automotive qualification consumption
2 μs). The current sense amplifier must be able to (Q1)
settle to < 1% in this time frame, and in many cases
Higher supply (maximum), Slightly higher
drive an ADC. In applications such as three-phase LMH6618 higher bandwidth, noise and less
inverters, maintain low distortion at the maximum rate lower I output current
Q
at which the output changes with respect to time. In Higher supply (maximum), IN to –V rail,
general, high-speed amplifiers offer slew rates > 25 LMH6611 higher bandwidth, slightly higher
V/μs and fast settling times of < 0.5 μs. These features higher slew rate power consumption
make high-speed amplifiers a great choice when a Single, dual, quad channels,
high rate of change exists in the output voltage that is LMH6642 higher supply (maximum), IN to –V rail,
lower I , slightly higher
caused by a step change on the input in the form of LMH6642Q-Q1 Q
automotive qualification noise
short current pulses. High slew rate, larger bandwidth, (Q1)
and fast settling high-speed amplifiers contribute to
keeping the detection time down to a few
45 Increase Measurement Accuracy Using High-Speed Amplifiers for Low-Side SBOA298A–May 2018–Revised August 2019
Shunt Current Monitoring Author: Leaphar Castro Submit Documentation Feedback
Copyright © 2018–2019, Texas Instruments Incorporated

## Page 46

Application Brief
Extending Beyond the Max Common-Mode Range of
Discrete Current-Sense Amplifiers
Arjun Prakash, Current Sensing Products
For high-side power supply current sensing needs, it is to use precision 0.1% matched low temperature
is critical to understand the maximum voltage rating drift external resistor dividers.
of the power supply. The maximum power supply
voltage will drive the selection of a current sense Power Supply
amplifier. The common mode voltage of the current
sense amplifier should exceed the maximum voltage
on the power supply. For example, if a current is
measured on the 48 V power supply with a transient
voltage not exceeding 96 V a current sense amplifier
with a maximum common mode voltage supporting 96 Load
V needs to be designed. Likewise for a 400 V supply
a common mode voltage of current sense amplifier
supporting 400 V needs to be chosen.
The system cost solution of high voltage, high-side
current sensing can be expensive considering a goal
of <1% accuracy needs to be achieved. For common
mode voltages higher than 90 V often the selection
of current sense amplifiers are limited to isolation
technology which can be expensive and BOM
extensive. Below are some of the techniques that
illustrates the extension of low voltage common mode
current sense amplifiers beyond its maximum rating
by adding a few inexpensive external components like
resistors, diodes and PMOS FETs.
Common Mode Voltage Divider Using Resistors.
The simplest approach to monitor high voltage high-
side current sensing is a design with a low voltage
current sensing amplifier with external input voltage
dividers, for example, if a 40 V common mode voltage
amplifier is selected for a 80 V application, the 80
V input common mode needs to be divided down to
40 V common mode voltage. This voltage division
can be accomplished using external resistor dividers
as shown in the Figure 1. This is a simple design
approach and the tradeoffs are significant. The gain
error and CMRR of the amplifier are dependent
on the accuracy and the matching of the external
input divider resistors. Apart from gain error and
CMRR errors, the tolerance of the external resistors
will contribute to imbalance in input voltage causing
additional output error. This error does increase over
temperature depending on the drift specifications of
the resistors. One technique to minimize output error
nuhsR
t
R1
R2
IN+
Vout = G x (V(IN+)  V(IN-))
R3 G = R2/R1 IN-
R4
Power Supply
Load
nuhsR
t
www.ti.com
RD1 R1 R2
V+
IN+ Vout = G x (VCMP - VCMN)
RD2 G = (R2/R1)
VCMP = V(+) x (RD2/(RD2 +RD1))
RD3 IN- R3 VCMN = V(-) x (RD4/(RD4 +RD3)) V-
RD4
R4
Figure 1. Extending Common Mode Range using
Resistor Dividers
Extending Common Mode Range for Current
Output Amplifiers
As voltage dividers has serious consequences with
output error and degradation in performance, another
alternative approach is to shift the ground reference
of the current output amplifier to the high voltage
common mode node as shown in Figure 2.
Figure 2 enables current sensing at higher voltages
beyond rated common mode voltage of INA168 is
60V. This technique can be extended to any voltage
beyond 60V by designing an appropriate PMOS FET
(Q1).
In Figure 2 Zener diode DZ1 regulates the supply
voltage that the current shunt monitor operates within,
and this voltage floats relative to the supply voltage.
DZ1 is chosen to provide sufficient operating voltage
for the combination of IC1 and Q1 over the expected
power-supply range (typically from 5.1 V to 56 V).
Select R1 to set the bias current for DZ1 at some
value greater than the maximum quiescent current of
SBOA198A – JULY 2017 – REVISED MARCH 2022 Extending Beyond the Max Common-Mode Range of Discrete Current-Sense 46
Submit Document Feedback Amplifiers
Copyright © 2022 Texas Instruments Incorporated

## Page 47

IC1. The INA168 shown in Figure 2 is specified at
90 μA maximum at 400 V. The bias current in DZ1
is approximately 1 mA at 400 V, well in excess of
IC1’s maximum current (the bias current value was
selected to limit dissipation in R1 to less than 0.1
W). Connect a P-channel MOSFET, Q1, as shown to
cascode the output current of IC1 down to or below
ground level. Transistor Q1’s voltage rating should
exceed the difference between the total supply and
DZ1 by several volts because of the upward voltage
swing on Q1’s source. Select RL, IC1’s load resistor,
as if IC1 were used alone. The cascode connection
of Q1 enables using IC1 well in excess of its normal
60-V rating. The example circuit shown in Figure 2
was specifically designed to operate at 400 V.
400V
Load
tnuhsR
to obtain accurate readings even at the low end of the
measurement. The voltage across R1 sets the drain
current of the FET and by matching the resistor R2
in the drain of the FET to be equal to R1, VSENSE
voltage is developed across R2 (VR2). Inputs of the
current monitor INA226 are connected across R2 for
current sensing. Hence the current monitor does not
need the high common mode capability as it will only
see common mode voltages around VSENSE which
is usually less than 100mV. INA226 was chosen for
current, voltage and power monitoring as it is a high
accuracy current/voltage/power monitor with an I2C
interface. The INA226 can also sense bus voltages
less than 36 V. Since the bus voltage employed here
is 400 V, a divider is employed to scale down the
high voltage bus to a voltage within the common
mode range of INA226. In this case a ratio of 64 is
chosen and hence the bus voltage LSB can be scaled
5k INA168 accordingly to obtain the actual bus voltage reading.
IN+
200uA/V
+ In this case the a modified LSB of 80 mV could be
used. Precision resistors are chosen for the divider to
DZ1
maintain accuracy of the bus measurement.
5k
-
IN-
Q1 400V
VOUT
R1
RL 10
50K
OPA333
10 K
Figure 2. High-Side DC Current Measurements for
400-V Systems
Load
Extending Common Mode Voltage Range for
Power Monitors
System optimization and power monitoring for high
voltage systems (40 V to 400 V), if implemented
accurately can result in improvement of overall
system power management and efficiency. Current,
voltage and system power information can be
beneficial in taking preventive steps to diagnose
faults or calculate total power consumption of the
system. Monitoring faults and power optimization
will assist high voltage system in premature failures
and significantly lower power savings by optimizing
system shut down and wake up.
Figure 3 illustrates a methodology by using INA226 a
36 V common mode voltage power monitoring device
to be used in applications supporting 40 V to 400 V
systems. Shown in Figure 2 is the precision, rail-to-rail
op amp OPA333 used to mirror the sense voltage
across the shunt resistor on to a precision resistor
R1. OPA333 is floated up to 400 V using a 5.1 V
zener diode between its supply pins. The op amp
drives the gate of the 600 V P-FET in a current
follower configuration. A low leakage P-FET is chosen
tnuhsR
Vsense
+ -
esnesV
+ 5.1V
Zener
IN+ VBUS SCL
- SDA
IN- GND
RZ
esnesV R
012
www.ti.com
-
+
Figure 3. High Voltage Power Monitoring
Table 1. Alternate Device Recommendations
Performance Trade-
Device Optimized Parameter
Off
Bandwidth : 900kHz,
LMP8645HV Slew rate: 0.5V/uS
Package: SOT-23-6
MSOP-8 Package, I2C
Gain Error (1%), Shunt
INA220 interface, Selectable
Offset Voltage: 100uV
I2C address
Package: SOT-23,
INA139 Bandwidth: 4400 kHz, Offset voltage: 1mV
cost
Related Documentation
1. Current Sensing in an H-Bridge
2. Switching Power Supply Current Measurements
3. High-Side Drive, High-Side Solenoid Monitor With
PWM Rejection
47 Extending Beyond the Max Common-Mode Range of Discrete Current-Sense SBOA198A – JULY 2017 – REVISED MARCH 2022
Amplifiers Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 48

Application Brief
Interfacing a Differential-Output (Isolated) Amp to a
Single-Ended Input ADC
Introduction
Whether you are sensing current in an industrial 3-
phase servo motor system, a battery management
system for an electric vehicle, or a photo voltaic
inverter, it is often necessary to include some sort
of safety isolation scheme. Safety-related standards
define the specific isolation requirements for the
end equipment associated with the particular design.
Various factors come into play when determining
what level of safety insulation (basic, supplemental,
or reinforced) is required depending on the type
of equipment, the voltage levels involved, and the
environment that the equipment is to be installed.
Texas Instruments offers a variety of isolated current
shunt amplifiers that are used in the previously-
mentioned applications for voltage and current shunt
sensing that meet either basic or reinforced insulation
requirements. For applications requiring reinforced
insulation, one such device is the AMC1301. The
output of the AMC1301 is a fully differential signal
centered around a common-mode voltage of 1.44 V
that can be fed directly to a stand-alone analog-to-
digital converter (ADC) as shown in Figure 1, or to
the on-board ADC found in the MSP430 and C2000
family of micro-controller devices.
Floating
HV+ Power Supply
3.3 V or AMC1301
Gate 5.0 V
Driver VDD1 VDD2 3.3 V or 5.0 V
GND1 GND2
RSHUNT VINN VOUTP
To Load ADS7263
14-Bit ADC
VINP VOUTN
Gate
Driver
HV-
noitalosI
decrofnieR
www.ti.com
Embedded ADCs
Both the MSP430 and C2000 family of processors
have embedded single-ended input ADCs so the
question becomes, How do I get this differential signal
into my single-ended data converter[unreadable glyph]
The simplest way to achieve this is to use only
one output of the AMC1301 leaving the second
output floating. The down side to this solution is
that only half the output voltage swing is available
to the data converter, reducing the dynamic range
of the measurement. The analog input range to the
AMC1301 is ±250 mV. With a fixed gain of 8.2, the
VOUTN and VOUTP voltages are ±1.025 V centered
around the 1.44-V common-mode output as shown in
Figure 2. Differentially, the output voltage is ±2.05 V.
Figure 2. Differential Output Voltage
The addition of a differential to single-ended amplifier
output stage, illustrated in Figure 3, allows the full
output range of the AMC1301 to be provided to the
ADC.
Figure 1. AMC1301 Functional Block Diagram
Figure 3. Differential to Single-Ended Output
SBAA229A – APRIL 2017 – REVISED MARCH 2022 Interfacing a Differential-Output (Isolated) Amp to a Single-Ended Input ADC 48
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 49

www.ti.com
Assuming a full scale sine wave of ±250 mV is applied Design Example
at VIN; the internal gain of the AMC1301 will provide
The ADC12 found on the MSP430 devices have
2.05 Vpk-pk outputs at points VOUTP and VOUTN
an input voltage range of 0–2.5 V when using the
which are 180° out of phase. The difference between
internal voltage reference. Using the VOUTP from the
these signals, VODIF, is 4.1 Vpk-pk. When R1 = R4
AMC1301 would provide the ADC12 with an input
and R2 = R3 the transfer function of the output stage
signal ranging from 0.415 V to 2.465 V, well within
is shown in Equation 1.
the input range of the converter while only utilizing
(cid:31) R4 (cid:30) (cid:31) R1 (cid:30) half the input range of the AMC1301. As shown
VOUT (cid:29) VOUTP (cid:28) (cid:26) (cid:25) (cid:27) VOUTN(cid:28) (cid:26) (cid:25) (cid:27) VCM in Figure 5, utilizing a differential to single-ended
(cid:24) R3 (cid:23) (cid:24) R2 (cid:23)
(1)
amplifier configuration with a gain of 0.5 and common
mode voltage of 1.25 V, the entire voltage range of
With equal value resistors for R1 through R4 in
the AMC1301 can be applied to the ADC12.
Equation 1 and VCM set to 2.5 V, Equation 2 reduces
to:
VOUT (cid:29) (cid:31)VOUTP (cid:28) VOUTN(cid:30)(cid:27) VCM
(2)
The plots of Figure 4 show the input voltage and
output voltages of the AMC1301 along with the output
voltage of the final differential to single-ended output
stage. Note that the differential voltage of ±2.05 V is
transposed to a single-ended signal from 0.5 to 4.5 V.
Figure 5. Scaled Differential to Single-Ended
Output
Alternative Device Recommendations
The AMC1100 or AMC1200 provide basic isolation
with similar performance to the AMC1301 at a lower
price point. For applications requiring a bipolar output
option, the TLV170 is an excellent choice.
Table 1. Alternative Device Recommendations
Device Optimized Parameter Performance Trade-Off
AMC1100 Galvanic Isolation up to 4250 Lower Transient Immunity
VPEAK
AMC1200 Galvanic Isolation up to 4250 Basic Isolation versus
VPEAK Reinforced
TLV170 Bi-polar operation to ±18 V Higher input bias current
Figure 4. Single-Ended Output Voltage Conclusion
Depending on the input voltage range of the ADC, While it is possible to use a single output of the
gain or attenuation can be incorporated into the AMC1301 to drive a single-ended ADC, adding a
differential to single-ended stage to adjust the output differential to single-ended op-amp stage at the output
swing. The output common mode voltage can be ensures the target application will have the largest
adjusted to fit the input needs of the ADC as well. possible dynamic range.
Related Documentation
1. Low-Drift, Low-Side Current Measurements for
Three-Phase Systems
2. Precision Current Measurements on High Voltage
Power Supply Rails
49 Interfacing a Differential-Output (Isolated) Amp to a Single-Ended Input ADC SBAA229A – APRIL 2017 – REVISED MARCH 2022
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 50

Low-Drift, Precision, In-Line Isolated Magnetic Motor
Current Measurements
Steven Loveless - Current Sensing Products
leadframe, which creates an internal magnetic field. A
The demand for higher efficiency systems continues to
galvanically isolated sensor then measures the
increase, leading to direct pressure for improvement in
magnetic field, providing a measurement of the current
motor operating efficiency and control. This focus
without any direct electrical connection between the
applies to nearly all classes of electric motors,
sensor IC and the isolated phase current. By
including those used in:
measuring only the magnetic field, the sensor provides
• White goods isolation to high common-mode voltages, as well as
• Industrial drives excellent immunity to PWM switching transients. This
results in excellent motor phase current measurements
• Automation
without unwanted disturbances at the sensor output
• Automotive applications due to large, PWM-driven input voltage steps. Figure 2
illustrates an RC-filtered TMCS1100 output waveform,
This is especially true in higher-power systems with
along with the motor phase voltage and current
elevated operating voltages. Operational
waveforms. Only minor PWM-coupling due to
characteristics of the motor fed back into the control
measurement parasitics are observable, and the
algorithm are critical to ensure the motor is operating
TMCS1100 output tracks the motor phase current with
at peak efficiency and performance. Phase current is
no significant output transients due to the 300-V
one of these critical diagnostic feedback elements
switching events.
used by the system controller to enable optimal motor
performance.
Due to the continuity of the measurement signal and
direct correlation to the phase currents, an ideal
location to measure the motor current is directly in-line
with each phase, as shown in Figure 1. Measuring
current in other locations, such as the low-side of each
phase, requires recombination and processing before
meaningful data can be used by the control algorithm.
Figure 2. Motor Phase Current Measurement with
High Transient Immunity
The unique characteristics of an in-package magnetic
To Controller
current sensor eliminate many of the challenges faced
Figure 1. In-Line Current Sensing by alternative solutions to measuring motor phase
currents. The inherent galvanic isolation provides
capability to withstand high voltage, and the high
The drive circuitry for the motor generates pulse width
transient immunity of the output reduces output noise
modulated (PWM) signals to control the operation of
due to switching events. Current sensing
the motor. These modulated signals subject the
implementations without this immunity require higher
measurement circuitry placed in-line with each motor
bandwidth in order to improve output glitch settling
phase to large voltage transients that switch between
time; a magnetic sensor can use a lower-bandwidth
the positive and negative power rails every cycle. An
signal chain without sacrificing transient immunity
ideal current sensor has the ability to completely reject
the common-mode voltage component of the
measurement, and only measure the current of
interest. In-package magnetic current sensors like the
TMCS1100 pass the phase current through a package
SBOA351–April 2019 Low-Drift, Precision, In-Line Isolated Magnetic Motor Current Measurements 50
Submit Documentation Feedback Steven Loveless - Current Sensing Products
Copyright © 2019, Texas Instruments Incorporated

## Page 51

Temperature (C)
)%(
rorrE
ytivitisneS
Temperature (C)
1
0.5
0
-0.5
-1
-40 -20 0 20 40 60 80 100 120
D006
)Vm(
tesffO
lacipyT
www.ti.com
performance. In-package magnetic current sensors temperature drift of the sensor. In addition to high-
also provide a reduction in total solution cost and sensitivity accuracy, the device has less than 2 mV of
design complexity due to no requirement for external output offset drift, shown in Figure 4, which greatly
resistive shunts, passive filtering, or isolated power improves measurement dynamic range, and allows for
supplies relative to the high voltage input. precise feedback control even at light loads.
For applications where phase current measurements 5
provide over-current protection or diagnostics, the high TMCS1100A1
4.5 TMCS1100A2
transient rejection of a magnetic current sensor
4 TMCS1100A3
prevents false overcurrent indications due to output TMCS1100A4
glitches. In motor systems where closed loop motor 3.5
control algorithms are used, precise phase current 3
measurements are needed in order to optimize motor
2.5
performance. Historically, Hall-based current sensors
2
have suffered from large temperature, lifetime, and
hysteresis errors that degrade motor efficiency, 1.5
dynamic response, and cause non-ideal errors such as 1
torque ripple. Common system-level calibration 0.5
techniques can improve accuracy at room
0
temperature, but accounting for temperature drift in -40 -20 0 20 40 60 80 100 120
parameters, such as sensitivity and offset, is
D009
challenging.
Figure 4. TMCS1100 Typical Output Offset Across
Magnetic current sensing products from Texas Temperature
Instruments improve system-level performance by
incorporating patented linearization techniques and Combining high-sensitivity stability and a low offset
zero-drift architectures that provide stable, precise results in an industry-leading isolated current sensing
current measurements across temperature. A high- solution with <1% total error across the full
precision sensor tightly controls phase-to-phase temperature range of the device. A 600-V working
current measurement errors, maintaining accurate voltage and 3 kV isolation barrier allows the device to
feedback control and delivering a seamless user fit into a wide array of high voltage systems.
experience. Combining measurement temperature stability,
galvanic isolation, and transient PWM input rejection,
the TMCS1100 is an ideal choice for PWM-driven
applications, such as motor phase current
measurements, where accurate and reliable
measurements are required for precisely controlled
performance.
Table 1. Alternate Device Recommendations
Device Optimized Parameter Performance Trade-Off
Magnetic Current Sensor with
TMCS1101 Lower precision, PSRR
Internal Reference
Reinforced Isolation Shunt
AMC1300 Solution size, complexity
Amplifier
Precision Shunt Amplifier with
INA240 80V functional isolation PWM Rejection
Precision Integrated-Shunt
INA253 80V Functional isolation, size
Figure 3. TMCS1100 Typical Sensitivity Error Amplifier with PWM Rejection
Across Temperature
Table 2. Related TI TechNotes
The TMCS1100 features less than 0.3% typical
Lit # Title
sensitivity error at room temperature, and less than
Ratiometric Versus Non-Ratiometric Magnetic Signal
0.85% maximum sensitivity error across the entire SBOA340 Chains
temperature range from –40°C to 125°C. This stability Low-Drift, Precision, In-Line Motor Current Measurements
SBOA160
across temperature, shown in Figure 3, provides With PWM Rejection
excellent phase-to-phase matching by minimizing the SBOA161 Low-Drift, Low-Side Current Measurements for Three-
Phase Systems
High-Side Motor Current Monitoring for Over-Current
SBOA163
Protection
51 Low-Drift, Precision, In-Line Isolated Magnetic Motor Current Measurements SBOA351–April 2019
Steven Loveless - Current Sensing Products Submit Documentation Feedback
Copyright © 2019, Texas Instruments Incorporated

## Page 52

Application Brief
Current Sensing in Mobile Robots
Kyle Stone Current Sensing
Introduction
Mobile robots are widely used in warehouse settings. These mobile robots help to increase efficiency within
warehouses by moving quickly around a facility and collecting items that are needed to fulfill orders in a timely
manner. By using these mobile robots, warehouse operators can have reduced overhead cost, a continuous
faster output, and warehouses can potentially be organized in more efficient configurations, as these mobile
robots can reach places humans cannot easily access.
Mobile robots start at their home base where robots are charged and are stored. When the robot is called upon,
the robot can move around the warehouse to pick up the requested items, and bring those items to a desired
location, where either a human or another robot such as industrial robot can take the items to an assembly
line or shipping line. The mobile robot typically uses brushless DC motors to move. An accurate phase current
sensing is critical in motor control. A poor current sensing can lead to large torque ripple, audible noise, and
inefficiency. Figure 1 illustrates where current sensing plays a role in motor-drive applications.
Vbus
+ + +
– – –
D1 D3 D5
Q1 Q3 Q5
I_U U
I_V V
I_W W
D2 D4 D6
Q2 Q4 Q6
+ + +
– – –
–
–
+
+
– + – + – +
www.ti.com Application Brief
High-side DC link
High-side
Inline
Low-side
Low-side DC link
Figure 1. Methods of Motor Current Sensing in Mobile Robots
There are three methods of motor current sensing with their own positives and negatives as shown in Table
1. The best location to measure the motor current is directly inline with each phase due to continuity of signal
measurement and direct correlation to the phase currents. In-line current sensing comes with the challenge
where each motor phase is subjected to common-mode voltage transitions that can switch between large
voltage levels over very short time periods. Current measurement in other locations, such as the low-side
of each phase, requires recombination and processing before meaningful data can be utilized by the control
SBOA554 – NOVEMBER 2022 Current Sensing in Mobile Robots 1
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 53

Application Brief www.ti.com
algorithm. INA241A is designed for in-line motor current sensing. INA241A offers high CMRR of 166 dB and
enhanced PWM rejection allowing for minimal error at the output in switching systems.
Table 1. Current Sensing in an H-Bridge
Current Measurement Positives Negatives
High-side Detect shorted load from battery for diagnostics High voltage common-mode amplifier
Direct motor current measurement, low bandwidth
Inline High dv/dt signals. PWM settling time.
amplifier
Low-side Low cost, low common-mode voltage Unable to detect shorted load.
The mobile robots typically have onboard 48-V lithium-ion batteries, and can be charged wirelessly or wired.
Charging currents and discharging currents can be monitored through Texas Instruments’ line of digital power
monitors such as the INA228 and INA238. The INA228 can monitor current, voltage, power, energy, or charge
up to 85-V common-mode input range with 20-bit resolution through an I2C output. This allows for the robot to
monitor the battery levels to the onboard computer. INA228 can monitor battery level and when the battery level
runs low, the mobile robot can report back to the home base to be re-charged.
There are several other subsystems within the mobile robot, such as DC/DC converters, onboard computers,
lighting systems, radars, and so forth, that can use current sensing. DC/DC converters can use current monitors
in the system such as the INA296A to monitor the safe level of current and to make the system more efficient.
The onboard computer is the brain of the mobile robot, and typically operates on a 5-V rail. The onboard
computer can leverage current sensing with a digital power monitor such as the INA219, to monitor current and
voltage into the system to report back health or power usage in the system. Current monitors can also be used
to monitor various peripherals on the robot such as the lighting and radar systems.
Using current monitors can verify these subsystems are operating correctly for safely preventing an incident
with a human or physical structure. Monitoring current through the peripheral systems can enable the system
to detect a malfunction and can have the robot return to home base for maintenance. The INA281 or INA296A
can be used to monitor the current flow in these peripherals. Figure 2 shows one possible way current sensing
can be used in point-of-load (POL) applications where understanding the current going to a specific load in the
system is important. This can be used for diagnostics, safety, and protection.
2 Current Sensing in Mobile Robots SBOA554 – NOVEMBER 2022
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 54

www.ti.com Application Brief
Shunt resistor
Load
3.3V DC/DC
Load switch
Digital power monitor
Shunt resistor
Load
5V DC/DC
EMI filter
Main switch Load switch
Battery
Digital power monitor
Shunt resistor
Load
12V DC/DC
Load switch
Digital power monitor
Figure 2. Power Monitoring at POL for Power System in Robotics
Conclusion
In conclusion, mobile robots are generally used in warehousing settings to reduce human involvement and
speed up the movement of goods within the facility. Safety and efficiency are critical components of mobile
robots. Leveraging various current-sensing techniques within the subsystems in motors, batteries, onboard
computers, and peripheral systems can help these robots to operate safely and efficiently.
Alternate Device Recommendations
The INA241B is a high-precision, accurate analog current-sense amplifier. INA241B can be used in high-voltage
bidirectional applications paired with 1.1-MHz bandwidth to offer fast response time with precise control for
in-line control within H-bridge applications. The INA241B can measure currents at common-mode voltages of –5
V to 110 V and survive voltages between –20 V to 120 V, making the INA241B device an excellent choice for
many systems.
The INA253 is an ultra-precise current-sense amplifier with an integrated low-inductive, precision 2-mΩ shunt
with an accuracy of 0.1% and a temperature drift of < 15 ppm/°C. The INA253 is limited to applications that need
< ±15 A of continuous current at T = 85°C. The INA253 integrated shunt is internally Kelvin-connected to the
A
INA240 amplifier. The INA253 provides the performance benefits of the INA240 amplifier with the inclusion of a
precision shunt providing a total uncalibrated system gain accuracy of < 0.2%.
The INA281 can be used in high-voltage applications such as high-side current sensing in a motor. The INA281
can measure currents at common-mode voltages of –4 V to 110 V and survive voltages between –20 V to 120 V,
making the device versatile for a variety of applications where voltage can swing negative.
An option for low-side sensing is the INA381 which is a cost-optimized current-sense amplifier with an integrated
comparator which serves to reduce PCB footprint size and simplifies design.
SBOA554 – NOVEMBER 2022 Current Sensing in Mobile Robots 3
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 55

Application Brief www.ti.com
Table 2. Alternate Device Recommendations
Device Optimized Parameter Performance Trade-Off
INA241B Vcm range: –5 V to 110 V Bidirectional Slightly lower accuracy compared to INA241A
INA281 Vcm range: –4 V to 110 V Unidirectional
INA381 Integrated Comparator Vcm limited to 26 V
INA253 Integrated shunt 2 mΩ, VCM range: –4 V to 80 V ±15-A maximum continuous current
Table 3. Related TI Application Briefs
Literature Number Description
Low-Drift, Precision, In-Line Motor Current Measurements With PWM
SBOA160
Rejection
SBOA176 Switching Power Supply Current Measurements
SBOA163 High-Side Current Overcurrent Protection Monitoring
SBOA555 Current Sensing in Collaborative and Industrial Robotic Arms
4 Current Sensing in Mobile Robots SBOA554 – NOVEMBER 2022
Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 56

IMPORTANT NOTICE AND DISCLAIMER
TI PROVIDES TECHNICAL AND RELIABILITY DATA (INCLUDING DATA SHEETS), DESIGN RESOURCES (INCLUDING REFERENCE
DESIGNS), APPLICATION OR OTHER DESIGN ADVICE, WEB TOOLS, SAFETY INFORMATION, AND OTHER RESOURCES “AS IS”
AND WITH ALL FAULTS, AND DISCLAIMS ALL WARRANTIES, EXPRESS AND IMPLIED, INCLUDING WITHOUT LIMITATION ANY
IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT OF THIRD
PARTY INTELLECTUAL PROPERTY RIGHTS.
These resources are intended for skilled developers designing with TI products. You are solely responsible for (1) selecting the appropriate
TI products for your application, (2) designing, validating and testing your application, and (3) ensuring your application meets applicable
standards, and any other safety, security, regulatory or other requirements.
These resources are subject to change without notice. TI grants you permission to use these resources only for development of an
application that uses the TI products described in the resource. Other reproduction and display of these resources is prohibited. No license
is granted to any other TI intellectual property right or to any third party intellectual property right. TI disclaims responsibility for, and you
will fully indemnify TI and its representatives against, any claims, damages, costs, losses, and liabilities arising out of your use of these
resources.
TI’s products are provided subject to TI’s Terms of Sale or other applicable terms available either on ti.com or provided in conjunction with
such TI products. TI’s provision of these resources does not expand or otherwise alter TI’s applicable warranties or warranty disclaimers for
TI products.
TI objects to and rejects any additional or different terms you may have proposed. IMPORTANT NOTICE
Mailing Address: Texas Instruments, Post Office Box 655303, Dallas, Texas 75265
Copyright © 2022, Texas Instruments Incorporated

## Page 57

Application Brief
Current Sensing in Collaborative and Industrial
Robotic Arms
Kyle Stone, Guang Zhou, Sanjeev Manandhar Current Sensing
Introduction
Collaborative robots and Industrial robots are the dominant robot technologies driving factories toward
automation and increasing efficiency and throughput of the factories. Factories are able to build their end
products at an ever-faster rate to meet the demand of the customers by implementing these semi- and full-
factory automation technologies.
Collaborative Robots
Collaborative robots are human-assisted robots that typically work only with human interactions. These robots
assist humans in lifting payloads in the range of 6 kg to 25 kg and greater to help reduce human fatigue,
while increasing production speed. The collaborative robot is typically operating at voltages lower than 80 V,
with most systems operating in the 48-V to 50-V range due to direct human interaction. These systems are
sometimes isolated and requirements are based on the system design. Typical current ranges for these systems
are between 10 A to 30 A depending on the circuit node. Current sensing in collaborative robots is typically
used for motor control within the axis, where a device is monitoring the in-phase current of the electric motors as
shown in Figure 1.
Vbus
+ + +
– – –
D1 D3 D5
Q1 Q3 Q5
I_U U
I_V V
I_W W
D2 D4 D6
Q2 Q4 Q6
+ + +
– – –
–
–
+
+
– + – + – +
www.ti.com Application Brief
High-side DC link
High-side
Inline
Low-side
Low-side DC link
Figure 1. Methods of Motor Current Sensing in Collaborative and Industrial Robots
An accurate phase current sensing is critical in motor control. Poor current sensing can lead to large torque
ripple, audible noise, and inefficiency. There are three methods of motor current sensing with their own positives
SBOA555 – NOVEMBER 2022 Current Sensing in Collaborative and Industrial 1
Submit Document Feedback Robotic Arms
Copyright © 2022 Texas Instruments Incorporated

## Page 58

Application Brief www.ti.com
and negatives as shown in Table 1. The best location to measure the motor current is directly inline with each
phase due to the continuity of signal measurement and direct correlation to the phase currents. In-line current
sensing comes with the challenge where each motor phase is subjected to common-mode voltage transitions
that can switch between large voltage levels over very short time periods. Current measurement in other
locations, such as the low-side of each phase, requires recombination and processing before meaningful data
can be utilized by the control algorithm.
Table 1. Current Sensing in an H-Bridge
Current Measurement Advantages Disadvantages
High-side Detect shorted load from battery for diagnostics High-voltage common-mode amplifier
Inline Direct motor current measurement, low bandwidth amplifier High dv/dt signals. PWM settling time.
Low-side Low cost, low common-mode voltage Unable to detect shorted load
Board space is highly constrained in collaborative robots, so a small-footprint device is desired. Since isolation
is typically not a requirement, a device such an the INA241A is an excellent choice for an in-line current-sensing
application because the INA241A offers high CMRR and enhanced PWM rejection allowing for minimal error in
switching systems in a collaborative robot. The INA241A is packaged in a small SOT-23 (DDF) 8-pin package
measuring 2.95 mm × 2.95 mm (8.70 mm2) with the leads. If the application does require isolation, then a device
such as the TMCS1108 (a Hall-effect current sensor), provides functional isolation up to 100 V.
Industrial Robots
Industrial robots are very large robots that are found in car factories and other manufacturing facilities where
heavy lifting is required to pick and place large products. Industrial robots help bring a level of safety to a
manufacturer because these robots are moving very large items without the need for human interaction, and can
be done in a very controlled manner. The industrial robots typically operate in the AC domain with voltage levels
between 400 V to 600 V, which are common AC voltage levels in large manufacturing facilities. The current
levels generally range from 35 A to 250 A dependent upon what the robot is moving around the facility. Current
sensing is typically done within a cabinet near the robot, so space is less of a concern in these systems. The
current monitoring is usually done in-phase with the motors controlling the robot. Given the high-voltage present
in industrial systems, isolation is generally required to separate the high-voltage levels that are dangerous to the
low-voltage circuit systems and to humans. Today, current sensing is typically done with isolated amplifiers such
as the AMC1306.
One other common place where current sensing is required in industrial robots is in the coil drive brake release
unit. Coil drive is used for releasing the brake friction disks once the motor moving the joints has stopped or to
provide dynamic stopping in an emergency event. The magnetic field in the coil drive keeps two spring-loaded
friction disks away from each other so that rotor can turn freely. When flow of the current stops, the two braking
friction disks are pressed against each other with the help of the loaded springs to stop the rotor. Figure 2
shows a low-side current sensing used in a coil drive in a brake release unit. Low-side current sensing gives
more choices of current-sensing devices but due to the inductive nature of the coil, inputs of the current-sense
amplifier can be subjected to negative voltage during switching. In such an event, a current-sensing device is
required to survive the negative input voltage.
2 Current Sensing in Collaborative and Industrial SBOA555 – NOVEMBER 2022
Robotic Arms Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 59

www.ti.com Application Brief
Friction
Disks
Spring
Rotor and Shaft
Field coil
+
–
Current sense
amplifier
Figure 2. Low-Side Current Sensing in Coil Drive for Brake Release Unit
Conclusion
In conclusion, both collaborative and industrial robots generally both use current sensing to monitor the in-phase
current of the electric motor in the various robots. The main differences are the need for isolation, the level of
power, and the amount of weight the robot can lift, hence the difference in requirements for current-sensing
devices. Industrial robots require current sensing in the coil drive to brake and stop motor. Collaborative robots
generally do not need isolation, have power levels that do not exceed 2.0 kW, and typically lift payloads between
6 kg to 25 kg. While industrial robots generally do need isolation, their power levels that can range from as little
as 3 kW up to and beyond 125 kW, and lift from 20 kg to a few thousand kilograms.
Alternate Device Recommendations
The INA241B is a high-precision, accurate analog current-sense amplifier. The INA241B can be used in high-
voltage bidirectional applications paired with 1.1-MHz bandwidth to offer fast response times with precise control
for in-line control within H-bridge applications. The INA241B can measure currents at common-mode voltages of
–5 V to 110 V and survive voltages between –20 V to 120 V, making the INA241B an excellent choice for many
systems.
The INA253 is an ultra-precise, current-sense amplifier with an integrated low inductive, precision 2-mΩ shunt
with an accuracy of 0.1% and a temperature drift of < 15 ppm/°C. The INA253 is limited to applications that need
< ±15 A of continuous current at T = 85 °C. The INA253 integrated shunt is internally Kelvin-connected to the
A
INA240 amplifier. The INA253 provides the performance benefits of the INA240 amplifier with the inclusion of a
precision shunt providing a total uncalibrated system gain accuracy of < 0.2%.
The INA281 can be used in high-voltage applications such as high-side current sensing in a motor. The INA281
can measure currents at common-mode voltages of –4 V to 110 V and survive voltages between –20 V to 120 V,
making the INA281 versatile for a variety of applications where voltage can swing negative.
An option for low-side sensing is the INA381. This device is a cost-optimized current-sense amplifier with an
integrated comparator which serves to reduce the PCB footprint size and simplify the design.
Table 2. Alternate Device Recommendations
Device Optimized Parameter Performance Trade-Off
INA241B Vcm range: –5-V to 110-V Bidirectional Slightly lower accuracy compared to INA241A
INA281 Vcm range: –4 V to 110 V Unidirectional
INA381 Integrated Comparator Vcm limited to 26 V
INA253 Integrated shunt 2 mΩ, VCM range: –4 V to 80 V ±15-A maximum continuous current
SBOA555 – NOVEMBER 2022 Current Sensing in Collaborative and Industrial 3
Submit Document Feedback Robotic Arms
Copyright © 2022 Texas Instruments Incorporated

## Page 60

Application Brief www.ti.com
Table 3. Related TI Application Briefs
Literature Number Description
Low-Drift, Precision, In-Line Motor Current Measurements With PWM
SBOA160
Rejection
SBOA176 Switching Power Supply Current Measurements
SBOA163 High-Side Current Overcurrent Protection Monitoring
SBOA554 Current Sensing in Mobile Robots
4 Current Sensing in Collaborative and Industrial SBOA555 – NOVEMBER 2022
Robotic Arms Submit Document Feedback
Copyright © 2022 Texas Instruments Incorporated

## Page 61

www.ti.com
Application Brief
How to optimize PoE systems using discrete current
sensing
Power over Ethernet (PoE) describes several Camera
IR LEDs
standards that pass power along with data on PTZ Motor
twisted-pair Ethernet cabling. PoE allows a single
cable to provide both data connection and power PoE PD DC/DC
Controller Controller
to devices such as wireless access points (WAPs),
Internet Protocol (IP) cameras, and voice-over-
MCU
Internet Protocol (VoIP) phones.
Power sourcing equipment (PSE) such as network
Figure 1. Current sensor in a PoE-powered device
switches or video recorders are used to power
(IP camera)
powered devices (PDs). However, many PDs have
an auxiliary power connector for an optional external Flyback converters are a common choice for DC/DC
power supply, which often acts as backup power. conversion in low-power applications like PDs due
to their low cost and BOM count. These DC/DC
PoE offers several classes from one to eight, allowing
converters reduce the voltage from 48 V to 12 V.
input powers between 4W and 71W. Powered devices
Current sensors can be placed in several locations
must be designed so that they do not exceed the
around the flyback transformer before the load.
PoE class specification chosen. If a powered device
Measuring current on the primary and high-voltage
exceeds its PoE class limit, the PSE turns off the PD.
side of the flyback offers the greatest accuracy as
There are several methods to avoid PSE shutdown. the power seen here includes losses in the DC/DC.
One solution would be to characterize the load by However, designers must select a higher common-
measuring the maximum power consumption of every mode input range capable device like the INA238 to
subsystem in the PD, and designing for a PoE accommodate the 44-V to 57-V rail.
class that is within that characterized peak power to
Low-side current sensing on the primary side offers
guarantee that the powered device never exceeds the
diminished accuracy as this option does not include
PoE class limit. However, this method is not ideal
power losses in the transformer. Still, these current
because it requires more characterization and the
sensing devices (like the INA180) do not require high
selection of an arbitrarily high PoE class.
common-mode voltage capability. Current sensing
Suppose designers chose not to characterize their on the secondary side of the transformer also
systems for the highest possible power consumption allows designers to choose lower common-mode
and want to dynamically control system power input voltage devices, but the sensor will not include
consumption to stay within a lower PoE class limit, power losses in the flyback converter. This leads
in this case, their circuit design would require current to diminished current sensing accuracy because
sensors. designers must estimate or characterize power
losses in their DC/DC converter to know the power
Current sensors can monitor DC/DC converter current
consumption of the entire PD.
and report to an MCU or PMIC that controls the
various subsystems within a PD, throttling power, and The next selection is the technology of the current
cycling functionality to avoid exceeding the PoE class sensor. Digital power monitors like the INA219 can
limit. measure bus voltage and current, multiplying these to
output a power calculation over I2C. Having power
information means designers can more accurately
and easily know how close the powered device is to
the PoE class limit.
The second option is to use an analog-output current
sense amplifier like the INA180 which will not give bus
SBOA561 – JANUARY 2023 How to optimize PoE systems using discrete current sensing 1
Submit Document Feedback
Copyright © 2023 Texas Instruments Incorporated

## Page 62

www.ti.com
voltage information, so if the rail voltage fluctuates,
designers will not be able to know the total power
consumption of the PD with as great accuracy.
Ultimately, this power or current information
communicates to a microcontroller or SoC. This
microcontroller or SoC is responsible for making
decisions about turning off or throttling power and
current to downstream electronic subsystems when
current reaches too close to the class limit. So in an
IP camera, for example, the MCU or SoC could turn
off the pan-tilt-zoom (PTZ) motor, heater, or IR LEDs
when power consumption gets too close to the class
limit.
Current sensors can be used for other uses
in powered devices. Specifically for IP cameras,
subsystem current can be monitored. Often IP
cameras are used outside in cold weather conditions
and heaters must be implemented. Power is a
leading indicator for temperature for resistive heating,
therefore digital power monitors or current sense
amplifiers can be used to actively control heating
elements. Current sensing can also be used for safety
and diagnostics in PTZ servo/stepper motors.
Additionally, IP cameras often have auxiliary power
inputs in the form of 24 V and 12V coaxial inputs.
ac DC
Current sense amplifiers can be used with a FET in
a simple e-fuse configuration on these input rails to
monitor if current exceeds a threshold, triggering the
FET and turning the device off to protect downstream
electronics.
Related Documentation:
1. Texas Instruments, Getting Started with Digital
Power Monitors application note
2. Texas Instruments, Integrating the Current
Sensing Signal Path application brief
2 How to optimize PoE systems using discrete current sensing SBOA561 – JANUARY 2023
Submit Document Feedback
Copyright © 2023 Texas Instruments Incorporated

## Page 63

Additional resources
Current Sense Amplifier Overview
Explore TI’s portfolio of current-sense amplifiers.
Current Sense Amplifier Comparison and Error Calculator
Compare up to five of TI’s current-sense amplifiers and calculate the residual sum-of-squares error based on the devices
selected and user-input system level parameters.
Power Monitor Tool
Input design constraints to determine the correct digital power monitor to use.
E2E is trademark of Texas Instruments.
All other trademarks are the property of their respective owners.
An© E n2g02in2e Teerx’ass GInustidruem teon tCs uInrcroernpto rSateendsing 65 Texas Instruments InScLoYrYp1o5r4aBted

## Page 64

IMPORTANT NOTICE AND DISCLAIMER
TI PROVIDES TECHNICAL AND RELIABILITY DATA (INCLUDING DATA SHEETS), DESIGN RESOURCES (INCLUDING REFERENCE
DESIGNS), APPLICATION OR OTHER DESIGN ADVICE, WEB TOOLS, SAFETY INFORMATION, AND OTHER RESOURCES “AS IS”
AND WITH ALL FAULTS, AND DISCLAIMS ALL WARRANTIES, EXPRESS AND IMPLIED, INCLUDING WITHOUT LIMITATION ANY
IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT OF THIRD
PARTY INTELLECTUAL PROPERTY RIGHTS.
These resources are intended for skilled developers designing with TI products. You are solely responsible for (1) selecting the appropriate
TI products for your application, (2) designing, validating and testing your application, and (3) ensuring your application meets applicable
standards, and any other safety, security, regulatory or other requirements.
These resources are subject to change without notice. TI grants you permission to use these resources only for development of an
application that uses the TI products described in the resource. Other reproduction and display of these resources is prohibited. No license
is granted to any other TI intellectual property right or to any third party intellectual property right. TI disclaims responsibility for, and you
will fully indemnify TI and its representatives against, any claims, damages, costs, losses, and liabilities arising out of your use of these
resources.
TI’s products are provided subject to TI’s Terms of Sale or other applicable terms available either on ti.com or provided in conjunction with
such TI products. TI’s provision of these resources does not expand or otherwise alter TI’s applicable warranties or warranty disclaimers for
TI products.
TI objects to and rejects any additional or different terms you may have proposed. IMPORTANT NOTICE
Mailing Address: Texas Instruments, Post Office Box 655303, Dallas, Texas 75265
Copyright © 2023, Texas Instruments Incorporated
