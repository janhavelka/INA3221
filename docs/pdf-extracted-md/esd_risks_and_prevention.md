# Risks and Prevention of ESD, EOS, and Latch Up Events for Current Sense Amplifiers

- Source PDF: `../reference/esd_risks_and_prevention.pdf`
- Extraction tool: pdfplumber
- Page count: 10
- SHA256: `5122f0726dde71b79b37e539e6734a33c0507c086f3514d6ac58b934081694e6`

## Page 1

www.ti.com Table of Contents
Application Note
Risks and Prevention of ESD, EOS, and Latch Up Events
for Current Sense Amplifiers
Peter Iliya
ABSTRACT
This application note discusses the best practices for reducing risk of abnormal operation and or damage of
current sense amplifiers from electrostatic discharges (ESD), electrical overstress events (EOS), and latch up
(LU).
Table of Contents
1 Introduction.............................................................................................................................................................................2
2 What is ESD, EOS, and Latch Up[unreadable glyph]........................................................................................................................................ 2
2.1 Electrical Overstress.......................................................................................................................................................... 2
2.2 Electrical Static Discharge................................................................................................................................................. 2
2.3 Latch Ups........................................................................................................................................................................... 3
3 Risky Applications for Current Sense Amplifiers................................................................................................................4
3.1 Applications with Over Voltage Transient Surges (EOS)................................................................................................... 4
3.2 Pulse Width Modulated Current Sensing Risks................................................................................................................. 5
3.3 Applications with Significant Electromagnetic Interference................................................................................................6
3.4 Applications that Float the Supply (VS or GND) Pins of CSA............................................................................................ 8
4 Summary................................................................................................................................................................................. 9
5 References.............................................................................................................................................................................. 9
Trademarks
All trademarks are the property of their respective owners.
SBOA615 – NOVEMBER 2024 Risks and Prevention of ESD, EOS, and Latch Up Events for Current Sense 1
Submit Document Feedback Amplifiers
Copyright © 2024 Texas Instruments Incorporated

## Page 2

Introduction www.ti.com
1 Introduction
All amplifiers have inherent risk damage or abnormal operation from electrostatic discharges (ESD), electrical
overstress events (EOS), and latch up (LU). Many of the methods to mitigate or prevent these risks are similar
to techniques proposed for many years with most amplifiers. However, there are some risks specific to use of
current sense amplifiers (CSA) that a designer needs to account for before building a PCB.
2 What is ESD, EOS, and Latch Up[unreadable glyph]
2.1 Electrical Overstress
Electrical Overstress (EOS) events are over-voltage events that expose a device to voltages beyond survivable
rating (or Absolute Maximum Voltage Rating). Once the event persists longer than a few hundred Nano-seconds,
this can be considered EOS. The EOS events mostly occur when IC components are assembled on the PCB.
EOS events can usually turn on ESD cells and cause them to conduct currents for longer than designed for, thus
generating damaging heat.
In general, CSA damage risk from EOS events can be prevented with input resistors that limit input currents to
< 5mA peak and or voltage clamps that clamp input pins to under the Absolute Maximum Ratings shown in data
sheet.
2.2 Electrical Static Discharge
Electrical Static Discharge (ESD) events are fast high voltage events (spikes are over within hundreds of
Nano-seconds) caused by the sudden discharge of static charge buildup on human and machine handlers of
IC components before or after ESD events are assembled onto PCBs. CSAs (and most amplifiers in general)
are protected from the high-voltage, fast edge ESD events with ESD cells. ESD cells are complex, but are
essentially comprised of an absorption device and body diode.
Figure 2-1. ESD Cell
For a fast overvoltage event (positive or negative), an ESD cell is triggered and then the absorption device can
quickly begin to sink increasing current. Once current through absorption device is high enough, the current can
snap back. However, if the over voltage event lasts longer than the ESD is designed for (EOS) and there is
no resistance to limit current into ESD cell, the current can increase quickly and generate excessive heat and
damage the device.
Note that the body diode of the ESD cell can become forward biased when the input voltage drops somewhere
below -0.3V. This is why many standard low-voltage CSAs have minimum Absolute Common-Mode Voltage
(V ) ratings of GND-0.3V.
CM
2 Risks and Prevention of ESD, EOS, and Latch Up Events for Current Sense SBOA615 – NOVEMBER 2024
Amplifiers Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated

## Page 3

www.ti.com What is ESD, EOS, and Latch Up[unreadable glyph]
Figure 2-2 shows the general ESD topology for a CSA.
Figure 2-2. ESD Architecture Single Stage CSA
2.3 Latch Ups
Latch ups (LU) are low-impedance pathways between Vs and GND, which dramatically increases supply current
and can easily damage the device through sustained heating. Although latch ups do not always cause damage
and can be removed by cycling power.
Latch ups are possible in all ICs that are CMOS or BiCMOS based or use junction isolated processes because
there exist inherent, lateral parasitic transistors and diodes formed by PN junctions from the fundamental use of
NMOS and PMOS transistors.
The three ways to latch up are overvoltage, current injection, and fast transient. These events can lead to
the inadvertent activation of ESD cells during normal device operating conditions. If an ESD cell is sufficiently
triggered by EOS or fast edge transient, then this can result in the flooding of carriers from ESD cell into the
device substrate, which then can cause a latch up.
Most latch ups are due to ESD cells or parasitic pathways turning on. An ESD cell is a tank of carriers and when
there is a trigger at input, the tank of carriers can spill over the layout and substrate.
Latch ups are mitigated in ICs by using guard rings. Guard rings act as carrier sinks to prevent carriers from
entering the device substrate. If there are too many carriers, latch ups can go under or over guard rings. All ESD
cells can have their own guard rings.
Guard rings however can only work properly when the supply pins are low-impedance and the decoupling
capacitance is sufficient. Thus, amplifiers can become more susceptible to latch up when basic layout
techniques are not followed.
The Latch-up, white paper describes the theory and practice of IC latch up if more information is desired.
SBOA615 – NOVEMBER 2024 Risks and Prevention of ESD, EOS, and Latch Up Events for Current Sense 3
Submit Document Feedback Amplifiers
Copyright © 2024 Texas Instruments Incorporated

## Page 4

Risky Applications for Current Sense Amplifiers www.ti.com
3 Risky Applications for Current Sense Amplifiers
When used as recommended, there is extremely low risk for damage or abnormal operation for current sense
amplifiers in most applications. However, there are applications that do require certain basic precautions and risk
mitigating techniques.
3.1 Applications with Over Voltage Transient Surges (EOS)
For applications where input voltages can exceed input ratings, the easiest option is to choose a CSA (or even
a hall-based or isolated current sensor) that can survive the voltage; however, this is not always possible. When
this is the case, the best practice is to use the following input protection scheme shown in Figure 3-1.
Figure 3-1. Input Protection for EOS
In Figure 3-1, D1 and D2 need reverse-breakdown voltages before the Absolute Maximum V Voltage of the
CM
CSA. Ideally, voltages at input pins are clamped before the maximum V rating. If D1 and D2 clamp after the
CM
V rating, the R resistor can limit the rest of current into ESD pin to < 5mA peak. General equation to
CM Protect2
determine R is shown in Equation 1.
Protect2
1 (1)
Figure 3-1 R sPhrootwecst2 u_PnoisdtiirveecVtCioMn al > di od V eCsla amnpd n − ot b V iCdMir,e Mcatixon / a 5 l m T A VS diodes because during negative voltage
event the clamping diodes breakdown with their forward bias characteristic, which is much better than a larger
breakdown voltage. The resulting forward bias voltage (V ) can probably be less than the -0.3V minimum V
F CM
rating for low-side CSAs. Thus, for negative voltage surges, R can be necessary to still limit current
Protect2
according to Equation 2.
2 (2)
If the CSA R iPsr ogtoecint2g_ Ntoeg baeti vreaVtcemd to > sy s V teFm − lev e 0 l . s 3 t V an / d 5 a m rd A s such as IEC61000-4-2, then D1 and D2 need to be
TVS diodes. For any less stringent and/or slower EOS events, D1 and D2 can be Zener diodes that are rated to
withstand the current draw for however long the EOS can occur. It is best to conservatively assume that the EOS
condition can last indefinitely.
4 Risks and Prevention of ESD, EOS, and Latch Up Events for Current Sense SBOA615 – NOVEMBER 2024
Amplifiers Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated

## Page 5

www.ti.com Risky Applications for Current Sense Amplifiers
R helps limit current mostly into D1 and D2 (and thus reduces required power ratings). However, there can
Protect1
be a tradeoff with circuit offset error over temperature due to D1 and D2 leakage currents. Mismatch in these
leakage currents, along with mismatch in the R resistors can create variable voltage drops in the R
Protect1 Protect1
resistors, which can cause input offset error.
The input capacitors (C and C ) can attenuate fast voltage surges increase robustness to system level
CM DIFF
testing and noise immunity.
Most CSAs (except for high-input impedance ones such as INA190 or INA186) can require total resistance into
an input (R +R ) to be less than 10-Ohm to avoid any noticeable change in error. However, there are
Protect1 Protect2
methods to bound this error. Refer to this Input Resistance Error for Current Sense Amplifiers, user's guide for
more information.
Additional testing of a CSA against IEC surges can be found Transient Robustness for Current Shunt Monitor,
reference design.
3.2 Pulse Width Modulated Current Sensing Risks
Sensing something in-line means the shunt resistor is next to the source and subject to a switching input
common-mode voltage, such as a shunt next to motor in between high and low side FETs. For in-line sensing, TI
recommends to use current-sense amplifiers that have PWM Enhanced Rejection capability such as the INA240,
INA241, INA253, INA254, and INA790. Functionally, other CSAs do not perform as well because of limited AC
common-mode rejection, which can yield large output disturbances.
Figure 3-2. Current Sensing Risks in an H-Bridge
SBOA615 – NOVEMBER 2024 Risks and Prevention of ESD, EOS, and Latch Up Events for Current Sense 5
Submit Document Feedback Amplifiers
Copyright © 2024 Texas Instruments Incorporated

## Page 6

Risky Applications for Current Sense Amplifiers www.ti.com
Additionally, measuring in-line and even low-side of the FETs can subject the CSA to inductive kickbacks pulling
input V below -0.3V. If using a low-side CSA (that has minimum V rating of GND-0.3V) on the low-side of
CM CM
the FET, take care to account for inductive kickbacks.
While this can seem simple to assume that the ground of the FET (PWRGND) and ground of the CSA (AGND)
needs to be the same and thus V needs to always be 0V, this cannot be the case if there is high impedance
CM
connection between PWRGND and AGND which can create a ground loop that is exacerbated by the inductive
kickback.
If significant inductive kickback voltages are measured for a low-side CSA, then the CSA can require input
protection as shown in Figure 3-1 where D1 and D2 is not necessary given low voltages of inductive kickback or
can be replaced with fast acting Schottky diodes for clamping.
3.3 Applications with Significant Electromagnetic Interference
These applications have two different risks: Electromagnetic Interference (EMI) induced noise and EMI induced
latch up. Obviously, a latch up is a more serious outcome compared to elevated signal noise; however, the
recommended best layout practices to mitigate both risks are the same.
EMI noise does not sound risky, but when in combination with bad layout practices, EMI noise can increase
susceptibility of a CSA latch up, specifically at V pin. Simple layout techniques can completely negate this risk.
S
The reason for this goes back to the guard rings discussed in Section 2.3 and how internal ESD guard rings help
prevent latch ups by absorbing excess carriers before the ESD guard rings can spill out to the substrate. Simply
put, proper layout is needed to make sure guard rings operate as expected and keep latch up risk low.
Fast and large voltage or current switching traces from motors or other switching FETs are the main culprits of
EMI noise. Thus, VS pin can become more susceptible to EMI coupling when VS trace is long and goes around
EMI sources.
3.3.1 Layout Best Practices for Reducing EMI Induced Latch Up or Noise
3.3.1.1 Techniques for Proper Grounding and Decoupling Capacitance
1. GND pin has a low-impedance, direct path to the decoupling capacitor pad.
2. There is low-impedance, direct path from decoupling ground pad to system ground pour.
3. The supply trace for supply pin (VS) must go through decoupling capacitor.
Figure 3-3 is an example with proper layout for filters at both supply and input pins on a standard CSA. C12 is
the standard 0.1μF decoupling capacitor.
Figure 3-3. Good Decoupling Capacitor Layout
6 Risks and Prevention of ESD, EOS, and Latch Up Events for Current Sense SBOA615 – NOVEMBER 2024
Amplifiers Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated

## Page 7

www.ti.com Risky Applications for Current Sense Amplifiers
Figure 3-4 shows bad layout where there is significant impedance in between GND pin and decoupling capacitor
making this device susceptible to latch up in EMI environments.
Figure 3-4. Bad Decoupling Capacitor Layout With Impedance from Pin to Capacitor
3.3.1.2 Additional and Advanced Layout Techniques
For the most part, the basic layout techniques mentioned above are going to negate risks from EMI induced
latch up at VS; however, for excessive noise coupling is always good to follow these additional layout techniques
as much as possible to reduce noise into signal.
1. Use shortest and widest possible traces for input pins.
2. Use multiple vias (or larger vias for plane connections).
a. Use thinner dielectric spacing between top layer and plane to reduce via inductance.
b. Adding more capacitors in parallel can reduce ESL and improve filtering; however, for best practice,
the capacitors are all the same value as most ceramic capacitors (of a single size) manufactured today
can have the same impedance regardless of the specified capacitance. Thus, trying to use multiple
capacitors of differing capacitance in parallel can create unpredictable resonances.
3. Use LC or PI filters at VS pin to further attenuate noise.
4. Shield or guard sensitive traces with ground traces or pours and vias to create vertical shield walls.
3.3.1.3 Proper Input Filtering Layout Techniques for Noise Reduction
1. Input traces are considered a differential net and kept as close together as possible.
2. Input traces go through input common-mode capacitors (C ) and differential capacitor (C ) pads.
CM DIFF
3. Input resistors (R ) are kept under 10Ω (or 1kΩ for capacitive-coupled input CSAs such as the INA186,
FILTER
INA190, or INAx191). If higher input resistors are needed, then refer to this Input Resistance Error for
Current Sense Amplifiers, user's guide on calculating error increase.
4. C and C are placed close to the CSA input pins.
CM DIFF
5. C has a low-impedance, direct path to CSA ground pin.
CM
6. C is greater than 10 times the C value. The reason for this is to negate effects of mismatches in C
DIFF CM CM
and resulting dynamic offsets, thus keeping the sense voltage stable during sudden V changes.
CM
7. If using a capacitive-coupled CSA, the requirement is usually to add a C of > 1nF to keep input front-end
DIFF
capacitors stable for loads with periodic current transients or when input resistors are high or input traces
are long creating high input inductance. See data sheet and this Input Resistance Error for Current Sense
Amplifiers, user's guide for more information.
SBOA615 – NOVEMBER 2024 Risks and Prevention of ESD, EOS, and Latch Up Events for Current Sense 7
Submit Document Feedback Amplifiers
Copyright © 2024 Texas Instruments Incorporated

## Page 8

Risky Applications for Current Sense Amplifiers www.ti.com
Figure 3-5 shows the general schematic for filtering input noise on CSAs.
Figure 3-5. Input Filtering for EMI Induced Noise
3.4 Applications that Float the Supply (VS or GND) Pins of CSA
In general, TI does not recommend to ever float the supply pins of a CSA usually with a switch or FET due to the
inherent risk of latch up as shown in Figure 3-6. A designed for VS needs to be driven to GND when turned off
and when this is the case, there is no required power sequence for CSA. VS and IN+/IN- can be turned on or off
independently of each other.
This latch up can occur when input pins (IN+ and IN-) are connected to a voltage (bus is on) and a floating VS
or GND suddenly switched through a FET or physically connected to analog source. This can be referred to as
hot-plugging or hot-swapping. This scenario presents two fundamental issues:
• When VS is floating and input pins are connected to a live bus voltage, the CSAs VS pin can float to some
unknown state, usually up to 1V above GND pin.
• When the hot-plug occurs, this generates a very noisy and uncontrolled charge circuit.
Figure 3-6. Avoid Floating Supply Pins
For many engineers, there are advantages to turn off a CSA for overall system power reduction and thus a FET
to open up VS is enticing. If an engineer does pursue this method, then the best practice is to place a 5kΩ
8 Risks and Prevention of ESD, EOS, and Latch Up Events for Current Sense SBOA615 – NOVEMBER 2024
Amplifiers Submit Document Feedback
Copyright © 2024 Texas Instruments Incorporated

## Page 9

www.ti.com Summary
resistor from VS pin to GND pin as illustrated in Figure 3-7. This pull-down resistor can act as a soft-ground for
Vs pin and keep Vs in a determined state as well as provide a path for sudden current discharge when FET turns
on.
Note that the usual case of powering a CSA with an LDO is shown in Figure 3-7 to illustrate that LDO usually
have some small leakage current to ground when disabled and also have stable voltage turn on.
Figure 3-7. Acceptable Supply Disable/Enable
Floating the GND pin is not a good idea unless your system has control over power sequencing and opening/
closing all pins of the device (including digital pins for digital power monitors), which usually is not be practical.
In general, the GND pin needs to be the first pin connected to a system and the last pin disconnected from
a system. Basically, you can think of opening/closing a ground pin as how you connect the pins of an EVM
(evaluation module) to a bench set up.
4 Summary
CSA are designed for many environments, but CSA still require proper layout techniques and BOM
considerations as all amplifiers do.
5 References
• Texas Instruments, Design consideration for system-level ESD circuit protection, analog applications journal.
• Texas Instruments, General hardware design/BGA PCB design/BGA decoupling, application note.
• Texas Instruments, Latch-Up, white paper.
• Texas Instruments, Precision labs series: Op amps, video series.
• Texas Instruments, Precision labs series: Analog-to-digital converters (ADCs), Video Series
• Texas Instruments, Transient Robustness for Current Shunt Monitor, reference design.
SBOA615 – NOVEMBER 2024 Risks and Prevention of ESD, EOS, and Latch Up Events for Current Sense 9
Submit Document Feedback Amplifiers
Copyright © 2024 Texas Instruments Incorporated

## Page 10

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
Copyright © 2024, Texas Instruments Incorporated
