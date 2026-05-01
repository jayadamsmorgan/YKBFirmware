#!/usr/bin/env python3

import argparse
from pathlib import Path


def c_ident(name: str) -> str:
    out = []
    for ch in name:
        if ch.isalnum():
            out.append(ch)
        else:
            out.append("_")
    return "".join(out)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-c", required=True)
    parser.add_argument("--out-h", required=True)
    parser.add_argument("--symbol", default="default_backlight_settings")
    parser.add_argument("bins", nargs="+")
    args = parser.parse_args()

    files = [Path(p) for p in args.bins]

    names = []
    offsets = [0]
    blob = bytearray()

    for f in files:
        data = f.read_bytes()

        names.append(f.stem)

        blob.extend(data)
        offsets.append(len(blob))

    out_c = Path(args.out_c)
    out_h = Path(args.out_h)

    out_h.write_text(f"""\
#pragma once

#include <subsys/kb_settings.h>

extern const ykb_backlight_settings_t {args.symbol};
""")

    def byte_array(data: bytes) -> str:
        lines = []
        for i in range(0, len(data), 16):
            chunk = data[i:i + 16]
            lines.append("        " + ", ".join(str(b) for b in chunk) + ",")
        return "\n".join(lines)

    names_init = "\n".join(f'        "{n}",' for n in names)
    offsets_init = ", ".join(str(x) for x in offsets)

    out_c.write_text(f"""\
#include "generated_backlight_resources.h"
#include <subsys/ykb_backlight.h>

const ykb_backlight_settings_t {args.symbol} =
{{
    .on = true,
    .script_amount = {len(names)},
    .active_script_index = 0,
    .speed = 1.0f,
    .brightness = 1.0f,
    .thread_sleep_ms = DEFAULT_THREAD_SLEEP_MS,
    .names =
    {{
{names_init}
    }},
    .offsets = {{ {offsets_init} }},
    .backlight_data =
    {{
{byte_array(blob)}
    }},
}};
""")


if __name__ == "__main__":
    main()
