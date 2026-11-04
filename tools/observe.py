"""Dump what rmw_zenoh_cpp actually puts on the wire.

Run it against a live zenohd to read the four conventions the firmware has to
reproduce: the liveliness tokens, the data key expressions, the 33-byte
attachment and the CDR payload.

    python3 observe.py                  # tokens, then 20s of traffic
    python3 observe.py --seconds 60
    python3 observe.py --key '0/pico/**'

Needs the Zenoh Python client, which is unrelated to ROS:

    python3 -m venv .venv && .venv/bin/pip install eclipse-zenoh
"""

import argparse
import binascii
import time

import zenoh

GID_SIZE = 16


def decode_attachment(raw: bytes) -> str:
    """Undo the zenoh-ext serialisation used by rmw_zenoh_cpp."""
    if len(raw) != 8 + 8 + 1 + GID_SIZE:
        return f"unexpected length {len(raw)}"

    sequence_number = int.from_bytes(raw[0:8], "little")
    timestamp_ns = int.from_bytes(raw[8:16], "little")
    gid_len = raw[16]
    gid = raw[17 : 17 + GID_SIZE]

    return (
        f"seq={sequence_number} "
        f"stamp={timestamp_ns}ns ({timestamp_ns / 1e9:.3f}s) "
        f"gid[{gid_len}]={gid.hex()}"
    )


def decode_payload(raw: bytes) -> str:
    if len(raw) < 4:
        return "shorter than a CDR header"

    header = raw[:4]
    endianness = "little" if header[1] & 0x01 else "big"
    body = raw[4:]

    return (
        f"encapsulation={header.hex()} ({endianness}-endian) "
        f"body={binascii.hexlify(body[:48]).decode()}"
        f"{'...' if len(body) > 48 else ''}"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", default="tcp/localhost:7447")
    parser.add_argument("--key", default="**", help="data key expression to watch")
    parser.add_argument("--seconds", type=int, default=20)
    args = parser.parse_args()

    conf = zenoh.Config()
    conf.insert_json5("mode", '"client"')
    conf.insert_json5("connect/endpoints", f'["{args.endpoint}"]')

    with zenoh.open(conf) as session:
        print("=== liveliness tokens under @ros2_lv/** ===")
        tokens = []
        liveliness_sub = session.liveliness().declare_subscriber(
            "@ros2_lv/**",
            lambda sample: tokens.append(str(sample.key_expr)),
            history=True,
        )
        time.sleep(2.0)
        liveliness_sub.undeclare()
        for token in sorted(tokens):
            print(f"  {token}")
        if not tokens:
            print("  (none: is a ROS node running against rmw_zenoh_cpp?)")

        print(f"\n=== data on '{args.key}' for {args.seconds}s ===")

        def on_data(sample):
            attachment = sample.attachment
            raw = bytes(attachment) if attachment is not None else b""
            print(f"\n[{sample.key_expr}]")
            print(f"  attachment: {decode_attachment(raw)}")
            print(f"  payload:    {decode_payload(bytes(sample.payload))}")

        data_sub = session.declare_subscriber(args.key, on_data)
        time.sleep(args.seconds)
        data_sub.undeclare()


if __name__ == "__main__":
    main()
