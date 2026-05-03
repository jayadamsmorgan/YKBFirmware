#!/usr/bin/env python3

import argparse
from pathlib import Path

ARRAY_SECTIONS = ("led_map", "x_coordinates", "y_coordinates")


def c_ident(name: str) -> str:
    out = []
    for ch in name:
        if ch.isalnum():
            out.append(ch)
        else:
            out.append("_")
    return "".join(out)


def parse_layout(path: Path):
    data = {section: [] for section in ARRAY_SECTIONS}
    current = None

    for lineno, raw_line in enumerate(path.read_text().splitlines(), start=1):
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue

        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1].strip().lower()
            if current not in data:
                raise ValueError(
                    f"{path}:{lineno}: unknown section '{current}', "
                    f"expected one of {', '.join(ARRAY_SECTIONS)}")
            continue

        if current is None:
            raise ValueError(
                f"{path}:{lineno}: content found before any section")

        for token in line.split():
            if token != "|":
                data[current].append(token)

    return data


def parse_u16_array(tokens, path: Path, section: str):
    values = []
    for idx, token in enumerate(tokens):
        try:
            value = int(token, 10)
        except ValueError as exc:
            raise ValueError(
                f"{path}: {section} token #{idx} '{token}' is not an integer"
            ) from exc

        if value < 0 or value > 65535:
            raise ValueError(
                f"{path}: {section} token #{idx} value {value} is outside 0..65535"
            )

        values.append(value)

    return values


def format_c_array(values, wrap=8, indent="    "):
    lines = []
    for start in range(0, len(values), wrap):
        chunk = values[start:start + wrap]
        lines.append(indent + ", ".join(values for values in chunk) + ",")
    return "\n".join(lines) if lines else ""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-c", required=True)
    parser.add_argument("--out-h", required=True)
    parser.add_argument("--layout", required=True)
    parser.add_argument("--symbol", default="default_backlight_settings")
    parser.add_argument("bins", nargs="+")
    args = parser.parse_args()

    files = [Path(p) for p in args.bins]
    layout_path = Path(args.layout)
    layout_sections = parse_layout(layout_path)
    led_map = parse_u16_array(layout_sections["led_map"], layout_path,
                              "led_map")
    x_coordinates = parse_u16_array(layout_sections["x_coordinates"],
                                    layout_path, "x_coordinates")
    y_coordinates = parse_u16_array(layout_sections["y_coordinates"],
                                    layout_path, "y_coordinates")

    key_count = len(led_map)
    if key_count == 0:
        raise ValueError(f"{layout_path}: led_map section is empty")
    if len(x_coordinates) != key_count:
        raise ValueError(
            f"{layout_path}: x_coordinates has {len(x_coordinates)} entries, "
            f"expected {key_count}")
    if len(y_coordinates) != key_count:
        raise ValueError(
            f"{layout_path}: y_coordinates has {len(y_coordinates)} entries, "
            f"expected {key_count}")

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
#include <subsys/ykb_backlight.h>

#define GENERATED_BACKLIGHT_LAYOUT_KEY_COUNT {key_count}U

extern const ykb_backlight_settings_t {args.symbol};
""")

    names_init = "\n".join(f'        "{n}",' for n in names)
    offsets_init = ", ".join(str(x) for x in offsets)

    out_c.write_text(f"""\
#include "generated_backlight_resources.h"

#include <zephyr/sys/util.h>

static const uint16_t generated_backlight_led_map[] = {{
{format_c_array([str(value) for value in led_map])}
}};

static const uint16_t generated_backlight_x_coordinates[] = {{
{format_c_array([str(value) for value in x_coordinates])}
}};

static const uint16_t generated_backlight_y_coordinates[] = {{
{format_c_array([str(value) for value in y_coordinates])}
}};

#if CONFIG_KB_HANDLER_SPLITLINK_SLAVE
#define GENERATED_BACKLIGHT_LAYOUT_OFFSET CONFIG_KB_SETTINGS_KEY_COUNT
#define GENERATED_BACKLIGHT_LAYOUT_LOCAL_KEY_COUNT CONFIG_KB_SETTINGS_KEY_COUNT_SLAVE
#else
#define GENERATED_BACKLIGHT_LAYOUT_OFFSET 0U
#define GENERATED_BACKLIGHT_LAYOUT_LOCAL_KEY_COUNT CONFIG_KB_SETTINGS_KEY_COUNT
#endif

BUILD_ASSERT(GENERATED_BACKLIGHT_LAYOUT_KEY_COUNT == TOTAL_KEY_COUNT,
             "generated backlight layout should match TOTAL_KEY_COUNT");

static const ykb_backlight_layout_t generated_backlight_layout = {{
    .key_count = GENERATED_BACKLIGHT_LAYOUT_LOCAL_KEY_COUNT,
    .led_map = &generated_backlight_led_map[GENERATED_BACKLIGHT_LAYOUT_OFFSET],
    .x_coordinates =
        &generated_backlight_x_coordinates[GENERATED_BACKLIGHT_LAYOUT_OFFSET],
    .y_coordinates =
        &generated_backlight_y_coordinates[GENERATED_BACKLIGHT_LAYOUT_OFFSET],
}};

const ykb_backlight_layout_t *ykb_backlight_get_layout(void) {{
    return &generated_backlight_layout;
}}

const ykb_backlight_settings_t {args.symbol} =
{{
    .on = true,
    .script_amount = {len(names)},
    .active_script_index = 2,
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
{format_c_array([str(b) for b in blob], wrap=16, indent="        ")}
    }},
}};
""")


if __name__ == "__main__":
    main()
