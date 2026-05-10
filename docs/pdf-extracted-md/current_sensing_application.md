# Current Sensing Applications in Communication Infrastructure Equipment (Rev. C)

- Source PDF: `../application_notes/current_sensing_application.pdf`
- Extraction tool: pdfplumber
- Page count: 4
- SHA256: `d07ce15b2597e95ec6011fbe8b01d286b1d6156edd0c9e213b1bfa833a0425e1`

## Page 1

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
These lower voltage rails are called point-of-load
Current Sensing in Power Supply Block (POL) supplies, stemming from the fact that these
rails satisfy a set of specific requirements and are
As shown in Figure 1, the power supply for the typically located in the vicinity of the supported
WI equipment comes from the utility grid, solar loads. Depending on how critical or informative
energy, or sometimes a combination of the two. The the measurements are, monitoring the current or
power supply is often backed up with battery storage voltage in one or more of these POL supplies is
for uninterrupted service during a power outage, recommended. The main requirements for the CSA
especially in remote areas where solely depending on in this situation can include (among others) accuracy,
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
into the WI end equipment, or can be stand-
As shown in Figure 2, current can be sensed at either
alone. Regardless of the implementation, a common
side of the load, with analog or digital CSA, and
requirement is an intelligent power management
through either external or integrated shunt resistors.
system to charge batteries and provide seamless
transitions between power sources. Current and CSA comes with a matched resistor gain network that
voltage sensing is an indispensable function in such provides value in terms of cost, board space, and
power management systems. performance. Most CSAs feature fixed-gain, ranging
from 10 to 1000. Some CSAs offer configurable
Current sensing can be implemented either on the
gain. For example, the INA225 has configurable gain
high side or on the low side. Dedicated high-voltage,
through two digital control pins, while other CSAs
shunt-based, Current Sense Amplifiers (CSA) such as
have a gain that is configurable through an external
the INA240 or INA241A are often required for fault
resistor, such as INA139.
to ground prevention. Magnetic current sensors such
as the TMCS1123 or TMCS1101 are both options System integration is further improved when a
for high-voltage applications due to their inherent selected CSA that comes with integrated analog
galvanic isolation. The TMCS1123 provides ambient to digital conversion (ADC) and a shunt resistor.
field rejection, fast overcurrent detection, and is EZShuntTM Technology integrates the shunt resistor
capable of 75A of continuous current flow perfect into the leadframe of the device, that generally
RMS
for incoming grid or solar power. reduces the overall form factor of the current sensing
SBOA366C – DECEMBER 2020 – REVISED AUGUST 2023 Current Sensing Applications in Communication Infrastructure Equipment 1
Submit Document Feedback
Copyright © 2023 Texas Instruments Incorporated

## Page 2

www.ti.com
circuit. The INA700 is an ultra-small wafer chip scale Integrated power amplifier monitor and control
digital power monitor with a 40-V common mode systems, such as the AMC7836, can simplify PA
range and a 2-mΩ leadframe resistance, which makes circuit design. As mentioned, due to natural device
the device 10A capable. The INA780A is a slightly variation, knowing gate voltage alone is sometimes
RMS
larger EZShuntTM product offered in a QFN package not sufficient to achieve accurate bias current control.
with a 85-V common-mode range and a 400-μΩ When current sensing is required in the control loop,
leadframe resistance, making the device 75A a separate high-voltage CSA (such as the INA290 or
RMS
capable. INA281) can be used.
The key considerations in selecting a CSA and Power amplifier monitor and control systems such
associated shunt resistor for POL measurement starts as the AMC7834 are another option with integrated
with common-mode voltage, current range, accuracy, current sensing capability. Such a design offers the
and speed. In addition, if overcurrent protection (OCP) possibility of further reducing board space.
is required, a CSA with an integrated fast-action
VDD
comparator is often the best choice, where system
Current/Voltage/Power/
parameters such as offset and propagation delay are Energy Sense
specified. Compared with discrete components, such ADC
a CSA helps remove uncertainties and as a result,
PA Controller Temperature Sense
(w/uC or FPGA)
simplifies the design.
DAC d
To monitor multiple POLs, a multichannel CSA such
g
s
as the INA4290 or INA4180 can make sense, since RF Input
these amplifiers offer four channels of analog output.
Figure 3. PA Biasing and Current Sensing
When a microcontroller or FPGA is present in the
equipment, an ADC channel is typically available, as
well as a digital bus such as I2C. In this situation, an Alternate Device Recommendations
analog or digital output CSA can be implemented as
TI offers a complete line of CSA and magnetic
a POL monitor. A multichannel digital monitor such as
sensors that serve in WI end equipment, from
the INA3221 is another option that frees up controller
high-voltage supply current and PA current sensing
ADC channels while taking advantage of an existing
to general purpose POL current monitoring. The
I2C bus. This device offers a number of warning and
high-voltage INA310A also comes with integrated
alert signals for fast action in case of a fault, as
comparator and reference output to facilitate the
well as current, voltage, or power information of three
OCP requirement. For applications where excellent
independent channels.
accuracy is required, the INA190 family of devices
are options with nA input bias current. These devices
Current Sensing in Power Amplifiers are essential in situations where the sensed current
is small. Some of these devices come with Enable
The bias current in power amplifiers (PA) is adjusted
pins for further power reduction; some are offered
to meet the need of an end application, modulation
in a WCSP package for board space optimization.
scheme, and operation class. A typical PA with
For applications with lower common mode voltage
current sensing is shown in Figure 3.
requirements, the INA180 offers speed and overall
The PA is often constructed with silicon LDMOS performance value. The INA301 family of devices
or GaN technology. Current sensing is important in features integrated fast comparators and high-speed
PA applications, both from the standpoint of the amplifiers. Both outputs that are available that meet
PA operation and from the standpoint of overall the requirements of OCP.
energy efficiency management. Under the same bias
Table 1. Alternate Device Recommendations
voltage, the PA bias current differs due to device
variation. Further, the bias current changes with Device Characteristics
temperature. Consequently, for the PA controller to Common mode range -4 V to 110 V;
INA310A
accurately control the bias current, both the current comparator
and temperature information must be available. The INA180 Low I Q ; High bandwidth
bias current information is required in improving INA190 Low I ; Low I ; enable pin
Q B
system efficiency, where approximately 50% of total
INA301 High bandwidth amplifier; fast comparator
system power is consumed by the PA itself.
2 Current Sensing Applications in Communication Infrastructure Equipment SBOA366C – DECEMBER 2020 – REVISED AUGUST 2023
Submit Document Feedback
Copyright © 2023 Texas Instruments Incorporated

## Page 3

www.ti.com
Related Documentation
1. Texas Instruments, Hybrid Battery Charger With Load Control for Telecom Equipment, application note.
2. Texas Instruments, Precision Current Measurements on High-Voltage Power Supply Rails, application brief.
3. Texas Instruments, Common Uses for Multichannel Current Monitoring
SBOA366C – DECEMBER 2020 – REVISED AUGUST 2023 Current Sensing Applications in Communication Infrastructure Equipment 3
Submit Document Feedback
Copyright © 2023 Texas Instruments Incorporated

## Page 4

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
