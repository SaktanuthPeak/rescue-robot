"""Process-wide camera service for FireBot.

Supports physical video devices (USB Webcam, V4L2, Raspberry Pi camera) when available,
with an automatic pure-Python synthetic HUD/viewfinder frame generator as a zero-dependency
fallback for offline/mock development and testing.
"""

from __future__ import annotations

import asyncio
import io
import math
import struct
import time
from collections.abc import AsyncGenerator
from contextlib import suppress
from dataclasses import dataclass
from datetime import UTC, datetime
from typing import Literal

from loguru import logger

from ..core.config import Settings

# ---------------------------------------------------------------------------
# Minimal Pure-Python Baseline JPEG Encoder
# ---------------------------------------------------------------------------

_ZIGZAG = [
    0,
    1,
    5,
    6,
    14,
    15,
    27,
    28,
    2,
    4,
    7,
    13,
    16,
    26,
    29,
    42,
    3,
    8,
    12,
    17,
    25,
    30,
    41,
    43,
    9,
    11,
    18,
    24,
    31,
    40,
    44,
    53,
    10,
    19,
    23,
    32,
    39,
    45,
    52,
    54,
    20,
    22,
    33,
    38,
    46,
    51,
    55,
    60,
    21,
    34,
    37,
    47,
    50,
    56,
    59,
    61,
    35,
    36,
    48,
    49,
    57,
    58,
    62,
    63,
]

_LUM_QUANT = [
    16,
    11,
    10,
    16,
    24,
    40,
    51,
    61,
    12,
    12,
    14,
    19,
    26,
    58,
    60,
    55,
    14,
    13,
    16,
    24,
    40,
    57,
    69,
    56,
    14,
    17,
    22,
    29,
    51,
    87,
    80,
    62,
    18,
    22,
    37,
    56,
    68,
    109,
    103,
    77,
    24,
    35,
    55,
    64,
    81,
    104,
    113,
    92,
    49,
    64,
    78,
    87,
    103,
    121,
    120,
    101,
    72,
    92,
    95,
    98,
    112,
    100,
    103,
    99,
]

_CHROMA_QUANT = [
    17,
    18,
    24,
    47,
    99,
    99,
    99,
    99,
    18,
    21,
    26,
    66,
    99,
    99,
    99,
    99,
    24,
    26,
    56,
    99,
    99,
    99,
    99,
    99,
    47,
    66,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
    99,
]

_STD_DC_LUM_NRCODES = [0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0]
_STD_DC_LUM_VALUES = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]

_STD_AC_LUM_NRCODES = [0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125]
_STD_AC_LUM_VALUES = [
    0x01,
    0x02,
    0x03,
    0x00,
    0x04,
    0x11,
    0x05,
    0x12,
    0x21,
    0x31,
    0x41,
    0x06,
    0x13,
    0x51,
    0x61,
    0x07,
    0x22,
    0x71,
    0x14,
    0x32,
    0x81,
    0x91,
    0xA1,
    0x08,
    0x23,
    0x42,
    0xB1,
    0xC1,
    0x15,
    0x52,
    0xD1,
    0xF0,
    0x24,
    0x33,
    0x62,
    0x72,
    0x82,
    0x09,
    0x0A,
    0x16,
    0x17,
    0x18,
    0x19,
    0x1A,
    0x25,
    0x26,
    0x27,
    0x28,
    0x29,
    0x2A,
    0x34,
    0x35,
    0x36,
    0x37,
    0x38,
    0x39,
    0x3A,
    0x43,
    0x44,
    0x45,
    0x46,
    0x47,
    0x48,
    0x49,
    0x4A,
    0x53,
    0x54,
    0x55,
    0x56,
    0x57,
    0x58,
    0x59,
    0x5A,
    0x63,
    0x64,
    0x65,
    0x66,
    0x67,
    0x68,
    0x69,
    0x6A,
    0x73,
    0x74,
    0x75,
    0x76,
    0x77,
    0x78,
    0x79,
    0x7A,
    0x83,
    0x84,
    0x85,
    0x86,
    0x87,
    0x88,
    0x89,
    0x8A,
    0x92,
    0x93,
    0x94,
    0x95,
    0x96,
    0x97,
    0x98,
    0x99,
    0x9A,
    0xA2,
    0xA3,
    0xA4,
    0xA5,
    0xA6,
    0xA7,
    0xA8,
    0xA9,
    0xAA,
    0xB2,
    0xB3,
    0xB4,
    0xB5,
    0xB6,
    0xB7,
    0xB8,
    0xB9,
    0xBA,
    0xC2,
    0xC3,
    0xC4,
    0xC5,
    0xC6,
    0xC7,
    0xC8,
    0xC9,
    0xCA,
    0xD2,
    0xD3,
    0xD4,
    0xD5,
    0xD6,
    0xD7,
    0xD8,
    0xD9,
    0xDA,
    0xE1,
    0xE2,
    0xE3,
    0xE4,
    0xE5,
    0xE6,
    0xE7,
    0xE8,
    0xE9,
    0xEA,
    0xF1,
    0xF2,
    0xF3,
    0xF4,
    0xF5,
    0xF6,
    0xF7,
    0xF8,
    0xF9,
    0xFA,
]

_STD_DC_CHROMA_NRCODES = [0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0]
_STD_DC_CHROMA_VALUES = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]

_STD_AC_CHROMA_NRCODES = [0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119]
_STD_AC_CHROMA_VALUES = [
    0x00,
    0x01,
    0x02,
    0x03,
    0x11,
    0x04,
    0x05,
    0x21,
    0x31,
    0x06,
    0x12,
    0x41,
    0x51,
    0x07,
    0x61,
    0x71,
    0x13,
    0x22,
    0x32,
    0x81,
    0x08,
    0x14,
    0x42,
    0x91,
    0xA1,
    0xB1,
    0xC1,
    0x09,
    0x23,
    0x33,
    0x52,
    0xF0,
    0x15,
    0x62,
    0x72,
    0xD1,
    0x0A,
    0x16,
    0x24,
    0x34,
    0xE1,
    0x25,
    0xF1,
    0x17,
    0x18,
    0x19,
    0x1A,
    0x26,
    0x27,
    0x28,
    0x29,
    0x2A,
    0x35,
    0x36,
    0x37,
    0x38,
    0x39,
    0x3A,
    0x43,
    0x44,
    0x45,
    0x46,
    0x47,
    0x48,
    0x49,
    0x4A,
    0x53,
    0x54,
    0x55,
    0x56,
    0x57,
    0x58,
    0x59,
    0x5A,
    0x63,
    0x64,
    0x65,
    0x66,
    0x67,
    0x68,
    0x69,
    0x6A,
    0x73,
    0x74,
    0x75,
    0x76,
    0x77,
    0x78,
    0x79,
    0x7A,
    0x82,
    0x83,
    0x84,
    0x85,
    0x86,
    0x87,
    0x88,
    0x89,
    0x8A,
    0x92,
    0x93,
    0x94,
    0x95,
    0x96,
    0x97,
    0x98,
    0x99,
    0x9A,
    0xA2,
    0xA3,
    0xA4,
    0xA5,
    0xA6,
    0xA7,
    0xA8,
    0xA9,
    0xAA,
    0xB2,
    0xB3,
    0xB4,
    0xB5,
    0xB6,
    0xB7,
    0xB8,
    0xB9,
    0xBA,
    0xC2,
    0xC3,
    0xC4,
    0xC5,
    0xC6,
    0xC7,
    0xC8,
    0xC9,
    0xCA,
    0xD2,
    0xD3,
    0xD4,
    0xD5,
    0xD6,
    0xD7,
    0xD8,
    0xD9,
    0xDA,
    0xE2,
    0xE3,
    0xE4,
    0xE5,
    0xE6,
    0xE7,
    0xE8,
    0xE9,
    0xEA,
    0xF2,
    0xF3,
    0xF4,
    0xF5,
    0xF6,
    0xF7,
    0xF8,
    0xF9,
    0xFA,
]


def _compute_huffman_table(nrcodes: list[int], values: list[int]) -> tuple[dict[int, tuple[int, int]], list[int]]:
    ht: dict[int, tuple[int, int]] = {}
    code = 0
    val_idx = 0
    for length in range(1, 17):
        count = nrcodes[length]
        for _ in range(count):
            val = values[val_idx]
            ht[val] = (code, length)
            code += 1
            val_idx += 1
        code <<= 1
    return ht, values


_HT_DC_LUM, _ = _compute_huffman_table(_STD_DC_LUM_NRCODES, _STD_DC_LUM_VALUES)
_HT_AC_LUM, _ = _compute_huffman_table(_STD_AC_LUM_NRCODES, _STD_AC_LUM_VALUES)
_HT_DC_CHROMA, _ = _compute_huffman_table(_STD_DC_CHROMA_NRCODES, _STD_DC_CHROMA_VALUES)
_HT_AC_CHROMA, _ = _compute_huffman_table(_STD_AC_CHROMA_NRCODES, _STD_AC_CHROMA_VALUES)


class _BitWriter:
    __slots__ = ("bits", "buffer", "byte")

    def __init__(self) -> None:
        self.buffer = bytearray()
        self.byte = 0
        self.bits = 0

    def write_bits(self, val: int, count: int) -> None:
        while count > 0:
            shift = 8 - self.bits
            take = min(count, shift)
            mask = (1 << take) - 1
            bits_to_add = (val >> (count - take)) & mask
            self.byte = (self.byte << take) | bits_to_add
            self.bits += take
            count -= take
            if self.bits == 8:
                self.buffer.append(self.byte)
                if self.byte == 0xFF:
                    self.buffer.append(0x00)
                self.byte = 0
                self.bits = 0

    def flush(self) -> None:
        if self.bits > 0:
            self.byte <<= 8 - self.bits
            self.buffer.append(self.byte)
            if self.byte == 0xFF:
                self.buffer.append(0x00)
            self.byte = 0
            self.bits = 0


def _fdct_8x8(block: list[float]) -> list[float]:
    """1D Fast Cosine Transform 2-pass approximation."""
    out = [0.0] * 64
    c = [0.35355339059327373] + [0.5] * 7  # 1/sqrt(8), 1/2
    # Pass 1: rows
    temp = [0.0] * 64
    for y in range(8):
        row_offset = y * 8
        for u in range(8):
            s = 0.0
            for x in range(8):
                s += block[row_offset + x] * math.cos((2 * x + 1) * u * math.pi / 16.0)
            temp[row_offset + u] = s * c[u]
    # Pass 2: columns
    for x in range(8):
        for v in range(8):
            s = 0.0
            for y in range(8):
                s += temp[y * 8 + x] * math.cos((2 * y + 1) * v * math.pi / 16.0)
            out[v * 8 + x] = s * c[v]
    return out


def _encode_block(
    block: list[float],
    prev_dc: int,
    q_table: list[int],
    dc_ht: dict[int, tuple[int, int]],
    ac_ht: dict[int, tuple[int, int]],
    bw: _BitWriter,
) -> int:
    dct = _fdct_8x8(block)
    # Quantize and Zigzag
    quant_zz = [0] * 64
    for i in range(64):
        zz_idx = _ZIGZAG[i]
        val = int(round(dct[zz_idx] / q_table[zz_idx]))
        quant_zz[i] = val

    # Encode DC
    dc_val = quant_zz[0]
    diff = dc_val - prev_dc
    if diff == 0:
        code, length = dc_ht[0]
        bw.write_bits(code, length)
    else:
        abs_diff = abs(diff)
        cat = abs_diff.bit_length()
        code, length = dc_ht[cat]
        bw.write_bits(code, length)
        bits_val = diff if diff > 0 else (diff - 1) & ((1 << cat) - 1)
        bw.write_bits(bits_val, cat)

    # Encode AC
    r = 0
    for i in range(1, 64):
        ac_val = quant_zz[i]
        if ac_val == 0:
            r += 1
        else:
            while r > 15:
                code, length = ac_ht[0xF0]  # ZRL
                bw.write_bits(code, length)
                r -= 16
            abs_val = abs(ac_val)
            cat = abs_val.bit_length()
            symbol = (r << 4) | cat
            code, length = ac_ht[symbol]
            bw.write_bits(code, length)
            bits_val = ac_val if ac_val > 0 else (ac_val - 1) & ((1 << cat) - 1)
            bw.write_bits(bits_val, cat)
            r = 0

    if r > 0:
        code, length = ac_ht[0x00]  # EOB
        bw.write_bits(code, length)

    return dc_val


def encode_rgb_to_jpeg(width: int, height: int, rgb_bytes: bytes) -> bytes:
    """Encodes raw RGB byte buffer (width * height * 3) into baseline JPEG bytes."""
    bw = _BitWriter()

    # Pad to multiple of 8
    pad_w = (width + 7) & ~7
    pad_h = (height + 7) & ~7

    y_blocks: list[list[float]] = []
    cb_blocks: list[list[float]] = []
    cr_blocks: list[list[float]] = []

    for by in range(0, pad_h, 8):
        for bx in range(0, pad_w, 8):
            yb = [0.0] * 64
            cbb = [0.0] * 64
            crb = [0.0] * 64
            idx = 0
            for py in range(8):
                curr_y = min(by + py, height - 1)
                row_offset = curr_y * width * 3
                for px in range(8):
                    curr_x = min(bx + px, width - 1)
                    p_offset = row_offset + curr_x * 3
                    r = rgb_bytes[p_offset]
                    g = rgb_bytes[p_offset + 1]
                    b = rgb_bytes[p_offset + 2]
                    # Convert to YCbCr - 128
                    y_val = 0.299 * r + 0.587 * g + 0.114 * b - 128.0
                    cb_val = -0.168736 * r - 0.331264 * g + 0.5 * b
                    cr_val = 0.5 * r - 0.418688 * g - 0.081312 * b
                    yb[idx] = y_val
                    cbb[idx] = cb_val
                    crb[idx] = cr_val
                    idx += 1
            y_blocks.append(yb)
            cb_blocks.append(cbb)
            cr_blocks.append(crb)

    # Encode Scan Data
    prev_y_dc = 0
    prev_cb_dc = 0
    prev_cr_dc = 0
    for i in range(len(y_blocks)):
        prev_y_dc = _encode_block(y_blocks[i], prev_y_dc, _LUM_QUANT, _HT_DC_LUM, _HT_AC_LUM, bw)
        prev_cb_dc = _encode_block(cb_blocks[i], prev_cb_dc, _CHROMA_QUANT, _HT_DC_CHROMA, _HT_AC_CHROMA, bw)
        prev_cr_dc = _encode_block(cr_blocks[i], prev_cr_dc, _CHROMA_QUANT, _HT_DC_CHROMA, _HT_AC_CHROMA, bw)

    bw.flush()

    # Build standard JPEG container
    out = io.BytesIO()
    # SOI
    out.write(b"\xff\xd8")
    # APP0 JFIF
    out.write(b"\xff\xe0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00")
    # DQT
    lum_dqt_data = bytes(_LUM_QUANT)
    out.write(b"\xff\xdb\x00\x43\x00" + lum_dqt_data)
    chroma_dqt_data = bytes(_CHROMA_QUANT)
    out.write(b"\xff\xdb\x00\x43\x01" + chroma_dqt_data)
    # SOF0 Baseline DCT
    sof_len = 8 + 3 * 3
    out.write(struct.pack(">BBHBHHB", 0xFF, 0xC0, sof_len, 8, height, width, 3))
    out.write(b"\x01\x11\x00\x02\x11\x01\x03\x11\x01")

    # DHT
    def _write_dht(table_class: int, table_id: int, nrcodes: list[int], values: list[int]) -> None:
        data = bytes(nrcodes[1:17]) + bytes(values)
        out.write(struct.pack(">BBHB", 0xFF, 0xC4, len(data) + 3, (table_class << 4) | table_id))
        out.write(data)

    _write_dht(0, 0, _STD_DC_LUM_NRCODES, _STD_DC_LUM_VALUES)
    _write_dht(1, 0, _STD_AC_LUM_NRCODES, _STD_AC_LUM_VALUES)
    _write_dht(0, 1, _STD_DC_CHROMA_NRCODES, _STD_DC_CHROMA_VALUES)
    _write_dht(1, 1, _STD_AC_CHROMA_NRCODES, _STD_AC_CHROMA_VALUES)

    # SOS Start of Scan
    out.write(b"\xff\xda\x00\x0c\x03\x01\x00\x02\x11\x03\x11\x00\x3f\x00")
    out.write(bw.buffer)
    # EOI
    out.write(b"\xff\xd9")

    return out.getvalue()


# ---------------------------------------------------------------------------
# Synthetic Tactical HUD Viewfinder Generator
# ---------------------------------------------------------------------------

_FONT_5X7: dict[str, list[int]] = {
    "0": [0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E],
    "1": [0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E],
    "2": [0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F],
    "3": [0x1F, 0x02, 0x04, 0x06, 0x01, 0x11, 0x0E],
    "4": [0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02],
    "5": [0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E],
    "6": [0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E],
    "7": [0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08],
    "8": [0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E],
    "9": [0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C],
    ":": [0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00],
    "-": [0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00],
    ".": [0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C],
    " ": [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00],
    "A": [0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11],
    "B": [0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E],
    "C": [0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E],
    "D": [0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C],
    "E": [0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F],
    "F": [0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10],
    "I": [0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E],
    "L": [0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F],
    "M": [0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11],
    "O": [0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E],
    "P": [0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10],
    "R": [0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11],
    "S": [0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E],
    "T": [0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04],
    "V": [0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04],
    "K": [0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11],
}


def _draw_char(buf: bytearray, w: int, h: int, x: int, y: int, char: str, color: tuple[int, int, int]) -> None:
    glyph = _FONT_5X7.get(char.upper(), _FONT_5X7[" "])
    r, g, b = color
    for row_idx, row_bits in enumerate(glyph):
        py = y + row_idx
        if 0 <= py < h:
            for col_idx in range(5):
                px = x + col_idx
                if 0 <= px < w:
                    if (row_bits >> (4 - col_idx)) & 1:
                        offset = (py * w + px) * 3
                        buf[offset] = r
                        buf[offset + 1] = g
                        buf[offset + 2] = b


def _draw_text(buf: bytearray, w: int, h: int, x: int, y: int, text: str, color: tuple[int, int, int]) -> None:
    cx = x
    for ch in text:
        _draw_char(buf, w, h, cx, y, ch, color)
        cx += 6


def generate_mock_frame(width: int = 320, height: int = 240, frame_count: int = 0) -> bytes:
    """Generates a synthetic thermal/tactical HUD viewfinder frame in JPEG format."""
    buf = bytearray(width * height * 3)
    cx = width // 2
    cy = height // 2
    t = frame_count * 0.1

    orbit_x = int(cx + math.cos(t * 0.7) * (width * 0.28))
    orbit_y = int(cy + math.sin(t * 0.7) * (height * 0.28))

    for y in range(height):
        row_offset = y * width * 3
        grid_y = y % 20 == 0
        for x in range(width):
            grid_x = x % 20 == 0
            offset = row_offset + x * 3

            dist_center = math.hypot(x - cx, y - cy)
            base_b = int(max(15, 60 - dist_center * 0.15))
            base_g = int(max(10, 30 - dist_center * 0.08))
            base_r = int(max(12, 40 - dist_center * 0.1))

            dist_target = math.hypot(x - orbit_x, y - orbit_y)
            if dist_target < 45:
                intensity = max(0.0, 1.0 - (dist_target / 45.0))
                hot_r = int(255 * intensity)
                hot_g = int(200 * (intensity**2))
                hot_b = int(40 * (intensity**3))
                base_r = min(255, base_r + hot_r)
                base_g = min(255, base_g + hot_g)
                base_b = min(255, base_b + hot_b)

            if grid_x or grid_y:
                base_r = min(255, base_r + 20)
                base_g = min(255, base_g + 30)
                base_b = min(255, base_b + 45)

            buf[offset] = base_r
            buf[offset + 1] = base_g
            buf[offset + 2] = base_b

    hud_green = (0, 255, 120)
    for i in range(-15, 16):
        if -3 < i < 3:
            continue
        if 0 <= cx + i < width:
            off = (cy * width + (cx + i)) * 3
            buf[off], buf[off + 1], buf[off + 2] = hud_green
        if 0 <= cy + i < height:
            off = ((cy + i) * width + cx) * 3
            buf[off], buf[off + 1], buf[off + 2] = hud_green

    if 0 <= orbit_x < width and 0 <= orbit_y < height:
        target_color = (255, 60, 60)
        box_s = 14
        for bx in range(-box_s, box_s + 1):
            for by in (-box_s, box_s):
                py, px = orbit_y + by, orbit_x + bx
                if 0 <= py < height and 0 <= px < width:
                    off = (py * width + px) * 3
                    buf[off], buf[off + 1], buf[off + 2] = target_color
        for by in range(-box_s, box_s + 1):
            for bx in (-box_s, box_s):
                py, px = orbit_y + by, orbit_x + bx
                if 0 <= py < height and 0 <= px < width:
                    off = (py * width + px) * 3
                    buf[off], buf[off + 1], buf[off + 2] = target_color

    now_str = datetime.now(UTC).strftime("%H:%M:%S")
    _draw_text(buf, width, height, 8, 8, f"FIREBOT CAM {now_str}", (0, 255, 120))
    _draw_text(buf, width, height, 8, 20, "MODE: MOCK SIMULATOR", (255, 200, 50))
    _draw_text(buf, width, height, 8, height - 16, f"{width}X{height} 15FPS", (150, 200, 255))
    _draw_text(buf, width, height, width - 64, 8, "LIVE", (255, 50, 50))

    return encode_rgb_to_jpeg(width, height, bytes(buf))


# ---------------------------------------------------------------------------
# Camera Service Class & Singleton
# ---------------------------------------------------------------------------


@dataclass
class CameraStatus:
    active: bool
    source: Literal["auto", "v4l2", "picam", "mock"]
    device: str
    width: int
    height: int
    fps: int
    frame_count: int
    last_frame_age_ms: int
    is_hardware: bool


class CameraService:
    """Manages video capture from hardware or synthetic fallback generator."""

    def __init__(self) -> None:
        self._settings: Settings | None = None
        self._active: bool = True
        self._source_mode: Literal["auto", "v4l2", "picam", "mock"] = "mock"
        self._is_hardware: bool = False
        self._width: int = 320
        self._height: int = 240
        self._fps: int = 15
        self._frame_count: int = 0
        self._latest_jpeg: bytes | None = None
        self._last_frame_time: float = 0.0
        self._cv2_cap: object | None = None
        self._loop_task: asyncio.Task[None] | None = None

    async def start(self, settings: Settings) -> None:
        self._settings = settings
        self._width = min(settings.CAMERA_WIDTH, 640)
        self._height = min(settings.CAMERA_HEIGHT, 480)
        self._fps = max(1, min(settings.CAMERA_FPS, 30))
        self._active = settings.CAMERA_AUTO_START
        self._source_mode = settings.CAMERA_SOURCE

        self._is_hardware = False
        if self._source_mode in ("auto", "v4l2"):
            try:
                import cv2

                device = settings.CAMERA_DEVICE
                candidates = []
                if device.isdigit():
                    candidates.append(int(device))
                else:
                    candidates.append(device)
                    # Extract index if path is like /dev/video0
                    if device.startswith("/dev/video") and device[10:].isdigit():
                        candidates.append(int(device[10:]))
                if 0 not in candidates:
                    candidates.append(0)

                cap = None
                for cand in candidates:
                    try:
                        # Try V4L2 backend first on Linux
                        if hasattr(cv2, "CAP_V4L2"):
                            cap = cv2.VideoCapture(cand, cv2.CAP_V4L2)
                        if cap is None or not cap.isOpened():
                            cap = cv2.VideoCapture(cand)
                        if cap is not None and cap.isOpened():
                            # Successfully opened webcam
                            cap.set(cv2.CAP_PROP_FRAME_WIDTH, self._width)
                            cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self._height)
                            cap.set(cv2.CAP_PROP_FPS, self._fps)
                            # Set MJPG fourcc if supported for faster USB webcam streaming
                            if hasattr(cv2, "VideoWriter_fourcc"):
                                cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
                            self._cv2_cap = cap
                            self._is_hardware = True
                            logger.info(f"Camera hardware opened: {cand}")
                            break
                    except Exception as probe_err:
                        logger.debug(f"Candidate {cand} probe error: {probe_err}")

                if not self._is_hardware:
                    logger.info(f"Camera device {settings.CAMERA_DEVICE} not openable; using mock fallback")
            except Exception as exc:
                logger.info(f"OpenCV/Camera hardware probe: {exc}; using mock fallback")

        self._latest_jpeg = self._capture_frame_sync()
        self._last_frame_time = time.monotonic()

        if self._loop_task is None or self._loop_task.done():
            self._loop_task = asyncio.create_task(self._grabber_loop(), name="camera-grabber")
        logger.info(
            f"Camera service started (source={self._source_mode}, is_hw={self._is_hardware}, {self._width}x{self._height}@{self._fps}fps)"
        )

    async def stop(self) -> None:
        if self._loop_task is not None:
            self._loop_task.cancel()
            with suppress(asyncio.CancelledError):
                await self._loop_task
            self._loop_task = None

        if self._cv2_cap is not None:
            try:
                self._cv2_cap.release()  # type: ignore[attr-defined]
            except Exception:
                pass
            self._cv2_cap = None
        self._is_hardware = False
        logger.info("Camera service stopped")

    def _capture_frame_sync(self) -> bytes:
        self._frame_count += 1
        self._last_frame_time = time.monotonic()

        if self._is_hardware and self._cv2_cap is not None:
            try:
                import cv2

                ret, frame = self._cv2_cap.read()  # type: ignore[attr-defined]
                if ret and frame is not None:
                    encode_param = [int(cv2.IMWRITE_JPEG_QUALITY), 80]
                    success, enc_jpg = cv2.imencode(".jpg", frame, encode_param)
                    if success:
                        return bytes(enc_jpg)
            except Exception as e:
                logger.warning(f"Hardware camera frame read failed ({e}); falling back to mock")

        return generate_mock_frame(self._width, self._height, self._frame_count)

    async def _grabber_loop(self) -> None:
        while True:
            interval = 1.0 / max(self._fps, 1)
            if self._active:
                self._latest_jpeg = self._capture_frame_sync()
            await asyncio.sleep(interval)

    def get_snapshot(self) -> bytes:
        if self._latest_jpeg is None or not self._active:
            self._latest_jpeg = self._capture_frame_sync()
        return self._latest_jpeg

    async def mjpeg_stream(self) -> AsyncGenerator[bytes, None]:
        """Yields multipart MJPEG chunks for continuous video streaming."""
        interval = 1.0 / max(self._fps, 1)
        while True:
            if self._active:
                frame_bytes = self._latest_jpeg or self.get_snapshot()
                yield (
                    b"--frame\r\n"
                    b"Content-Type: image/jpeg\r\n"
                    b"Content-Length: " + str(len(frame_bytes)).encode("ascii") + b"\r\n\r\n" + frame_bytes + b"\r\n"
                )
            await asyncio.sleep(interval)

    def get_status(self) -> CameraStatus:
        age_ms = int(max(time.monotonic() - self._last_frame_time, 0.0) * 1000)
        return CameraStatus(
            active=self._active,
            source=self._source_mode,
            device=self._settings.CAMERA_DEVICE if self._settings else "/dev/video0",
            width=self._width,
            height=self._height,
            fps=self._fps,
            frame_count=self._frame_count,
            last_frame_age_ms=age_ms,
            is_hardware=self._is_hardware,
        )

    def set_active(self, active: bool) -> CameraStatus:
        self._active = active
        return self.get_status()

    def set_config(self, width: int | None = None, height: int | None = None, fps: int | None = None) -> CameraStatus:
        if width:
            self._width = min(max(160, width), 1280)
        if height:
            self._height = min(max(120, height), 720)
        if fps:
            self._fps = min(max(1, fps), 30)
        return self.get_status()


camera_service = CameraService()
