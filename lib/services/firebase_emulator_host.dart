String? validateFirebaseEmulatorHost(String value) {
  final host = value.trim();
  if (host.isEmpty) {
    return "Enter this PC's private LAN IPv4 address.";
  }

  final parts = host.split('.');
  if (parts.length != 4) {
    return 'Enter a private LAN IPv4 address, such as 192.168.1.25.';
  }

  final octets = <int>[];
  for (final part in parts) {
    if (!RegExp(r'^(0|[1-9]\d{0,2})$').hasMatch(part)) {
      return 'Enter a private LAN IPv4 address, such as 192.168.1.25.';
    }
    final octet = int.parse(part);
    if (octet > 255) {
      return 'Enter a private LAN IPv4 address, such as 192.168.1.25.';
    }
    octets.add(octet);
  }

  final first = octets[0];
  final second = octets[1];
  final isPrivate =
      first == 10 ||
      (first == 172 && second >= 16 && second <= 31) ||
      (first == 192 && second == 168);
  if (!isPrivate) {
    return "Use this PC's private LAN IPv4 address, not localhost or a public address.";
  }

  return null;
}
