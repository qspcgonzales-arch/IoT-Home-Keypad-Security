import hashlib
import secrets
import unittest

from provision_pin import render_config, valid_pin


class ProvisionPinTests(unittest.TestCase):
    def test_accepts_only_four_ascii_digits(self):
        local_test_pin = f"{secrets.randbelow(10_000):04d}"
        self.assertTrue(valid_pin(local_test_pin))
        self.assertFalse(valid_pin("123"))
        self.assertFalse(valid_pin("１２３４"))
        self.assertFalse(valid_pin("12a4"))

    def test_config_contains_salted_hash_without_plaintext_pin(self):
        pin = f"{secrets.randbelow(10_000):04d}"
        salt = "ab" * 16
        config = render_config(pin, salt)
        expected = hashlib.sha256(f"{salt}:{pin}".encode("ascii")).hexdigest()

        self.assertIn(f'"{expected}"', config)
        self.assertIn(f'"{salt}"', config)
        self.assertNotIn(pin, config)

    def test_rejects_invalid_input(self):
        with self.assertRaises(ValueError):
            render_config("123", "ab" * 16)
        with self.assertRaises(ValueError):
            render_config("1234", "not-hex")
        with self.assertRaises(ValueError):
            render_config("1234", "ab" * 25)


if __name__ == "__main__":
    unittest.main()
