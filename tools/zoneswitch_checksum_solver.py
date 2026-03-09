#!/usr/bin/env python3
import argparse
import re
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Callable


@dataclass
class Frame:
    raw: list[int]
    source_file: str

    @property
    def dst(self) -> int:
        return self.raw[1]

    @property
    def src(self) -> int:
        return self.raw[2]

    @property
    def seq(self) -> int:
        return self.raw[3]

    @property
    def cmd(self) -> int:
        return self.raw[4]

    @property
    def arg0(self) -> int:
        return self.raw[5]

    @property
    def arg1_or_mask(self) -> int:
        return self.raw[6]

    @property
    def chk(self) -> int:
        return self.raw[7]


def parse_capture(path: Path) -> list[Frame]:
    lines = [line for line in path.read_text().splitlines() if "<<<" in line]
    frames: list[Frame] = []

    for line in lines:
        payload = line.split("<<<", 1)[1]
        hexbytes = re.findall(r"\b[0-9A-F]{2}\b", payload)
        vals = [int(value, 16) for value in hexbytes]

        for index in range(0, len(vals), 9):
            chunk = vals[index : index + 9]
            if len(chunk) == 9:
                frames.append(Frame(raw=chunk, source_file=str(path)))

    return frames


def split_families(frames: list[Frame]) -> tuple[list[Frame], list[Frame]]:
    requests = [frame for frame in frames if frame.cmd == 0x01 and frame.arg0 == 0x00]
    responses = [frame for frame in frames if frame.cmd == 0x81 and frame.arg0 == 0x01]
    return requests, responses


def reflect8(value: int) -> int:
    reflected = 0
    for bit in range(8):
        if value & (1 << bit):
            reflected |= 1 << (7 - bit)
    return reflected


def crc8(data: list[int], poly: int, init: int, xorout: int, refin: bool, refout: bool) -> int:
    crc = init
    for byte in data:
        if refin:
            byte = reflect8(byte)
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) & 0xFF) ^ poly
            else:
                crc = (crc << 1) & 0xFF
    if refout:
        crc = reflect8(crc)
    return crc ^ xorout


def score_formula(frames: list[Frame], formula: Callable[[Frame], int]) -> int:
    return sum(1 for frame in frames if (formula(frame) & 0xFF) == frame.chk)


def run_simple_models(frames: list[Frame]) -> list[tuple[str, int, int]]:
    models: list[tuple[str, Callable[[Frame], int]]] = [
        ("xor(b1..b6)", lambda frame: frame.raw[1] ^ frame.raw[2] ^ frame.raw[3] ^ frame.raw[4] ^ frame.raw[5] ^ frame.raw[6]),
        ("sum(b1..b6)", lambda frame: sum(frame.raw[1:7])),
        ("twos(sum b1..b6)", lambda frame: ((~sum(frame.raw[1:7])) + 1)),
        ("xor(seq,arg)", lambda frame: frame.seq ^ frame.arg1_or_mask),
        ("sum(seq,arg)", lambda frame: frame.seq + frame.arg1_or_mask),
        ("seq", lambda frame: frame.seq),
    ]

    results = []
    for name, model in models:
        matches = score_formula(frames, model)
        results.append((name, matches, len(frames)))

    return results


def run_xor_plus_const(frames: list[Frame]) -> list[tuple[str, int]]:
    formulas = {
        "xor(b1..b6)+k": lambda frame, k: (frame.raw[1] ^ frame.raw[2] ^ frame.raw[3] ^ frame.raw[4] ^ frame.raw[5] ^ frame.raw[6] ^ k),
        "xor(seq,arg)+k": lambda frame, k: (frame.seq ^ frame.arg1_or_mask ^ k),
    }

    best: list[tuple[str, int, int]] = []
    for name, formula in formulas.items():
        max_match = -1
        best_k = -1
        for k in range(256):
            matches = sum(1 for frame in frames if (formula(frame, k) & 0xFF) == frame.chk)
            if matches > max_match:
                max_match = matches
                best_k = k
        best.append((name, best_k, max_match))

    return [(entry[0], entry[1]) for entry in sorted(best, key=lambda item: item[2], reverse=True)]


def run_crc_search(frames: list[Frame], full: bool) -> list[str]:
    if not frames:
        return []

    windows = {
        "0..6": lambda frame: frame.raw[0:7],
        "1..6": lambda frame: frame.raw[1:7],
    }

    common_polys = [0x07, 0x31, 0x1D, 0x9B, 0x39, 0xA6, 0xD5]
    polys = list(range(1, 256, 2)) if full else common_polys

    findings: list[str] = []
    for window_name, selector in windows.items():
        found = False
        for refin in (False, True):
            for refout in (False, True):
                for poly in polys:
                    for init in range(256):
                        xorout = crc8(selector(frames[0]), poly, init, 0, refin, refout) ^ frames[0].chk
                        ok = True
                        for frame in frames[1:]:
                            expected = crc8(selector(frame), poly, init, xorout, refin, refout)
                            if expected != frame.chk:
                                ok = False
                                break
                        if ok:
                            findings.append(
                                f"crc8 exact match window={window_name} poly=0x{poly:02X} init=0x{init:02X} xorout=0x{xorout:02X} refin={refin} refout={refout}"
                            )
                            found = True
                            break
                    if found:
                        break
                if found:
                    break
            if found:
                break

    return findings


def summarize_pairs(frames: list[Frame]) -> str:
    pairs = list(zip(frames[0::2], frames[1::2]))
    same_seq = sum(1 for request, response in pairs if request.seq == response.seq)
    req_types = Counter((request.dst, request.src, request.cmd, request.arg0) for request, _ in pairs)
    rsp_types = Counter((response.dst, response.src, response.cmd, response.arg0) for _, response in pairs)
    return (
        f"pairs={len(pairs)} same_seq={same_seq} "
        f"req_types={dict(req_types)} rsp_types={dict(rsp_types)}"
    )


def print_family_report(name: str, frames: list[Frame], full_crc: bool) -> None:
    print(f"\n=== {name} ({len(frames)} frames) ===")
    if not frames:
        print("no frames")
        return

    for model_name, matches, total in run_simple_models(frames):
        print(f"simple: {model_name:20s} -> {matches}/{total}")

    for formula_name, best_k in run_xor_plus_const(frames):
        print(f"best-const: {formula_name:20s} k=0x{best_k:02X}")

    findings = run_crc_search(frames, full=full_crc)
    if findings:
        for finding in findings:
            print(f"crc: {finding}")
    else:
        print("crc: no exact crc8 match in searched space")


def main() -> int:
    parser = argparse.ArgumentParser(description="Analyze ZoneSwitch 9-byte frame captures and search checksum models.")
    parser.add_argument(
        "captures",
        nargs="*",
        default=[
            "docs/research/saved_rs485_packets.md",
            "docs/research/saved_rs485_packets2.md",
        ],
        help="Capture markdown/text files containing UART hex lines.",
    )
    parser.add_argument(
        "--full-crc",
        action="store_true",
        help="Search all odd CRC-8 polynomials (slower).",
    )
    args = parser.parse_args()

    all_frames: list[Frame] = []
    for capture in args.captures:
        path = Path(capture)
        if not path.exists():
            print(f"skip missing capture: {capture}")
            continue
        frames = parse_capture(path)
        print(f"loaded {len(frames)} frames from {capture}")
        print(f"shape: sof={Counter(frame.raw[0] for frame in frames)} eof={Counter(frame.raw[8] for frame in frames)}")
        all_frames.extend(frames)

    if not all_frames:
        print("no frames loaded")
        return 1

    requests, responses = split_families(all_frames)
    print(f"\nall frames: {len(all_frames)}")
    print(summarize_pairs(all_frames))

    print_family_report("REQUEST family (CMD=0x01,ARG0=0x00)", requests, full_crc=args.full_crc)
    print_family_report("RESPONSE family (CMD=0x81,ARG0=0x01)", responses, full_crc=args.full_crc)

    mask_values = sorted(set(frame.arg1_or_mask for frame in responses))
    req_arg_values = sorted(set(frame.arg1_or_mask for frame in requests))
    print(f"\nrequest ARG1 values: {[hex(value) for value in req_arg_values]}")
    print(f"response MASK values: {[hex(value) for value in mask_values]}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
