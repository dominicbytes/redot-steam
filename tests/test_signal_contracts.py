#!/usr/bin/env python3
"""Check literal signal emissions against their registered public contracts."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/', re.S)


def without_comments(source):
    return TOKEN.sub(lambda m: " " if m[0].startswith(("//", "/*")) else m[0], source)


def arguments(source, opening):
    """Split a call while respecting strings and nested calls/containers."""
    parts, stack = [], [")"]
    start = index = opening + 1
    pairs = {"(": ")", "[": "]", "{": "}"}
    while stack:
        char = source[index]
        if char in ('"', "'"):
            literal = TOKEN.match(source, index)
            assert literal, "Invalid string literal"
            index = literal.end()
            continue
        if char in pairs:
            stack.append(pairs[char])
        elif char == stack[-1]:
            stack.pop()
            if not stack:
                part = source[start:index].strip()
                if part:
                    parts.append(part)
                return parts
        elif char == "," and len(stack) == 1:
            parts.append(source[start:index].strip())
            start = index + 1
        index += 1
    raise AssertionError("Unterminated call")


def contracts(source):
    source = without_comments(source)
    registered = {}
    for match in re.finditer(r'\bADD_SIGNAL\s*\(\s*MethodInfo\s*\(', source):
        args = arguments(source, match.end() - 1)
        name = args[0].strip('"')
        assert name not in registered, f"Duplicate signal: {name}"
        registered[name] = [re.fullmatch(r'PropertyInfo\(Variant::(\w+),\s*"(\w+)"\)', arg).groups() for arg in args[1:]]
    emitted = 0
    for match in re.finditer(r'\bemit_signal\s*\(', source):
        args = arguments(source, match.end() - 1)
        assert re.fullmatch(r'"\w+"', args[0]), "Review nonliteral signal name"
        name = args[0].strip('"')
        assert name in registered, f"Unregistered signal: {name}"
        assert len(args) - 1 == len(registered[name]), f"Signal argument count mismatch: {name}"
        emitted += 1
    return registered, emitted


def main():
    source = (ROOT / "godotsteam/godotsteam.cpp").read_text(encoding="utf-8")
    registered, emitted = contracts(source)
    assert emitted >= 175, "Signal scan unexpectedly incomplete"
    docs = ET.parse(ROOT / "godotsteam/doc_classes/Steam.xml")
    # These callbacks previously disagreed in argument type, order, or arity.
    names = ("enumerate_following_list", "item_installed", "get_app_dependencies_result",
             "connected_friend_chat_message", "file_details_result",
             "remote_play_session_avatar_loaded", "html_browser_restarted")
    types = {"INT": "int", "BOOL": "bool", "STRING": "String",
             "PACKED_BYTE_ARRAY": "PackedByteArray", "PACKED_INT32_ARRAY": "PackedInt32Array",
             "PACKED_INT64_ARRAY": "PackedInt64Array"}
    for name in names:
        signal = docs.find(f'./signals/signal[@name="{name}"]')
        assert signal is not None, f"Undocumented signal: {name}"
        actual = [(p.attrib["type"], p.attrib["name"]) for p in signal.findall("param")]
        expected = [(types[t], n) for t, n in registered[name]]
        assert actual == expected, f"XML contract mismatch: {name}"
    # Prove the scanner ignores comments and handles commas in nested arguments.
    probe = 'ADD_SIGNAL(MethodInfo("ok", PropertyInfo(Variant::INT, "value")));'
    contracts(probe + '/* emit_signal("missing"); */ emit_signal("ok", f(1, 2));')
    for broken in ('emit_signal("missing");', 'emit_signal("ok");'):
        try:
            contracts(probe + broken)
        except AssertionError:
            pass
        else:
            raise AssertionError("Signal checker accepted an invalid emission")
    print(f"PASS: {emitted} signal emissions match {len(registered)} registrations")


if __name__ == "__main__":
    main()
