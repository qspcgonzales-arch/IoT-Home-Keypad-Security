import json
import re
import sys
from pathlib import Path
from typing import Any
from urllib.parse import urlsplit


ROOT = Path(__file__).resolve().parents[2]
FLUTTER_CONFIG = ROOT / "staging_firebase_options.json"
FIRMWARE_CONFIG = ROOT / "hardware/esp32/esp32_only/src/secrets.h"
RULES_FILE = ROOT / "database.rules.staging.local.json"
STAGING_PROJECT_ID = "iothomekeypadsecurity-staging"
STAGING_DATABASE_URL = (
    "https://iothomekeypadsecurity-staging-default-rtdb."
    "asia-southeast1.firebasedatabase.app"
)
STAGING_PROJECT_NUMBER = "637464771451"
STAGING_DEVICE_ID = "esp32-frontdoor-1"
UID_PLACEHOLDERS = {"", "__STAGING_ADMIN_UID__", "__STAGING_DEVICE_UID__"}


def read_define(header: str, name: str) -> str | None:
    match = re.search(
        rf'^\s*#define\s+{re.escape(name)}\s+"([^"]*)"\s*$',
        header,
        flags=re.MULTILINE,
    )
    return match.group(1) if match else None


def validate_configuration(
    flutter: dict[str, Any], firmware_header: str, rules_text: str
) -> list[str]:
    problems: list[str] = []
    firmware = {
        name: read_define(firmware_header, name)
        for name in (
            "FIREBASE_PROJECT_ID",
            "FIREBASE_DATABASE_URL",
            "FIREBASE_API_KEY",
            "FIREBASE_DEVICE_EMAIL",
            "FIREBASE_DEVICE_PASSWORD",
            "FIREBASE_ADMIN_UID",
            "FIREBASE_DEVICE_UID",
            "FIREBASE_DEVICE_ID",
            "WIFI_SSID",
            "WIFI_PASSWORD",
        )
    }
    project_id = flutter.get("FIREBASE_PROJECT_ID")
    if not isinstance(project_id, str) or not project_id:
        problems.append("Flutter staging project ID is missing.")
    elif project_id != STAGING_PROJECT_ID:
        problems.append("Flutter staging project ID does not match the confirmed project.")
    if firmware["FIREBASE_PROJECT_ID"] != project_id:
        problems.append("Flutter and ESP32 project IDs do not match.")
    if firmware["FIREBASE_DATABASE_URL"] != flutter.get("FIREBASE_DATABASE_URL"):
        problems.append("Flutter and ESP32 Realtime Database URLs do not match.")
    database_url = firmware["FIREBASE_DATABASE_URL"] or ""
    database_host = urlsplit(database_url).hostname or ""
    if not (
        database_host.startswith(f"{project_id}-default-rtdb.")
        or database_host == f"{project_id}.firebaseio.com"
    ):
        problems.append("Staging database endpoint does not match its project ID.")
    if database_url.rstrip("/") != STAGING_DATABASE_URL:
        problems.append("Staging database endpoint does not match the confirmed URL.")
    if firmware["FIREBASE_ADMIN_UID"] != flutter.get("FIREBASE_ADMIN_UID"):
        problems.append("Flutter and ESP32 administrator UIDs do not match.")
    if firmware["FIREBASE_ADMIN_UID"] == firmware["FIREBASE_DEVICE_UID"]:
        problems.append("Administrator and device UIDs must be different.")
    for name in ("FIREBASE_ADMIN_UID", "FIREBASE_DEVICE_UID"):
        if not firmware[name] or firmware[name] in UID_PLACEHOLDERS:
            problems.append(f"{name} must be configured from staging Firebase Auth.")
    if firmware["FIREBASE_DEVICE_ID"] != flutter.get(
        "FIREBASE_DEVICE_ID"
    ):
        problems.append("Flutter and ESP32 device IDs do not match.")
    if firmware["FIREBASE_DEVICE_ID"] != STAGING_DEVICE_ID:
        problems.append("Device ID does not match the confirmed staging device.")
    if flutter.get("FIREBASE_ADMIN_UID") in UID_PLACEHOLDERS:
        problems.append("Staging administrator UID must come from Firebase Auth.")
    if flutter.get("FIREBASE_MESSAGING_SENDER_ID") != STAGING_PROJECT_NUMBER:
        problems.append("Flutter sender ID does not match the confirmed project number.")
    flutter_api_key = flutter.get("FIREBASE_API_KEY")
    if not isinstance(flutter_api_key, str) or not flutter_api_key or "REPLACE" in flutter_api_key:
        problems.append("Flutter staging API key is not configured.")
    app_id = flutter.get("FIREBASE_APP_ID")
    if not isinstance(app_id, str) or not re.fullmatch(
        rf"1:{STAGING_PROJECT_NUMBER}:android:[A-Za-z0-9_-]+", app_id
    ):
        problems.append("Flutter Firebase Android App ID is incomplete or mismatched.")
    for name in (
        "FIREBASE_API_KEY",
        "FIREBASE_DEVICE_EMAIL",
        "FIREBASE_DEVICE_PASSWORD",
        "WIFI_SSID",
        "WIFI_PASSWORD",
    ):
        value = firmware[name] or ""
        if not value or "REPLACE" in value:
            problems.append(f"Local ESP32 {name} is not configured.")
    for name in (
        "FIREBASE_DEVICE_UID",
        "FIREBASE_ADMIN_UID",
        "FIREBASE_DEVICE_ID",
    ):
        if not firmware[name] or firmware[name] not in rules_text:
            problems.append(f"Staging rules do not contain the configured {name}.")
    if "__STAGING_ADMIN_UID__" in rules_text or "__STAGING_DEVICE_UID__" in rules_text:
        problems.append("Local staging rules still contain UID placeholders.")
    if not database_url.startswith("https://"):
        problems.append("ESP32 database URL must be an explicit HTTPS endpoint.")
    return problems


def main() -> int:
    missing = [
        path.name
        for path in (FLUTTER_CONFIG, FIRMWARE_CONFIG, RULES_FILE)
        if not path.is_file()
    ]
    if missing:
        print(
            "Staging configuration is incomplete; missing local file(s): "
            + ", ".join(missing)
            + "."
        )
        print("Create them from the checked-in example templates first.")
        return 2
    try:
        flutter = json.loads(FLUTTER_CONFIG.read_text(encoding="utf-8"))
        firmware = FIRMWARE_CONFIG.read_text(encoding="utf-8")
        rules = RULES_FILE.read_text(encoding="utf-8")
    except (OSError, json.JSONDecodeError) as error:
        print(f"Could not read staging configuration: {error.__class__.__name__}.")
        return 2
    problems = validate_configuration(flutter, firmware, rules)
    if problems:
        for problem in problems:
            print(problem)
        return 1
    print("Flutter, ESP32 and staging rules use matching isolated identifiers.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
