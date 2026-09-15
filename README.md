# NetGuard by Mashatir

NetGuard by Mashatir is a local-first ESP32-S3 family DNS safety appliance. It provides permanent blocks, daily access windows, and per-device profiles without a cloud account, subscription, or browsing-history upload. A daily window is DNS policy, not a measurement of real screen time: modern sites cache DNS and keep network connections open.

## Why NetGuard

- Family device profiles: give a router-reserved IPv4 address a friendly name and apply additive device-specific rules.
- Privacy by design: counters and the last 20 possible-bypass alerts stay only in RAM and reset after reboot; queried-domain history is not retained.
- Transparent controls: global permanent blocks always win; device schedules cannot weaken a global restriction.
- Responsible use: NetGuard supports conversations and healthy boundaries; it does not replace supervision, router security, or informed consent.

## Deploy safely

1. Flash the firmware and join `NetGuard-Mashatir Setup` using the WPA2 password `netguard@1234`.
2. Open `http://192.168.4.1`, create an administrator password, and set Wi-Fi details. The device hostname is `netguard-mashatir`.
3. Reserve NetGuard's LAN IP in the router, then set the router **LAN DHCP DNS server** to that stable IP. For per-device profiles, clients must query NetGuard directly; a router DNS proxy makes every request appear to come from the router. Block direct outbound DNS (UDP/TCP 53 and TCP 853) and DNS-over-HTTPS at the router when it supports those rules.
4. Add a schedule such as `youtube.com`, 16:00–17:00. It applies to the domain and its subdomains every day. Scheduled domains are blocked until NTP time sync succeeds.

## Security notes

- Management uses a password-derived salted hash, HTTP-only session cookie, and CSRF token. This is LAN HTTP, not TLS; use a trusted WPA2/WPA3 LAN and never expose the dashboard to the Internet.
- For a production device, enable secure boot, flash encryption, and NVS encryption in ESP-IDF before provisioning. These are one-way device-security operations and must be tested on a spare board first. They cannot be safely enabled by a source-code default alone.
- DNS filtering cannot block traffic addressed directly by IP, VPNs, or all encrypted-DNS implementations.
- Possible VPN/proxy/DoH alerts mean a device looked up a built-in known provider domain. They are not confirmation of VPN use and cannot see direct-IP setup, HTTPS/QUIC tunnels, IPv6 DNS, Private Relay, or traffic after a tunnel connects.

## Current DNS compatibility

UDP DNS requests up to 1232 bytes (EDNS-sized) are accepted and upstream answers are source/transaction-validated. TCP DNS is not yet implemented; clients that require TCP DNS will need a router-side resolver in front of NetGuard or a future TCP-proxy extension.
