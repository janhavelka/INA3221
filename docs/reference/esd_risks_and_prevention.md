# Risks and Prevention of ESD, EOS, and Latch Up Events for Current Sense Amplifiers
**Source:** esd_risks_and_prevention.pdf | **Doc #:** SBOA615 | **Pages:** 10

## Key Takeaways
- Three threat categories for CSAs: Electrical Overstress (EOS — sustained over-voltage), ESD (fast transient discharge), and Latch-Up (parasitic low-impedance VS-to-GND path causing thermal damage)
- EOS protection: use input protection diodes (D1, D2) + series resistors limiting current to < 5 mA peak at input pins
- Latch-up is most commonly caused by poor decoupling, floating supply pins, or EMI coupling into VS — proper layout eliminates the risk
- Never float VS or GND pins while input pins are connected to a live bus; use a 5 kΩ pull-down from VS to GND if switching supply via FET
- Input filtering: CDIFF > 10× CCM; total input resistance < 10 Ω for standard CSAs (< 1 kΩ for capacitive-coupled types like INA186/INA190)

## Summary
This application note identifies four risky application scenarios for current sense amplifiers and provides concrete mitigation strategies. EOS events expose devices to voltages beyond absolute maximum ratings, typically during PCB assembly or system transients. ESD events are fast high-voltage discharges during component handling. Latch-ups create parasitic low-impedance paths between VS and GND that can destroy devices through sustained heating.

For EOS protection, unidirectional clamping diodes (D1, D2) with breakdown voltage below the device's absolute maximum VCM, combined with series resistors to limit peak input current to < 5 mA, are recommended. For system-level IEC61000-4-2 compliance, TVS diodes replace Zeners. Input capacitors (CCM, CDIFF) attenuate fast surges and improve noise immunity.

PWM applications (in-line motor sensing) expose CSAs to inductive kickbacks pulling VCM below −0.3 V. TI recommends PWM-rejection devices (INA240, INA241, INA253, INA254, INA790) for in-line sensing. Low-side CSAs near FETs must account for ground loops and inductive kickback; Schottky diode clamping or protection resistors may be needed.

EMI-induced latch-up at VS is the most insidious risk. Proper layout — low-impedance GND-to-decoupling path, supply trace routed through the decoupling capacitor, short/wide input traces — completely negates this risk. Floating VS or GND pins while inputs are live is explicitly prohibited; a 5 kΩ pull-down to GND keeps VS in a determined state when power-switched.

## Technical Details

### EOS Input Protection
```
                RProtect1    RProtect1
  Bus ──┤├──┬──/\/\/──┬──[IN+]
              │        │
              D1      CCM
              │        │
  Bus ──┤├──┬──/\/\/──┬──[IN-]
              │        │
              D2      CDIFF
              │        │
             GND      GND
```

### Protection Resistor Calculations
$$R_{\text{Protect2\_Positive}} > \frac{V_{\text{Clamp}} - V_{\text{CM,Max}}}{5\,\text{mA}}$$

$$R_{\text{Protect2\_Negative}} > \frac{V_F - 0.3\,\text{V}}{5\,\text{mA}}$$

### Input Filtering Rules
| Parameter | Requirement |
|-----------|-------------|
| CDIFF | > 10× CCM (to negate CCM mismatch effects) |
| CCM placement | Close to CSA input pins, low-impedance path to CSA GND |
| RFILTER (standard CSAs) | < 10 Ω total per input |
| RFILTER (capacitive-coupled: INA186, INA190) | < 1 kΩ |
| CDIFF for capacitive-coupled CSAs | > 1 nF minimum |

### Decoupling Layout Best Practices
1. GND pin → low-impedance, direct path → decoupling capacitor pad
2. Decoupling capacitor ground pad → low-impedance, direct path → system ground pour
3. VS trace **must route through** decoupling capacitor (cap in series with supply path)
4. Standard: 0.1 µF ceramic decoupling capacitor
5. Multiple parallel caps should be **same value** to avoid unpredictable resonances

### Supply Pin Management
- **Never float VS** when inputs are connected to a live bus — VS floats to ~1 V above GND
- Hot-plugging VS creates noisy, uncontrolled charge circuit → latch-up risk
- **Mitigation:** 5 kΩ resistor from VS to GND as soft-ground
- GND should be first pin connected, last pin disconnected
- No required power sequence if VS is driven to GND when disabled

### Latch-Up Mitigation
- Guard rings in IC absorb carriers from ESD cells
- Guard rings require low-impedance supply pins and sufficient decoupling
- Additional techniques: LC/PI filters at VS, ground shielding of sensitive traces, multiple vias

## Relevance to INA3221 Implementation
These protection guidelines directly apply to INA3221 PCB design. The INA3221 has three input channel pairs (IN1±, IN2±, IN3±) plus VS, GND, SDA, SCL, and ALERT pins. Key implementation considerations:
- **Decoupling:** 0.1 µF ceramic cap with VS trace routed through it; low-impedance GND path
- **Input protection:** If bus voltage can exceed the INA3221's ±26 V absolute maximum VCM, add clamping diodes + series resistors per the protection scheme, keeping total series R < 10 Ω to avoid gain error
- **No floating supplies:** If power-gating the INA3221, drive VS to GND when disabled; do not open-circuit VS
- **I²C digital pins:** The note mentions that floating GND is especially dangerous when digital pins (SDA, SCL) are active — ensure the INA3221's I²C bus is not driven while the device supply is off
- **EMI layout:** Keep input trace pairs close together (differential pair), route through CCM/CDIFF pads, keep short and wide
