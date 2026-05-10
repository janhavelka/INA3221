# Energy and Power Monitoring with Digital Current Sensors

- Source PDF: `../application_notes/power_energy_monitoring.pdf`
- Extraction tool: pdfplumber
- Page count: 3
- SHA256: `08d4cd07045ce2b36f0d5155f3d317307a923791e8ba61e5af9d9e2daa9fc639`

## Page 1

____________________________________________________
Power and Energy Monitoring With Digital Current
Sensors
Dennis Hudgins
As the demand for power efficient systems continues
to grow, accurately monitoring system power and A simplified block diagram of the power conversion
energy consumption is increasingly important and is a engine is shown in Figure 2. Power is internally
problem more engineers must solve. One solution to calculated from the shunt and bus voltage
this problem is to use an analog to digital converter measurements in an interleaved fashion to minimize
(ADC) for both the current and voltage and then time alignment errors in the power calculation.
multiply the result in a processor to obtain power.
However, the communications delay and overhead
between getting the current and voltage information 24-bit
introduces time alignment errors in the power Power Accumulator
measurement since both the current and the voltage
Power
can be varying independently of each other. To
minimize the delay between the voltage and current
measurements, the processor would need to dedicate Bus Voltage
adequate processing power to ADC communications
and power calculations. Even with the processor Current
Shunt Voltage
primarily dedicated to this function, any interactions Channel 16-bit
with other devices in the system could delay the Bus Voltage ADC
Channel
voltage and current measurements reducing power Calibration
monitoring accuracy. Adding additional responsibilities
like averaging the system voltage, current, and power, Shunt Voltage
as well as energy monitoring would start to further
burden the processor with additional functions.
Figure 2. INA233 Power Conversion Engine
A better way to monitor power is to use a digital
current monitor to handle the mathematical
The internal calculations for power are done in the
processing, freeing up the processor to deal with other
background independent of ADC conversion rates or
system tasks, and alerting the processor if higher level
digital bus communications. The device also features
system actions need to occur. Texas Instruments
an ALERT pin that will notify the host processor if the
provides a wide range of digital power and current
current, power, or bus voltage is out of the expected
monitors to address this problem. One such power,
range of operation. In the INA233, fault events are
current and voltage monitor is the INA233. The
handled independently, such that multiple
INA233 enables the monitoring of voltage, current,
simultaneous fault conditions can be reported by
power, and energy via an I2C, SMBus, PMBus
reading internal status registers when the ALERT pin
compatible interface. A typical application and block
asserts. The internal processing and alert capabilities
diagram of the INA233 is shown in Figure 1.
of the INA233 free the host processor to manage other
Supply Voltage tasks while the device takes care of continually
Bus Voltage (2.7 V to 5.5 V)
(0 V to 36 V) CBYPASS monitoring the system. The host processor is notified
via the ALERT pin only when additional attention is
INA233 0.1 μF
High-Side VBUS VS needed.
Shunt
u(cid:3) Power Register SDA
The INA233 also features a 24-bit power accumulator
SCL
Load Acc P u o m w u e la r tor that continuously adds the current power reading to
IN+ V ADC Current Register I2 C C o - P , m M S p M B a u t B i s b u l s e - , the sum of previous power readings. The power
Low S -S hu id n e t IN- I Voltage Register Interface ALERT accumulator can be used to monitor system energy
A0 consumption to get an average measurement of the
Re A g l i e s r te t rs A1 power consumption overtime. Since power levels can
GND
Copyright © 2017, Texas Instruments Incorporated fluctuate during any given instant in time, monitoring
Figure 1. INA233 Typical Application Circuit the energy provides a better way to gauge the average
SBOA194–April 2017 Power and Energy Monitoring with Digital Current Sensors 1
Dennis Hudgins
Submit Documentation Feedback
Copyright © 2017, Texas Instruments Incorporated

## Page 2

n
¦
ADCPowerMeasurment
i
Average Power = i 1
n
Total Accumulated Power over n samples
Number of samples
vid/Vm01
www.ti.com
system power usage over long time intervals. Knowing Energy = Average Power u time
the system energy consumption provides a metric to § n ·
 ̈ ¦ADCPowerMeasurment ̧
gauge system runtime and power efficiency, as well as ̈ i ̧
= ̈ i 1 ̧ u (cid:11)n u ADC conversion time(cid:12)
the effects of power optimizations involving adjustment ̈ n ̧
of power supply voltages and processor clock rates. ̈ ̧
© 1
The ADC conversion times for both shunt and bus
Total Accumulated Power u ADC conversion time
voltage measurements are programmable from 140 μs
(2)
to 8.244 ms. Longer conversion times are useful to
decrease noise susceptibility and to achieve increased Since the ADC conversion time can vary by as much
stability in device measurements. The effects of as 10%, it is recommended to multiply the average
increased ADC conversion times measurements power by the time measured by an external time base.
results is shown in Figure 3. The time interval for the energy calculation should be
long enough so that the communications time due to
Conversion Time: 140ms the digital bus is insignificant to the total time used in
the energy calculation.
The size of the power accumulator in the INA233 is
limited to 24 bits. The value of the accumulator should Conversion Time: 1.1ms
be read periodically and cleared by the host to avoid
overflow. The accumulator can be configured to be
automatically cleared after each read. The time to
Conversion Time: 8.244ms overflow will be a function of the power, ADC
conversion times and averaging times. Higher power
levels will cause any overflow in the power
0 200 400 600 800 1000
accumulator to occur faster than lower power levels.
Number of Conversions
Also, longer conversion times and higher number of
Figure 3. Noise vs. ADC Conversion Time
averages will increase the time to overflow; in lower
power cases, the time to overflow can be extended to
In addition to programmable ADC conversion times, be several hours up to days in length.
the device can average up to 1024 conversion cycles
The INA233 is one of many digital current monitors
and update the internal power, current, and voltage
offered by Texas Instruments. Table 1 shows some
registers once the averaging is finished.
alternative devices that can be used to monitor a
Programmable conversion times along with averaging
system and help free the host processor to handle
windows allow the device telemetry update rate to be
higher level tasks.
adjusted to meet system timing needs.
Even though the INA233 has built in averaging and Table 1. Alternative Device Recommendations
adjustable ADC conversion times, the user has to wait
until the averaging is complete before getting the Device Optimized Parameters Performance Trade-Off
result. One benefit of the internal power accumulator is No Power Accumulator,
I2C/SMBus compatible
that it allows the host to calculate the average power INA226 with reduced register set no independent fault
monitoring
on demand, eliminating the otherwise delay for the
averaging interval to finish. Getting an average power Less accuracy, no power
WCSP package, reduced accumulator, no
reading on demand is achieved by taking the value of INA231
register set, lower cost independent fault
the total accumulated power and dividing by the total
monitoring.
sample count for that accumulation period as shown in
Less accuracy and
Equation 1. Lowest Cost, reduced
INA219 resolution, no ALERT, No
register set
power accumulator
Less accuracy and
INA3221 Monitors 3 channels resolution, monitors bus
and shunt voltages
Table 2. Adjacent Tech Notes
(1) Integrated, Current Sensing Analog-to-
SBOA179
Digital Converter
Once the average power is calculated, the energy
consumption is determined by multiplying the average
power by the time interval of that average or by
multiplying the total accumulated power by the ADC
conversion time as shown in Equation 2.
2 Power and Energy Monitoring with Digital Current Sensors SBOA194–April 2017
Dennis Hudgins
Submit Documentation Feedback
Copyright © 2017, Texas Instruments Incorporated

## Page 3

IMPORTANT NOTICE AND DISCLAIMER
TI PROVIDES TECHNICAL AND RELIABILITY DATA (INCLUDING DATASHEETS), DESIGN RESOURCES (INCLUDING REFERENCE
DESIGNS), APPLICATION OR OTHER DESIGN ADVICE, WEB TOOLS, SAFETY INFORMATION, AND OTHER RESOURCES “AS IS”
AND WITH ALL FAULTS, AND DISCLAIMS ALL WARRANTIES, EXPRESS AND IMPLIED, INCLUDING WITHOUT LIMITATION ANY
IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT OF THIRD
PARTY INTELLECTUAL PROPERTY RIGHTS.
These resources are intended for skilled developers designing with TI products. You are solely responsible for (1) selecting the appropriate
TI products for your application, (2) designing, validating and testing your application, and (3) ensuring your application meets applicable
standards, and any other safety, security, or other requirements. These resources are subject to change without notice. TI grants you
permission to use these resources only for development of an application that uses the TI products described in the resource. Other
reproduction and display of these resources is prohibited. No license is granted to any other TI intellectual property right or to any third
party intellectual property right. TI disclaims responsibility for, and you will fully indemnify TI and its representatives against, any claims,
damages, costs, losses, and liabilities arising out of your use of these resources.
TI’s products are provided subject to TI’s Terms of Sale (www.ti.com/legal/termsofsale.html) or other applicable terms available either on
ti.com or provided in conjunction with such TI products. TI’s provision of these resources does not expand or otherwise alter TI’s applicable
warranties or warranty disclaimers for TI products.
Mailing Address: Texas Instruments, Post Office Box 655303, Dallas, Texas 75265
Copyright © 2020, Texas Instruments Incorporated
