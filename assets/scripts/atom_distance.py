#!/usr/bin/env python3
"""Distance between two atoms -- a minimal ChemLab node script.

ChemLab node scripts speak JSON over stdio (protocol v0):

  * `script.py --describe` must print a JSON spec of the node's pins:
        {"name": ..., "inputs": [{"name", "type"}, ...], "outputs": [...]}
    Types: float, int, text, floatvec, positions (N x [x,y,z]), labels, any.
  * A normal run receives {"inputs": {...}} on stdin and must print
    {"outputs": {...}} (or {"error": "message"}) on stdout.

Anything goes in between -- numpy, ASE, another interpreter entirely; ChemLab
only sees the JSON on stdout.
"""
import json
import math
import sys

DESCRIBE = {
    "name": "Atom distance",
    "inputs": [
        {"name": "positions", "type": "positions"},
        {"name": "i", "type": "int"},
        {"name": "j", "type": "int"},
    ],
    "outputs": [
        {"name": "distance", "type": "float"},
    ],
}


def main():
    if "--describe" in sys.argv:
        json.dump(DESCRIBE, sys.stdout)
        return

    req = json.load(sys.stdin)["inputs"]
    pos, i, j = req["positions"], req["i"], req["j"]
    if not (0 <= i < len(pos) and 0 <= j < len(pos)):
        json.dump({"error": f"atom index out of range (0..{len(pos) - 1})"}, sys.stdout)
        return
    d = math.dist(pos[i], pos[j])
    json.dump({"outputs": {"distance": d}}, sys.stdout)


if __name__ == "__main__":
    main()
