# Ranging Subsystem (Legacy Two-Node Workflow)

5.04.2026

This module contains a direct master-slave ranging implementation for SX1280 radios at 2.4 GHz. It serves as a baseline acquisition and calibration workflow that predates the ring-coordinated trilateration branch. The subsystem is useful for controlled pairwise characterization, firmware bring-up, and calibration-table validation.

## Design Intent

- Provide a minimal two-node protocol for ranging experiments.
- Sweep bandwidth and spreading-factor configurations from a single master.
- Capture raw ranging values, ranging RSSI, and frequency error observables.
- Support both online ingestion (HTTP POST) and local post-processing output.

## Node Roles

- [Ranging/RadioLib_Ranging_Master/RadioLib_Ranging_Master.ino](RadioLib_Ranging_Master/RadioLib_Ranging_Master.ino): initiates communication, executes ranging as master, and uploads results.
- [Ranging/RadioLib_Ranging_Slave/RadioLib_Ranging_Slave.ino](RadioLib_Ranging_Slave/RadioLib_Ranging_Slave.ino): receives recipe, acknowledges, and executes ranging as slave.

## Legacy Packet Recipe

The master transmits an 8-byte configuration frame:

- Bytes 0-1: source ID (MSB, LSB)
- Bytes 2-3: destination ID (MSB, LSB)
- Byte 4: BW_ID in high nibble, SF in low nibble
- Byte 5: RF-frequency selector (reserved in current implementation)
- Bytes 6-7: sample size (MSB, LSB)

The slave echoes relevant fields in its acknowledgement frame, enabling the master to capture link diagnostics (including frequency error) prior to ranging.

## Measurement Loop

For each configuration, the master executes:

1. Communication phase (recipe transmit and ACK receive).
2. Ranging phase over sample_size attempts.
3. Optional post-processing and calibration statistics.
4. Optional online upload to the Python receiver.

Configuration sweep in the master branch:

- BW_ID: 0..2 corresponding to {406250, 812500, 1625000} Hz
- SF: 5..10

## Calibration and Post-Processing

The master firmware includes optional routines for:

- Raw-to-distance conversion using SX1280 range-register scaling.
- Clock-drift compensation using frequency-error gradient tables.
- LNA-gain correction using RSSI-indexed lookup tables.
- Summary statistics (mean, standard deviation, median).

Calibration assets are defined in [Ranging/rangingCorrection.h](rangingCorrection.h) and should be treated as platform-specific defaults rather than universal constants.

## Online Data Path

When online mode is enabled:

- The master connects to Wi-Fi.
- JSON payloads are sent to the Flask endpoint configured by SERVER_URL.
- The backend archives and materializes measurements for analysis.

Field naming in this legacy branch may differ from newer trilateration payload conventions (for example freq_error and sample_size). The current receiver includes compatibility logic for these variants.

## Relationship to Trilateration Branch

This module and the Trilateration module share radio fundamentals but differ in coordination strategy:

- Ranging branch: single master, single slave, direct parameter sweep.
- Trilateration branch: three masters in ring handoff plus one slave, synchronized multi-anchor collection.

The Ranging branch is suitable for pairwise calibration and regression testing, while the Trilateration branch is suitable for structured multi-anchor campaign execution.

## Practical Use Cases

- Hardware validation after board assembly or radio replacement.
- Baseline link-quality characterization before multi-anchor campaigns.
- Verification of correction-table behavior under controlled conditions.
- Rapid experimentation when full ring coordination is unnecessary.
