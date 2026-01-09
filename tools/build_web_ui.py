#!/usr/bin/env python3
"""
Build step for Hockey Stick web UI.

- Input:  web/ui.html, hockey_stick.ino
- Output: build/hockey_stick.ino (single file with inlined HTML)

This is intentionally dependency-free (no npm/htmlmin required).
"""

from __future__ import annotations

import argparse
import datetime as _dt
import os
import re
from pathlib import Path


def _light_minify(html: str) -> str:
    # Normalize newlines
    html = html.replace("\r\n", "\n").replace("\r", "\n")

    # Remove HTML comments (safe for our template)
    html = re.sub(r"<!--.*?-->", "", html, flags=re.DOTALL)

    # Strip trailing spaces and drop empty lines
    lines = [ln.rstrip() for ln in html.split("\n")]
    lines = [ln for ln in lines if ln.strip() != ""]

    # Keep newlines (raw string literal friendly, and safe for JS/CSS)
    return "\n".join(lines).strip() + "\n"


def _choose_delimiter(payload: str) -> str:
    base = "HS_WEB_UI"
    if f"){base}" not in payload:
        return base
    # Fallback if payload contains the delimiter
    for i in range(1, 1000):
        d = f"{base}_{i}"
        if f"){d}" not in payload:
            return d
    raise RuntimeError("Could not find a safe raw string delimiter")


def _load_env(env_path: Path) -> dict[str, str]:
    """Load .env file and return dict of key=value pairs."""
    env_vars = {}
    if not env_path.exists():
        raise SystemExit(f".env file does not exist: {env_path}\nCreate one from .env.example")
    
    for line in env_path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        # Skip empty lines and comments
        if not line or line.startswith("#"):
            continue
        # Parse KEY=VALUE
        if "=" in line:
            key, value = line.split("=", 1)
            key = key.strip()
            value = value.strip()
            # Remove quotes if present
            if value.startswith('"') and value.endswith('"'):
                value = value[1:-1]
            elif value.startswith("'") and value.endswith("'"):
                value = value[1:-1]
            env_vars[key] = value
    
    return env_vars


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--html", required=True, help="Path to web/ui.html")
    ap.add_argument("--ino", required=True, help="Path to source hockey_stick.ino")
    ap.add_argument("--out", required=True, help="Path to output build/hockey_stick.ino")
    ap.add_argument("--env", default=".env", help="Path to .env file (default: .env)")
    args = ap.parse_args()

    html_path = Path(args.html)
    ino_path = Path(args.ino)
    out_path = Path(args.out)
    env_path = Path(args.env)

    if not html_path.exists():
        raise SystemExit(f"HTML input does not exist: {html_path}")
    if not ino_path.exists():
        raise SystemExit(f"INO input does not exist: {ino_path}")
    
    # Load environment variables from .env
    env_vars = _load_env(env_path)

    # Read and process HTML
    html = html_path.read_text(encoding="utf-8")
    html = _light_minify(html)
    delim = _choose_delimiter(html)

    # Generate the PROGMEM definition
    stamp = _dt.datetime.now().isoformat(timespec="seconds")
    progmem_def = (
        "// AUTO-GENERATED WEB UI (from web/ui.html)\n"
        f"// Generated: {stamp}\n"
        "#include <pgmspace.h>\n"
        f"const char WEB_UI_HTML[] PROGMEM = R\"{delim}(\n"
        f"{html}"
        f"){delim}\";\n"
    )

    # Read source .ino file
    ino_content = ino_path.read_text(encoding="utf-8")

    # Replace the #include line with the inlined definition
    include_pattern = r'#include\s+"web_ui\.generated\.h"'
    if not re.search(include_pattern, ino_content):
        raise SystemExit(f"Could not find #include \"web_ui.generated.h\" in {ino_path}")
    
    ino_content = re.sub(include_pattern, progmem_def, ino_content)
    
    # Replace template variables from .env
    for key, value in env_vars.items():
        placeholder = f"{{{{{key}}}}}"
        ino_content = ino_content.replace(placeholder, value)

    # Write the combined file
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(ino_content, encoding="utf-8")
    print(f"Wrote {out_path}")


if __name__ == "__main__":
    main()

