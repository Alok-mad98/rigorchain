#!/usr/bin/env python3
"""Copy P0-024 vectors into C++ test data and register them in the test binary."""
from __future__ import annotations

import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VEC = ROOT / "formal_verification" / "vectors"
DATA = ROOT / "core" / "src" / "test" / "data"
SRC = ROOT / "core" / "src"

JSON_FILES = [
    "rigor_classification_vectors.json",
    "rigor_sighash_vectors.json",
]

NEW_CPP = [
    "rigor_classification_vector_tests",
    "rigor_sighash_vector_tests",
]


def write_json_h(src: Path, dst: Path) -> None:
    data = src.read_bytes()
    name = src.stem
    lines = [
        "namespace json_tests{",
        f"static unsigned const char {name}[] = {{",
    ]
    for i in range(0, len(data), 8):
        chunk = data[i : i + 8]
        lines.append(", ".join(f"0x{b:02x}" for b in chunk) + ",")
    lines.append("};};")
    dst.write_text("\n".join(lines) + "\n", encoding="utf-8")


def already_has(text: str, snippet: str) -> bool:
    return snippet in text


def insert_after_backslash_lines(text: str, needle: str, extras: list[str]) -> str:
    """Insert extras after list-continuation lines that contain needle and end with \\."""
    if already_has(text, extras[0].strip()):
        return text
    out = []
    for line in text.splitlines(keepends=True):
        out.append(line)
        stripped = line.rstrip("\r\n")
        if needle in stripped and stripped.endswith("\\"):
            indent = line[: len(line) - len(line.lstrip(" \t"))]
            nl = "\n" if line.endswith("\n") else ""
            for extra in extras:
                out.append(f"{indent}{extra}{nl}")
    return "".join(out)


def insert_after_exact_substr(text: str, needle: str, extras: list[str]) -> str:
    if already_has(text, extras[0].strip()):
        return text
    out = []
    for line in text.splitlines(keepends=True):
        out.append(line)
        if needle in line:
            indent = line[: len(line) - len(line.lstrip(" \t"))]
            nl = "\n" if line.endswith("\n") else ""
            for extra in extras:
                out.append(f"{indent}{extra}{nl}")
    return "".join(out)


def clone_compile_rules(text: str) -> str:
    if "test/test_rigor-rigor_classification_vector_tests.o:" in text:
        return text
    marker = "test/test_rigor-rigor_opcode_namespace_tests.o:"
    pred = "test/test_rigor-rigor_predicate_registry_tests.o:"
    start = text.find(marker)
    end = text.find(pred)
    if start < 0 or end < 0 or end <= start:
        raise SystemExit(f"could not locate opcode compile-rule block")
    block = text[start:end]
    inserts = [block.replace("rigor_opcode_namespace_tests", stem) for stem in NEW_CPP]
    return text[:end] + "".join(inserts) + text[end:]


def patch_file(path: Path) -> None:
    text = path.read_text(encoding="utf-8")
    orig = text
    text = insert_after_backslash_lines(
        text,
        "test/data/rigor_script_asset_classifier.json",
        [
            "test/data/rigor_classification_vectors.json \\",
            "test/data/rigor_sighash_vectors.json \\",
        ],
    )
    text = insert_after_backslash_lines(
        text,
        "test/rigor_opcode_namespace_tests.cpp",
        [
            "test/rigor_classification_vector_tests.cpp \\",
            "test/rigor_sighash_vector_tests.cpp \\",
        ],
    )
    text = insert_after_backslash_lines(
        text,
        "test/test_rigor-rigor_opcode_namespace_tests.$(OBJEXT)",
        [
            "test/test_rigor-rigor_classification_vector_tests.$(OBJEXT) \\",
            "test/test_rigor-rigor_sighash_vector_tests.$(OBJEXT) \\",
        ],
    )
    text = insert_after_backslash_lines(
        text,
        "test/$(DEPDIR)/test_rigor-rigor_opcode_namespace_tests.Po",
        [
            "test/$(DEPDIR)/test_rigor-rigor_classification_vector_tests.Po \\",
            "test/$(DEPDIR)/test_rigor-rigor_sighash_vector_tests.Po \\",
        ],
    )
    text = insert_after_exact_substr(
        text,
        "include test/$(DEPDIR)/test_rigor-rigor_opcode_namespace_tests.Po",
        [
            "include test/$(DEPDIR)/test_rigor-rigor_classification_vector_tests.Po # am--include-marker",
            "include test/$(DEPDIR)/test_rigor-rigor_sighash_vector_tests.Po # am--include-marker",
        ],
    )
    text = insert_after_exact_substr(
        text,
        "-rm -f test/$(DEPDIR)/test_rigor-rigor_opcode_namespace_tests.Po",
        [
            "-rm -f test/$(DEPDIR)/test_rigor-rigor_classification_vector_tests.Po",
            "-rm -f test/$(DEPDIR)/test_rigor-rigor_sighash_vector_tests.Po",
        ],
    )
    if path.name in {"Makefile", "Makefile.in"}:
        text = clone_compile_rules(text)
    if text != orig:
        path.write_text(text, encoding="utf-8")
        print(f"patched {path}")
    else:
        print(f"unchanged {path}")


def main() -> None:
    for name in JSON_FILES:
        src = VEC / name
        dst = DATA / name
        shutil.copyfile(src, dst)
        write_json_h(src, DATA / f"{src.stem}.json.h")
        print(f"copied {name} + json.h")

    patch_file(SRC / "Makefile.test.include")
    patch_file(SRC / "Makefile")
    patch_file(SRC / "Makefile.in")


if __name__ == "__main__":
    main()
