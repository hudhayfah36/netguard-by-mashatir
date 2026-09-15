# Changelog

## [0.1.0] - 2026-09-16

### Added

- Local dashboard authentication, CSRF protection, and RAM-only sessions.
- WPA2-protected setup network and Wi-Fi provisioning.
- Global DNS blocks, NTP-aware daily schedules, and Cloudflare Family upstream DNS.
- Per-device IPv4 profiles, counters, and possible-bypass alerts.
- Release, flashing, contributor, and source-availability documentation.

### Known limitations

- DNS-over-TCP is not yet supported.
- Per-device policy needs direct client DNS rather than a router DNS proxy.
- Possible-bypass signals are not confirmed VPN detection or blocking.
