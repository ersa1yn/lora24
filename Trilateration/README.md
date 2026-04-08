# Trilateration Subsystem

5.04.2026

This module implements a coordinated multi-anchor measurement protocol for 2.4 GHz LoRa ranging using SX1280 radios. Rather than estimating coordinates directly on-device, the subsystem orchestrates synchronized data acquisition among three anchor masters and one slave. The resulting measurements are uploaded for offline trilateration and calibration analysis.

## Objectives

- Coordinate repeated ranging campaigns over a controlled BW/SF sweep.
- Enforce deterministic turn-taking among anchor nodes.
- Collect raw ranging observables with packet-level accountability.
- Export measurements to a central server in a machine-readable format.

## Network Topology and Roles

### Logical Topology

Anchors are organized as a directed ring:

- Anchor 0x01 -> Anchor 0x02 -> Anchor 0x03 -> Anchor 0x01

Slave node:

- Device ID 0xFF

### Role Mapping

- [Trilateration/Trilateration_Master_1/Trilateration_Master_1.ino](Trilateration_Master_1/Trilateration_Master_1.ino): main master (0x01), initiates campaign and advances configuration sweep.
- [Trilateration/Trilateration_Master_2/Trilateration_Master_2.ino](Trilateration_Master_2/Trilateration_Master_2.ino): anchor participant (0x02).
- [Trilateration/Trilateration_Master_3/Trilateration_Master_3.ino](Trilateration_Master_3/Trilateration_Master_3.ino): anchor participant (0x03).
- [Trilateration/Trilateration_Slave/Trilateration_Slave.ino](Trilateration_Slave/Trilateration_Slave.ino): slave ranging responder (0xFF).

## Protocol Definition

Control messages use a compact 4-byte frame implemented in shared utilities.

### Packet Semantics

- Byte 0:
  - Bits [7:6]: packet type
  - Bits [5:4]: bandwidth index (BW_ID)
  - Bits [3:0]: spreading factor (SF)
- Byte 1:
  - Bits [7:4]: sweep count
  - Bits [3:0]: reserved
- Byte 2: source device ID (LSB)
- Byte 3: destination device ID (LSB)

Current packet types:

- 0: RangingRequest
- 1: MasterDone (turn handoff)

Supported experimental configuration set:

- BW_ID in {0, 1, 2} mapping to {406250, 812500, 1625000} Hz
- SF in [5, 10]
- sweepCount <= 6

## Phase Execution Model

Each master follows the same high-level finite-state flow:

1. WaitTurn
2. ConfigureSlave
3. Ranging
4. PassTurn
5. SendData

Main-master behavior:

- Starts the first cycle without waiting for an inbound handoff.
- Advances BW/SF sweep and increments persistent run_id after completing a full configuration loop.

Non-main masters:

- Wait for MasterDone from parent anchor.
- Reuse received configuration for slave setup and local ranging.
- Forward MasterDone to child anchor.

Slave behavior:

- Waits for RangingRequest.
- Acknowledges configuration.
- Performs slave-side ranging loops using requested BW/SF and sweep count.

## Shared Utility Layer

The files [Trilateration/Utilities.h](Utilities.h) and [Trilateration/Utilities.cpp](Utilities.cpp) define reusable protocol and link primitives:

- ControlPacket and PacketType representations.
- LinkContext for radio instance, interrupt flag, node identity, and LED policy.
- sendAndAwaitAck for master-initiated transaction.
- awaitAndSendAck for receiver-side validation and acknowledgement.
- Topology helpers parentOf and childOf.
- Serialization helpers packControlPacket and unpackControlPacket.

This abstraction reduces duplicate radio-handshake logic and centralizes protocol validation (type, destination, peer, sweep constraints, and frame length).

## Measurement and Telemetry

Per master configuration cycle, firmware records:

- Frequency error at configuration acknowledgement.
- Raw ranging register values for valid samples.
- Ranging RSSI values for valid samples.
- Valid/timeout/fail counters.

Payload fields transmitted to the backend include:

- run_id
- device_id
- target_id
- bw_sf (packed BW/SF)
- freqError
- sweepCount
- valid
- timeout
- fail
- raw_rng
- rng_rssi

## Build and Deployment Notes

- Hardware: ESP32-class boards with SX1280 modules.
- Library dependencies: RadioLib, WiFi, HTTPClient, Preferences.
- Calibration/channel constants: [Trilateration/rangingCorrection.h](rangingCorrection.h).
- Server endpoint is configured in shared utilities and must match the host running [receiver.py](../receiver.py).

## Data Products

The Python ingestion service stores records by device and run under:

- data/<device_name>/runs/<run_id>/raw_packets.jsonl
- data/<device_name>/runs/<run_id>/run_samples.csv
- data/<device_name>/runs/<run_id>/run_meta.json

For consistency with the current server schema, payloads should include an explicit device_name field when posting data.

## Current Scope and Limitations

- Coordination protocol is designed for three anchors and one slave.
- Control frame currently uses 8-bit source/destination IDs.
- Geometry solver is intentionally outside this firmware module.
- Calibration tables are hardware-dependent and may require retuning for different RF front-ends.
