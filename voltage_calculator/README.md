# NLX Electrical Calculator

The Nuerolynx field calculator combines Ohm's-law calculations with a two-conductor voltage-drop and wire-sizing workflow.

## Voltage-drop inputs

- Source voltage
- Load entered as amps or watts
- One-way length in feet or meters
- 00, 0, and every whole AWG conductor size from 1 through 24
- Copper or aluminum conductor
- Estimated conductor temperature
- Configurable maximum voltage-drop percentage

The result includes current, loop resistance, voltage drop, percentage drop, delivered voltage, wire loss, maximum run length, a sizing verdict, and the smallest supported gauge that meets the selected voltage-drop target.

The verdicts are:

- **ADEQUATE:** calculated drop is at or below the target.
- **MARGINAL:** calculated drop is no more than 15 percent above the target.
- **UPSIZE REQUIRED:** calculated drop exceeds the marginal range.

This is a voltage-drop sizing verdict, not an ampacity or code-compliance determination. Actual conductor suitability also depends on insulation rating, cable listing, bundling, ambient conditions, terminals, installation method, fault protection, and applicable requirements.

## Ohm's law

The app solves voltage, current, or resistance from the other two values and also calculates power.

Highlight any numeric field in the configuration menu and press Left or Right to change
the value by 0.1. Holding Left or Right changes it by 1.0 and repeats. Press OK on the
highlighted field to open the existing exact-entry keypad; use `_` in place of the
decimal point. On the Wire row, Left and Right cycle through 00, 0, and every whole AWG
size through 24, while OK opens exact gauge entry.

## Calculation basis

Copper resistance values are rounded from the [NIST Copper Wire Tables](https://nvlpubs.nist.gov/nistpubs/Legacy/hb/nbshandbook100.pdf) for standard annealed copper at 20 degrees Celsius. Resistance is corrected for the selected conductor temperature. Aluminum values are engineering estimates derived from the material resistivity ratio.

The default 3 percent voltage-drop target is a design aid and remains user-configurable. The NFPA describes 3 percent branch-circuit and 5 percent combined feeder/branch-circuit voltage-drop recommendations as informational efficiency guidance; the app does not claim that a result is code compliant.

## Credits and license

This application extends the original MIT-licensed VoltCalc by Andrew Diamond. The original copyright and license are retained in `LICENSE`.
