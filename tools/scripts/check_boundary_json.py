#!/usr/bin/env python3
import json


def build(n: int) -> str:
    payload = "a" * n
    return (
        '{"kernel_version":"0.4.0","adapter_version":"0.4.0",'
        '"manual_requirements":[],"instructions":[{"type":"test",'
        '"name":"long-val","metadata":{"payload":"'
        + payload
        + '"}}]}'
    )


for n in [10, 100, 4095, 4096, 4097]:
    s = build(n)
    try:
        json.loads(s)
        print(f"{n}: ok len={len(s)}")
    except json.JSONDecodeError as e:
        print(f"{n}: BAD {e}")
