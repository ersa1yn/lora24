# lora24: LoRa Ranging and Trilateration Experimental Framework

5.04.2026

This repository contains an experimental framework for short-range 2.4 GHz LoRa ranging and coordinated multi-anchor data collection. The codebase combines embedded firmware (ESP32 + SX1280 via RadioLib) with a lightweight server-side ingestion pipeline (Flask) to support controlled measurement campaigns across bandwidth and spreading-factor configurations.

Two firmware branches are maintained:

- A legacy two-node ranging branch for direct master-slave calibration and baseline characterization.
- A ring-scheduled trilateration branch with three masters and one slave, designed to standardize coordinated measurements prior to geometric estimation.

## Repository Scope

The project focuses on repeatable collection of raw ranging observables rather than final position estimation. The principal outputs are packet-level and sample-level telemetry suitable for downstream statistical analysis, calibration, and trilateration research.

## Repository Structure

- [README.md](README.md): project-wide overview, setup, and data model.
- [receiver.py](receiver.py): HTTP ingestion service for measurement payloads.
- [Trilateration](Trilateration): coordinated ring protocol with shared link utilities.
- [Ranging](Ranging): legacy direct ranging implementation and calibration workflow.
- [data](data): persisted experiment outputs (example and/or generated runs).

Detailed subsystem documentation:

- [Trilateration/README.md](Trilateration/README.md)
- [Ranging/README.md](Ranging/README.md)

## System Architecture

### Embedded Layer

- Platform: Arduino-compatible ESP32 devices.
- Radio: Semtech SX1280 transceivers controlled through RadioLib.
- Persistence: on-device run identifiers stored in Preferences/NVS.
- Measurements: raw ranging register outputs and ranging RSSI values.

### Ingestion Layer

- Framework: Flask service exposing /reading and /health endpoints.
- Responsibility: schema validation, archival of raw payloads, sample-level CSV materialization, and run metadata maintenance.
- Storage hierarchy: data/<device_name>/runs/<run_id>.

## Experimental Workflow

1. Firmware initializes radio and retrieves persistent run identifier.
2. Master nodes perform configuration exchange and ranging operations.
3. Measurement payload is posted as JSON to the Flask endpoint.
4. Server validates payload consistency and appends records to:
    - raw_packets.jsonl for packet-level traceability,
    - run_samples.csv for sample-level analysis,
    - run_meta.json for run-level metadata and configuration inventory.

## Telemetry and Data Model

The backend supports packed BW/SF descriptors and stores decoded fields for analysis.

Canonical run folder layout:

- data/<device_name>/runs/<run_id>/raw_packets.jsonl
- data/<device_name>/runs/<run_id>/run_samples.csv
- data/<device_name>/runs/<run_id>/run_meta.json

Note on historical datasets: legacy data may appear under data/runs from earlier project versions.

## Software Prerequisites

- Python 3.10 or newer.
- Flask (for receiver.py).
- Arduino IDE or Arduino CLI with ESP32 board support.
- RadioLib library.

## Minimal Execution Procedure

1. Start the ingestion server:
    - python receiver.py
2. Flash the selected firmware branch to each device.
3. Power nodes and observe serial logs for protocol progress.
4. Verify ingestion using /health and the generated run directory.

## Reproducibility Considerations

- Run identifiers are monotonic and persisted in non-volatile storage.
- Measurement quality depends on RF environment, antenna orientation, and hardware-specific calibration constants.
- Calibration assets are provided in rangingCorrection.h and should be re-estimated for new hardware variants.

## Integration Note

The ingestion backend requires a non-empty device_name field in incoming JSON payloads. Firmware variants that define local device-name constants should ensure this value is explicitly transmitted in POST payloads to remain compatible with the current server-side schema.

## License

This repository is distributed under the license declared in [LICENSE](LICENSE).
