#!/usr/bin/env python3

import argparse
from pathlib import Path


ARRAY_SECTIONS = ("thresholds", "layer1", "layer2", "layer3")
MOUSEEMU_KEYS = {
    "enabled",
    "direction",
    "move_x",
    "move_y",
    "scroll",
}
MOUSEEMU_ARRAY_KEYS = {
    "move_keys",
    "scroll_keys",
    "button_keys",
    "move_keys_deadzones",
    "scroll_keys_deadzones",
}


def parse_layout(path: Path):
    data = {section: [] for section in ARRAY_SECTIONS}
    data["mouseemu"] = {}
    for key in MOUSEEMU_ARRAY_KEYS:
        data[key] = []
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
                    f"expected a known layout section"
                )
            continue

        if current is None:
            raise ValueError(f"{path}:{lineno}: content found before any section")

        if current == "mouseemu":
            if "=" not in line:
                raise ValueError(
                    f"{path}:{lineno}: mouseemu entries should use key = value"
                )
            key, value = [part.strip().lower() for part in line.split("=", 1)]
            if key not in MOUSEEMU_KEYS:
                raise ValueError(
                    f"{path}:{lineno}: unknown mouseemu key '{key}'"
                )
            data[current][key] = value
            continue

        for token in line.split():
            if token != "|":
                data[current].append(token)

    return data


def parse_thresholds(tokens, path: Path):
    thresholds = []
    for idx, token in enumerate(tokens):
        try:
            value = int(token, 10)
        except ValueError as exc:
            raise ValueError(
                f"{path}: thresholds token #{idx} '{token}' is not an integer"
            ) from exc

        if value < 1 or value > 1023:
            raise ValueError(
                f"{path}: thresholds token #{idx} value {value} is outside 1..1023"
            )
        thresholds.append(value)

    return thresholds


def normalize_key_token(token: str):
    return token if token.startswith("KEY_") else f"KEY_{token}"


def parse_bool(value: str, path: Path, key: str):
    lowered = value.lower()
    if lowered in ("true", "1", "yes", "on"):
        return "true"
    if lowered in ("false", "0", "no", "off"):
        return "false"
    raise ValueError(f"{path}: mouseemu {key} should be true/false")


def parse_ratio(value: str, path: Path, key: str):
    if "/" not in value:
        raise ValueError(f"{path}: mouseemu {key} should be in NUM/DEN form")
    num_str, den_str = [part.strip() for part in value.split("/", 1)]
    try:
        num = int(num_str, 10)
        den = int(den_str, 10)
    except ValueError as exc:
        raise ValueError(f"{path}: mouseemu {key} ratio should be integers") from exc
    if den == 0:
        raise ValueError(f"{path}: mouseemu {key} denominator should not be zero")
    return num, den


def parse_mouseemu_indices(tokens, path: Path, key: str, max_count: int):
    if len(tokens) > max_count:
        raise ValueError(
            f"{path}: mouseemu {key} has {len(tokens)} entries, max is {max_count}"
        )
    values = []
    for idx, token in enumerate(tokens):
        try:
            values.append(int(token, 10))
        except ValueError as exc:
            raise ValueError(
                f"{path}: mouseemu {key} token #{idx} '{token}' is not an integer"
            ) from exc
    return values


def format_c_array(values, wrap=8):
    lines = []
    for start in range(0, len(values), wrap):
        chunk = values[start : start + wrap]
        lines.append("    " + ", ".join(chunk) + ",")
    return "\n".join(lines) if lines else ""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--layout", required=True)
    parser.add_argument("--out-c", required=True)
    parser.add_argument("--out-h", required=True)
    args = parser.parse_args()

    layout_path = Path(args.layout)
    out_c = Path(args.out_c)
    out_h = Path(args.out_h)

    sections = parse_layout(layout_path)

    thresholds = parse_thresholds(sections["thresholds"], layout_path)
    layer1 = [normalize_key_token(token) for token in sections["layer1"]]
    layer2 = [normalize_key_token(token) for token in sections["layer2"]]
    layer3 = [normalize_key_token(token) for token in sections["layer3"]]

    key_count = len(thresholds)
    if key_count == 0:
        raise ValueError(f"{layout_path}: thresholds section is empty")

    for name, layer in (("layer1", layer1), ("layer2", layer2), ("layer3", layer3)):
        if len(layer) not in (0, key_count):
            raise ValueError(
                f"{layout_path}: {name} has {len(layer)} entries, expected {key_count}"
            )

    if not layer1:
        raise ValueError(f"{layout_path}: layer1 section is empty")
    if not layer2:
        raise ValueError(f"{layout_path}: layer2 section is empty")
    if not layer3:
        layer3 = ["KEY_NOKEY"] * key_count

    mouseemu_cfg = sections["mouseemu"]
    mouseemu_enabled = parse_bool(mouseemu_cfg.get("enabled", "false"), layout_path,
                                  "enabled")
    mouseemu_direction = mouseemu_cfg.get("direction", "4way")
    if mouseemu_direction == "4way":
        mouseemu_direction = "KB_MOUSEEMU_DIRECTION_4_WAY"
    elif mouseemu_direction == "8way":
        mouseemu_direction = "KB_MOUSEEMU_DIRECTION_8_WAY"
    else:
        raise ValueError(f"{layout_path}: mouseemu direction should be 4way or 8way")

    move_x_num, move_x_den = parse_ratio(
        mouseemu_cfg.get("move_x", "1/1"), layout_path, "move_x"
    )
    move_y_num, move_y_den = parse_ratio(
        mouseemu_cfg.get("move_y", "1/1"), layout_path, "move_y"
    )
    scroll_num, scroll_den = parse_ratio(
        mouseemu_cfg.get("scroll", "1/1"), layout_path, "scroll"
    )

    move_keys = parse_mouseemu_indices(
        sections["move_keys"], layout_path, "move_keys", 8
    )
    scroll_keys = parse_mouseemu_indices(
        sections["scroll_keys"], layout_path, "scroll_keys", 2
    )
    button_keys = parse_mouseemu_indices(
        sections["button_keys"], layout_path, "button_keys", 3
    )
    move_keys_deadzones = parse_mouseemu_indices(
        sections["move_keys_deadzones"], layout_path, "move_keys_deadzones", 8
    )
    scroll_keys_deadzones = parse_mouseemu_indices(
        sections["scroll_keys_deadzones"],
        layout_path,
        "scroll_keys_deadzones",
        2,
    )

    while len(move_keys) < 8:
        move_keys.append(0)
    while len(scroll_keys) < 2:
        scroll_keys.append(0)
    while len(button_keys) < 3:
        button_keys.append(0)
    while len(move_keys_deadzones) < 8:
        move_keys_deadzones.append(0)
    while len(scroll_keys_deadzones) < 2:
        scroll_keys_deadzones.append(0)

    header = f"""#ifndef GENERATED_KB_HANDLER_LAYOUT_H
#define GENERATED_KB_HANDLER_LAYOUT_H

#include <subsys/kb_settings.h>
#include <stdint.h>

#define GENERATED_KB_HANDLER_KEY_COUNT {key_count}U

extern const uint16_t
    generated_kb_handler_default_thresholds[GENERATED_KB_HANDLER_KEY_COUNT];
extern const uint8_t
    generated_kb_handler_default_keymap_layer1[GENERATED_KB_HANDLER_KEY_COUNT];
extern const uint8_t
    generated_kb_handler_default_keymap_layer2[GENERATED_KB_HANDLER_KEY_COUNT];
extern const uint8_t
    generated_kb_handler_default_keymap_layer3[GENERATED_KB_HANDLER_KEY_COUNT];
extern const kb_mouseemu_settings_t generated_kb_handler_default_mouseemu;

#endif // GENERATED_KB_HANDLER_LAYOUT_H
"""

    source = f"""#include "generated_kb_handler_layout.h"

#include <dt-bindings/kb-handler/kb-key-codes.h>

const uint16_t generated_kb_handler_default_thresholds[GENERATED_KB_HANDLER_KEY_COUNT] = {{
{format_c_array([str(value) for value in thresholds])}
}};

const uint8_t generated_kb_handler_default_keymap_layer1[GENERATED_KB_HANDLER_KEY_COUNT] = {{
{format_c_array(layer1)}
}};

const uint8_t generated_kb_handler_default_keymap_layer2[GENERATED_KB_HANDLER_KEY_COUNT] = {{
{format_c_array(layer2)}
}};

const uint8_t generated_kb_handler_default_keymap_layer3[GENERATED_KB_HANDLER_KEY_COUNT] = {{
{format_c_array(layer3)}
}};

const kb_mouseemu_settings_t generated_kb_handler_default_mouseemu = {{
    .enabled = {mouseemu_enabled},
    .direction_mode = {mouseemu_direction},
    .move_keys_count = {len(sections["move_keys"])}U,
    .move_keys = {{{", ".join(str(value) for value in move_keys)}}},
    .scroll_keys_count = {len(sections["scroll_keys"])}U,
    .scroll_keys = {{{", ".join(str(value) for value in scroll_keys)}}},
    .button_keys_count = {len(sections["button_keys"])}U,
    .button_keys = {{{", ".join(str(value) for value in button_keys)}}},
    .move_x_k = (double){move_x_num} / (double){move_x_den},
    .move_y_k = (double){move_y_num} / (double){move_y_den},
    .scroll_k = (double){scroll_num} / (double){scroll_den},
    .move_keys_deadzones = {{{", ".join(str(value) for value in move_keys_deadzones)}}},
    .scroll_keys_deadzones = {{{", ".join(str(value) for value in scroll_keys_deadzones)}}},
}};
"""

    out_h.write_text(header)
    out_c.write_text(source)


if __name__ == "__main__":
    main()
