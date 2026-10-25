import argparse
import getpass
import hashlib
import secrets
from pathlib import Path


def valid_pin(pin: str) -> bool:
    return len(pin) == 4 and all("0" <= character <= "9" for character in pin)


def render_config(pin: str, salt: str) -> str:
    if not valid_pin(pin):
        raise ValueError("PIN must contain exactly four decimal digits.")
    if (
        not salt
        or len(salt) > 48
        or any(character not in "0123456789abcdef" for character in salt)
    ):
        raise ValueError("Salt must be non-empty lowercase hexadecimal.")
    digest = hashlib.sha256(f"{salt}:{pin}".encode("ascii")).hexdigest()
    return (
        "#pragma once\n\n"
        f'#define ESP32_PIN_HASH_HEX "{digest}"\n'
        f'#define ESP32_PIN_SALT "{salt}"\n'
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Create the ignored local PIN verifier header for the ESP32-only build."
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("hardware/esp32/esp32_only/src/pin_config.h"),
    )
    args = parser.parse_args()

    pin = getpass.getpass("Choose a 4-digit local test PIN: ")
    confirmation = getpass.getpass("Re-enter the local test PIN: ")
    if pin != confirmation:
        raise SystemExit("PIN entries did not match; no file was written.")

    salt = secrets.token_hex(16)
    content = render_config(pin, salt)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(content, encoding="ascii")
    print(f"Local PIN verifier written to {args.output}; PIN was not displayed.")


if __name__ == "__main__":
    main()
