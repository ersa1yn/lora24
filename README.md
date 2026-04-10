# lora24

Compact framework for 2.4 GHz LoRa ranging experiments.

## Overview

This project is designed for repeatable radio-measurement campaigns with ESP32 and SX1280 nodes.  
It standardizes how ranging data is produced on-device and how results are stored on the backend for later calibration, statistics, and trilateration research.

This repo combines:

- ESP32 + SX1280 firmware (RadioLib)
- Flask receiver for measurement logging
- datasets for calibration and analysis

## What It Covers

- Direct two-node ranging (legacy path in [Ranging](Ranging))
- Three-anchor + one-target coordinated ranging (in [Trilateration](Trilateration))
- Raw data collection only (no final position solver)

## Main Files

- [receiver.py](receiver.py): HTTP endpoint for incoming measurements
- [Ranging](Ranging): legacy ranging and correction workflow
- [Trilateration](Trilateration): ring-scheduled multi-anchor protocol
- [data](data): recorded runs

## Quick Start

1. Install Python 3.10+ and Flask.
2. Start backend:

   ```bash
   python receiver.py
   ```

3. Flash devices with the firmware set you want.
4. Power devices and verify new run folders under [data](data).

## Backend Contract

- Endpoint: POST /reading
- Health: GET /health
- Required payload field: device_name must be a non-empty string

Run output path:

- data/<device_name>/runs/<run_id>/raw_packets.jsonl
- data/<device_name>/runs/<run_id>/run_samples.csv
- data/<device_name>/runs/<run_id>/run_meta.json

## Notes

- Run IDs are persisted on device (NVS).
- Calibration constants should be re-estimated per hardware setup.

## License

See [LICENSE](LICENSE).
