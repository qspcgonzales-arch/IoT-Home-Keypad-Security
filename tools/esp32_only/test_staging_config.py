import json
import unittest
from pathlib import Path

from check_staging_config import validate_configuration


ROOT = Path(__file__).resolve().parents[2]
ADMIN_UID = "staging-admin-test-uid"
DEVICE_UID = "staging-device-test-uid"
TEST_APP_ID = "1:637464771451:android:test-only-app-id"


def configuration_fixture():
    flutter = json.loads(
        (ROOT / "staging_firebase_options.example.json").read_text(
            encoding="utf-8"
        )
    )
    flutter.update(
        {
            "FIREBASE_API_KEY": "test-only-api-key",
            "FIREBASE_APP_ID": TEST_APP_ID,
            "FIREBASE_ADMIN_UID": ADMIN_UID,
        }
    )

    firmware = (
        ROOT / "hardware/esp32/esp32_only/src/secrets.h.example"
    ).read_text(encoding="utf-8")
    replacements = {
        'FIREBASE_API_KEY "REPLACE_WITH_STAGING_WEB_API_KEY"':
            'FIREBASE_API_KEY "test-only-api-key"',
        'FIREBASE_DEVICE_EMAIL "REPLACE_WITH_DEDICATED_DEVICE_EMAIL"':
            'FIREBASE_DEVICE_EMAIL "device@example.test"',
        'FIREBASE_DEVICE_PASSWORD "REPLACE_WITH_RANDOM_DEVICE_PASSWORD"':
            'FIREBASE_DEVICE_PASSWORD "test-only-password"',
        'FIREBASE_DEVICE_UID "66BkT43W8tPkWYL8jV4dgroMCbN2"':
            f'FIREBASE_DEVICE_UID "{DEVICE_UID}"',
        'FIREBASE_ADMIN_UID "fyTwhPR5RMek0Bdd40C08McNZmm2"':
            f'FIREBASE_ADMIN_UID "{ADMIN_UID}"',
        'WIFI_SSID "REPLACE_WITH_2_4_GHZ_STAGING_WIFI"':
            'WIFI_SSID "test-only-network"',
        'WIFI_PASSWORD "REPLACE_LOCALLY_NEVER_COMMIT"':
            'WIFI_PASSWORD "test-only-password"',
    }
    for old, new in replacements.items():
        firmware = firmware.replace(old, new)

    rules = (ROOT / "database.rules.staging.json").read_text(
        encoding="utf-8"
    )
    rules = rules.replace(
        "fyTwhPR5RMek0Bdd40C08McNZmm2", ADMIN_UID
    )
    rules = rules.replace(
        "66BkT43W8tPkWYL8jV4dgroMCbN2", DEVICE_UID
    )
    return flutter, firmware, rules


class StagingConfigurationTests(unittest.TestCase):
    def test_confirmed_project_values_are_in_templates(self):
        flutter = json.loads(
            (ROOT / "staging_firebase_options.example.json").read_text(
                encoding="utf-8"
            )
        )
        firmware = (
            ROOT / "hardware/esp32/esp32_only/src/secrets.h.example"
        ).read_text(encoding="utf-8")

        self.assertEqual(
            flutter["FIREBASE_PROJECT_ID"],
            "iothomekeypadsecurity-staging",
        )
        self.assertEqual(
            flutter["FIREBASE_DEVICE_ID"], "esp32-frontdoor-1"
        )
        self.assertIn(
            'FIREBASE_PROJECT_ID "iothomekeypadsecurity-staging"',
            firmware,
        )
        self.assertEqual(
            flutter["FIREBASE_ADMIN_UID"],
            "fyTwhPR5RMek0Bdd40C08McNZmm2",
        )
        self.assertIn(
            'FIREBASE_ADMIN_UID "fyTwhPR5RMek0Bdd40C08McNZmm2"',
            firmware,
        )
        self.assertIn(
            'FIREBASE_DEVICE_UID "66BkT43W8tPkWYL8jV4dgroMCbN2"',
            firmware,
        )
        rules = (ROOT / "database.rules.staging.json").read_text(
            encoding="utf-8"
        )
        self.assertIn("fyTwhPR5RMek0Bdd40C08McNZmm2", rules)
        self.assertIn("66BkT43W8tPkWYL8jV4dgroMCbN2", rules)

    def test_valid_local_configuration_matches_staging_rules(self):
        flutter, firmware, rules = configuration_fixture()
        self.assertEqual(
            validate_configuration(flutter, firmware, rules), []
        )

    def test_example_templates_include_confirmed_uids_but_require_secrets(self):
        flutter = json.loads(
            (ROOT / "staging_firebase_options.example.json").read_text(
                encoding="utf-8"
            )
        )
        firmware = (
            ROOT / "hardware/esp32/esp32_only/src/secrets.h.example"
        ).read_text(encoding="utf-8")
        rules = (ROOT / "database.rules.staging.json").read_text(
            encoding="utf-8"
        )

        problems = validate_configuration(flutter, firmware, rules)
        self.assertNotIn(
            "Staging administrator UID must come from Firebase Auth.",
            problems,
        )
        self.assertNotIn(
            "FIREBASE_DEVICE_UID must be configured from staging Firebase Auth.",
            problems,
        )
        self.assertIn("Flutter staging API key is not configured.", problems)
        self.assertIn(
            "Flutter Firebase Android App ID is incomplete or mismatched.",
            problems,
        )

    def test_mismatched_project_or_database_url_is_rejected(self):
        flutter, firmware, rules = configuration_fixture()
        flutter["FIREBASE_PROJECT_ID"] = "demo-smart-home"
        flutter["FIREBASE_DATABASE_URL"] = "https://different.firebaseio.com"

        problems = validate_configuration(flutter, firmware, rules)
        self.assertIn(
            "Flutter staging project ID does not match the confirmed project.",
            problems,
        )
        self.assertIn(
            "Flutter and ESP32 project IDs do not match.", problems
        )
        self.assertIn(
            "Flutter and ESP32 Realtime Database URLs do not match.", problems
        )

    def test_administrator_and_device_uids_must_remain_distinct(self):
        flutter, firmware, rules = configuration_fixture()
        firmware = firmware.replace(
            f'FIREBASE_DEVICE_UID "{DEVICE_UID}"',
            f'FIREBASE_DEVICE_UID "{ADMIN_UID}"',
        )

        problems = validate_configuration(flutter, firmware, rules)
        self.assertIn(
            "Administrator and device UIDs must be different.", problems
        )

    def test_database_host_must_match_the_staging_project(self):
        flutter, firmware, rules = configuration_fixture()
        wrong_url = (
            "https://other-project-default-rtdb."
            "asia-southeast1.firebasedatabase.app"
        )
        flutter["FIREBASE_DATABASE_URL"] = wrong_url
        firmware = firmware.replace(
            "https://iothomekeypadsecurity-staging-default-rtdb."
            "asia-southeast1.firebasedatabase.app",
            wrong_url,
        )

        problems = validate_configuration(flutter, firmware, rules)
        self.assertIn(
            "Staging database endpoint does not match its project ID.",
            problems,
        )

    def test_database_region_must_match_the_confirmed_endpoint(self):
        flutter, firmware, rules = configuration_fixture()
        wrong_url = (
            "https://iothomekeypadsecurity-staging-default-rtdb."
            "europe-west1.firebasedatabase.app"
        )
        flutter["FIREBASE_DATABASE_URL"] = wrong_url
        firmware = firmware.replace(
            "https://iothomekeypadsecurity-staging-default-rtdb."
            "asia-southeast1.firebasedatabase.app",
            wrong_url,
        )

        problems = validate_configuration(flutter, firmware, rules)
        self.assertIn(
            "Staging database endpoint does not match the confirmed URL.",
            problems,
        )


if __name__ == "__main__":
    unittest.main()
