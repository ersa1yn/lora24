from flask import Flask, request, jsonify
from datetime import datetime, timezone
from pathlib import Path
import csv
import json
import re
from typing import Any, Iterable, Tuple

app = Flask(__name__)

DATA_ROOT = Path("data")
DATA_ROOT.mkdir(parents=True, exist_ok=True)

TOTAL_CHNL = 40

BW_ID_TO_KHZ = {
    0: 406250,
    1: 812500,
    2: 1625000,
    3: 203125
}

SAFE_DEVICE_RE = re.compile(r"[^A-Za-z0-9._-]+")

def utc_now_iso():
    return datetime.now(timezone.utc).isoformat()


def first_present(data: dict, keys: Iterable[str]) -> Tuple[Any, str | None]:
    for k in keys:
        if k in data and data[k] is not None:
            return data[k], k
    return None, None


def sanitize_device_name(name: str) -> str:
    cleaned = SAFE_DEVICE_RE.sub("_", name.strip())
    return cleaned or "unknown-device"


def resolve_device_name(payload_name: Any):
    if not isinstance(payload_name, str):
        return None
    if not payload_name.strip():
        return None
    return sanitize_device_name(payload_name)


def ensure_run_files(run_dir: Path, device_name: str):
    run_dir.mkdir(parents=True, exist_ok=True)

    samples_csv = run_dir / "run_samples.csv"
    if not samples_csv.exists():
        with samples_csv.open("w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow([
                "timestamp_utc",
                "run_id",
                "device_name",
                "device_id",
                "target_id",
                "bw_id",
                "bw_khz",
                "sf",
                "freq_error",
                "sweep_count",
                "sample_size",
                "valid",
                "timeout",
                "fail",
                "sample_index",
                "raw_rng",
                "rng_rssi",
            ])

    meta_json = run_dir / "run_meta.json"
    if not meta_json.exists():
        meta = {
            "run_id": None,
            "device_name": device_name,
            "created_at_utc": utc_now_iso(),
            "last_update_utc": utc_now_iso(),
            "devices_seen": [],
            "targets_seen": [],
            "configs_seen": [],  # list of {"bw_id":x,"bw_khz":y,"sf":z}
            "packets_received": 0,
            "notes": "Optional: add distance, tx power, firmware hash, antenna, environment"
        }
        meta_json.write_text(json.dumps(meta, indent=2), encoding="utf-8")

def load_meta(meta_path: Path):
    return json.loads(meta_path.read_text(encoding="utf-8"))

def save_meta(meta_path: Path, meta: dict):
    meta["last_update_utc"] = utc_now_iso()
    meta_path.write_text(json.dumps(meta, indent=2), encoding="utf-8")

def append_jsonl(path: Path, obj: dict):
    with path.open("a", encoding="utf-8") as f:
        f.write(json.dumps(obj, ensure_ascii=False) + "\n")

def decode_bw_sf(bw_sf: int):
    bw_id = (bw_sf >> 4) & 0x0F
    sf = bw_sf & 0x0F
    bw_khz = BW_ID_TO_KHZ.get(bw_id, None)
    return bw_id, sf, bw_khz

@app.get("/health")
def health():
    return jsonify({"ok": True, "utc": utc_now_iso()})

@app.post("/reading")
def reading():
    data = request.get_json(silent=True)
    if not data:
        return jsonify({"ok": False, "error": "Invalid or missing JSON"}), 400

    required = [
        "run_id",
        "device_name",
        "device_id",
        "target_id",
        "bw_sf",          # packed uint8
        "valid",
        "timeout",
        "fail",
        "raw_rng",
        "rng_rssi",
    ]
    missing = [k for k in required if k not in data]

    freq_error_raw, freq_key = first_present(data, ["freq_error", "freqError"])
    sample_size_raw, sample_size_key = first_present(data, ["sample_size"])
    sweep_count_raw, sweep_count_key = first_present(data, ["sweepCount", "sweep_count"])

    if freq_key is None:
        missing.append("freq_error|freqError")
    if sample_size_key is None and sweep_count_key is None:
        missing.append("sample_size|sweepCount")

    if missing:
        return jsonify({"ok": False, "error": f"Missing keys: {missing}"}), 400

    if freq_error_raw is None:
        return jsonify({"ok": False, "error": "Missing freq_error/freqError value"}), 400
    if sample_size_key is not None and sample_size_raw is None:
        return jsonify({"ok": False, "error": "sample_size key is present but value is null"}), 400
    if sweep_count_key is not None and sweep_count_raw is None:
        return jsonify({"ok": False, "error": "sweepCount key is present but value is null"}), 400

    try:
        run_id = int(data["run_id"])
        device_id = int(data["device_id"])
        target_id = int(data["target_id"])
        bw_sf = int(data["bw_sf"])
        freq_error = float(freq_error_raw)
        valid = int(data["valid"])
        timeout = int(data["timeout"])
        fail = int(data["fail"])

        sweep_count = None
        if sweep_count_key is not None:
            sweep_count = int(sweep_count_raw)
            if sweep_count < 0:
                return jsonify({"ok": False, "error": "sweepCount must be >= 0"}), 400

        if sample_size_key is not None:
            sample_size = int(sample_size_raw)
        elif sweep_count is not None:
            sample_size = sweep_count * TOTAL_CHNL
        else:
            sample_size = valid + timeout + fail

        if sample_size < 0:
            return jsonify({"ok": False, "error": "sample_size must be >= 0"}), 400

        if sweep_count is None and sample_size >= 0:
            sweep_count = sample_size // TOTAL_CHNL

        raw_rng = data["raw_rng"]
        rng_rssi = data["rng_rssi"]

        device_name = resolve_device_name(data.get("device_name"))
        if device_name is None:
            return jsonify({"ok": False, "error": "device_name must be a non-empty string"}), 400

        if not isinstance(raw_rng, list) or not isinstance(rng_rssi, list):
            return jsonify({"ok": False, "error": "raw_rng and rng_rssi must be arrays"}), 400

        if valid < 0 or sample_size < 0:
            return jsonify({"ok": False, "error": "valid/sample_size must be >= 0"}), 400

        if len(raw_rng) != valid or len(rng_rssi) != valid:
            return jsonify({
                "ok": False,
                "error": f"Array lengths must equal valid ({valid}). Got raw_rng={len(raw_rng)}, rng_rssi={len(rng_rssi)}"
            }), 400

        if timeout + fail + valid != sample_size:
            return jsonify({
                "ok": False,
                "error": f"timeout + fail + valid must equal sample_size ({sample_size})"
            }), 400

        bw_id, sf, bw_khz = decode_bw_sf(bw_sf)

        ts = utc_now_iso()
        run_dir = DATA_ROOT / device_name / "runs" / f"{run_id:06d}"
        ensure_run_files(run_dir, device_name)

        # 1) raw packet archive
        jsonl_path = run_dir / "raw_packets.jsonl"
        append_jsonl(jsonl_path, {
            "received_at_utc": ts,
            "remote_ip": request.remote_addr,
            "device_name": device_name,
            "payload": data
        })

        # 2) append per-sample rows
        samples_csv = run_dir / "run_samples.csv"
        with samples_csv.open("a", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            for i in range(valid):
                w.writerow([
                    ts,
                    run_id,
                    device_name,
                    device_id,
                    target_id,
                    bw_id,
                    bw_khz if bw_khz is not None else "",
                    sf,
                    freq_error,
                    sweep_count,
                    sample_size,
                    valid,
                    timeout,
                    fail,
                    i,
                    int(raw_rng[i]),
                    int(rng_rssi[i]),
                ])

        # 3) update meta
        meta_path = run_dir / "run_meta.json"
        meta = load_meta(meta_path)
        if meta["run_id"] is None:
            meta["run_id"] = run_id
        meta["device_name"] = device_name

        if device_id not in meta["devices_seen"]:
            meta["devices_seen"].append(device_id)
        if target_id not in meta["targets_seen"]:
            meta["targets_seen"].append(target_id)

        cfg = {"bw_id": bw_id, "bw_khz": bw_khz, "sf": sf}
        if cfg not in meta["configs_seen"]:
            meta["configs_seen"].append(cfg)

        meta["packets_received"] += 1
        save_meta(meta_path, meta)

        return jsonify({
            "ok": True,
            "run_id": run_id,
            "device_name": device_name,
            "bw_id": bw_id,
            "sf": sf,
            "bw_khz": bw_khz,
            "run_dir": str(run_dir),
            "rows_written": valid
        }), 200

    except Exception as e:
        return jsonify({"ok": False, "error": f"{type(e).__name__}: {e}"}), 400

if __name__ == "__main__":
    # reachable from ESP on hotspot/LAN
    app.run(host="0.0.0.0", port=5000, debug=False)