#!/usr/bin/env python3

import argparse
import hashlib
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pair-id", required=True)
    parser.add_argument("--out-h", required=True)
    args = parser.parse_args()

    digest = hashlib.sha256(args.pair_id.encode("utf-8")).digest()
    address = bytearray(digest[:8])

    # ESB base addresses are 4 + 4 bytes here. Avoid an all-zero address and
    # keep the first byte non-zero for easier debugging.
    if all(byte == 0 for byte in address):
        address[0] = 0xA5
    elif address[0] == 0:
        address[0] = 0xA5

    bytes_literal = ", ".join(f"0x{byte:02X}" for byte in address)
    header = f"""#ifndef GENERATED_SPLITLINK_ADDRESS_H
#define GENERATED_SPLITLINK_ADDRESS_H

#include <stdint.h>

#define GENERATED_SPLITLINK_PAIR_ID "{args.pair_id}"

static const uint8_t generated_splitlink_esb_address[8] = {{{bytes_literal}}};

#endif // GENERATED_SPLITLINK_ADDRESS_H
"""

    Path(args.out_h).write_text(header)


if __name__ == "__main__":
    main()
