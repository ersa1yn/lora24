# Trilateration

Three-anchor ranging workflow for controlled LoRa data collection.

## Overview

This module coordinates anchors in a ring so measurements are collected in a consistent order and format.  
Its purpose is to produce clean, comparable ranging datasets across BW and SF settings before any position estimation step.

## Goal

Collect synchronized ranging measurements from a fixed topology:

- Anchor 1 (main master): ID 0x01
- Anchor 2: ID 0x02
- Anchor 3: ID 0x03
- Target: ID 0xff

This module focuses on measurement collection, not final coordinate solving.

## Structure

- [devices](devices): board-specific entry sketches
- [src](src): protocol, node logic, radio link layer, calibration

## Device Sketches

- [devices/anchor_1/anchor_1.ino](devices/anchor_1/anchor_1.ino)
- [devices/anchor_2/anchor_2.ino](devices/anchor_2/anchor_2.ino)
- [devices/anchor_3/anchor_3.ino](devices/anchor_3/anchor_3.ino)
- [devices/target_ff/target_ff.ino](devices/target_ff/target_ff.ino)

## Protocol Basics

- Control packet size: 4 bytes
- Packet types:
  - 0: ranging request
  - 1: master done / pass turn
- Bandwidth IDs:
  - 0: 406250 Hz
  - 1: 812500 Hz
  - 2: 1625000 Hz
  - 3: 203125 Hz
- SF range: 5 to 12

## Config Defaults

From [src/config/ProjectConfig.h](src/config/ProjectConfig.h):

- Default BW: 812500 Hz
- Default SF: 7
- Total channels: 40
- Server URL: http://10.42.0.1:5000/reading

## Run

1. Start backend from repo root:

   ```bash
   python receiver.py
   ```

2. Flash anchor and target sketches.
3. Power all nodes.
4. Check output in:

   - data/<device_name>/runs/<run_id>/raw_packets.jsonl
   - data/<device_name>/runs/<run_id>/run_samples.csv
   - data/<device_name>/runs/<run_id>/run_meta.json

## Dependency

- RadioLib