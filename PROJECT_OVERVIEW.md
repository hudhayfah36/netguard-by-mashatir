# NetGuard by Mashatir

## Project purpose

NetGuard by Mashatir is a local-first DNS safety appliance built for the ESP32-S3. It is designed for families, parents, schools, and small networks that want understandable internet controls without sending browsing history to a cloud service or paying for a subscription.

It works by becoming the DNS server given to devices by the router. When a phone, laptop, or tablet asks for the address of a website, NetGuard evaluates the domain against the configured rules before forwarding permitted requests to a family-safe upstream DNS resolver.

The project is deliberately transparent about the boundary of DNS filtering: it can control and observe DNS requests that reach it, but it cannot by itself control every type of internet traffic. A user can bypass DNS-only filtering through manually configured DNS, encrypted DNS, a VPN, IPv6 DNS, or direct IP connections unless the router also enforces network rules.

## What we are building

NetGuard is being developed as a low-cost, privacy-respecting family network controller with these core ideas:

- A local dashboard for configuring the appliance.
- Permanent website blocks for the entire network.
- Daily access windows for sites, such as allowing a learning or entertainment site only between selected local times.
- Per-device family profiles using a friendly name and a router-reserved IPv4 address.
- Possible-bypass alerts for known VPN, proxy, and encrypted-DNS provider domains. These are signals, not proof that a VPN is in use.
- No cloud account, no subscription, and no upload of browsing history.

## How it works

```text
Client device
    |
    | DNS request (router DHCP gives the client NetGuard's LAN IP)
    v
NetGuard by Mashatir
    |-- Global permanent block rules
    |-- Matching device-profile rules
    |-- Global and device daily schedules
    |-- Possible-bypass indicator check
    v
Filtered upstream DNS (Cloudflare Family)
    v
Internet DNS answer
```

For per-device rules to work, clients must send DNS directly to NetGuard. If a router acts as a DNS proxy, every request can appear to originate from the router, so NetGuard cannot tell which individual client made it.

## What has been achieved

### Device identity and setup

- The product and dashboard are branded **NetGuard by Mashatir**.
- The Wi-Fi hostname is `netguard-mashatir`.
- The fallback setup access point is named `NetGuard-Mashatir Setup`.
- The setup access point uses WPA2 protection. Its current configured password is `netguard@1234`.
- The dashboard allows Wi-Fi credentials to be saved and reports whether the device is on the home network or has fallen back to setup mode.

### Dashboard security

- An administrator password setup and login flow is included.
- Passwords are stored as a salted, iterative hash rather than plain text.
- Authenticated sessions use a cookie and CSRF protection for state-changing requests.
- Sessions are stored only in RAM: logout, reboot, or a new login invalidates the previous session.
- The intended session behaviour is a 12-hour sliding idle window, so normal authenticated activity refreshes the session rather than forcing frequent logins.

### Global DNS policies

- A persistent global blocklist supports normalized domain names and subdomain matching. For example, blocking `youtube.com` also blocks `www.youtube.com`.
- A blocked domain returns a valid DNS `NXDOMAIN` response instead of being forwarded upstream.
- Global daily site schedules support one allowed local-time window each day.
- Schedules that cross midnight are handled, for example `22:00` to `06:00`.
- Schedule-controlled sites fail closed while the clock is not synchronized, preventing access before valid time is available.
- Local time uses NTP and a configurable UTC offset; the default is UTC+05:00.
- Cloudflare Family resolvers are used as the primary and secondary upstream resolvers, avoiding an unfiltered fallback.

### Per-device profiles

- Up to 16 profiles can be stored.
- Each profile has a friendly name and one reserved IPv4 address.
- Each profile can have up to 16 additional blocked domains and 16 additional daily schedules.
- The DNS engine identifies a profile from the requesting client IPv4 address.
- Profile rules are additive: they can add restrictions but cannot weaken global restrictions.
- Per-profile total DNS-query and blocked-query counters are kept in RAM and reset on reboot.
- Profile configuration persists in NVS; activity counters and alerts do not persist as browsing history.

### Possible-bypass indicators

- NetGuard includes a small built-in suffix list for selected VPN, proxy, and encrypted-DNS provider domains.
- A matching DNS lookup records a **possible bypass alert** for the relevant profile without changing the DNS response by itself.
- Alerts retain a category, signal type, and timestamp (or uptime before time sync).
- Each profile retains only the 20 newest alerts in RAM and clears them on reboot.

This is intentionally not called VPN detection. A lookup for a provider domain can have harmless explanations, and many VPNs can operate without making a recognizable DNS query.

### Reliability foundations

- Inputs such as domains, profile names, IPv4 addresses, and schedule times are validated before storage.
- DNS domain matching includes parent-domain/subdomain checks.
- Configuration is stored in NVS, with error paths exposed instead of falsely claiming a setting was saved.
- Profile lookup structures prefer PSRAM when available to preserve internal memory.

## Policy order

When NetGuard receives a DNS query from a known profile, it evaluates rules in this order:

1. A global permanent block always blocks the domain.
2. A matching profile permanent block blocks the domain.
3. A global schedule is enforced.
4. A matching profile schedule is also enforced.
5. Only a domain allowed by every applicable rule is forwarded upstream.

Therefore, a profile cannot use a schedule to override a global permanent block or to widen a global schedule.

## What NetGuard contributes

Many consumer routers already offer a basic website blocklist. NetGuard aims to add value by being more focused, understandable, and privacy-safe:

- **Per-device family rules:** different limits for a child's tablet, a study computer, or a shared device.
- **Daily windows instead of only permanent blocks:** for example, allow a site after homework time while keeping the rest of the day restricted.
- **Local ownership:** configuration and DNS decisions stay on the appliance; there is no required cloud account.
- **Transparent alerts:** possible-bypass signals are labelled carefully instead of pretending that they prove VPN use.
- **Affordable and repairable hardware:** ESP32-S3 hardware makes the project accessible for homes and small community deployments.
- **Honest boundaries:** DNS controls complement conversations, supervision, and router security; they do not replace them.

## Deployment requirements

### Router configuration

1. Give NetGuard a stable LAN address, ideally through a router DHCP reservation.
2. Configure the router's LAN DHCP DNS server option to give client devices NetGuard's LAN address directly.
3. Reserve a stable IPv4 address for each device that needs its own profile.
4. Test from a client with `nslookup example.com <netguard-lan-ip>`.
5. Check the client itself is actually using NetGuard for DNS, not the router or a public resolver.

If the router only has a field for its own upstream DNS, it may continue to proxy client DNS. In that configuration global filtering may work, but per-device identification and alerts will not be reliable.

### Important bypass limitations

DNS filtering cannot reliably stop all access by itself. The following can evade or reduce visibility unless blocked by the router/firewall:

- Manually configured public DNS servers.
- DNS-over-HTTPS (DoH), DNS-over-TLS (DoT), and browser secure-DNS features.
- VPNs, proxy tunnels, Apple Private Relay, and similar encrypted tunnels.
- IPv6 DNS when NetGuard is only deployed for IPv4.
- Direct IP connections that do not require a DNS lookup.

Reliable VPN blocking requires router/firewall policy, traffic controls, or DPI-capable network equipment. NetGuard v1 offers privacy-safe **possible bypass alerts**, not confirmed VPN detection or automatic VPN blocking.

## Known current limitations

- NetGuard measures DNS requests, not actual minutes spent viewing a website. A daily schedule is an access window, not a precise screen-time meter.
- Device profiles are based on a configured IPv4 address. DNS alone cannot reliably learn a device's friendly name, MAC address, or every device connected to the router.
- Passive observation can reveal devices that send DNS directly to NetGuard, but it cannot enumerate all router clients. Full network inventory needs router API access, DHCP/ARP visibility, or NetGuard operating as the network gateway.
- DNS-over-TCP is not yet implemented. The current DNS service is UDP-focused, with bounded UDP response handling.
- The dashboard has core profile controls, but profile-rule editing, detailed alert presentation, and discovery views still need refinement.
- Production security features such as secure boot, flash encryption, and NVS encryption need a deliberate one-time provisioning process before production use.

## Future updates roadmap

### Dashboard and usability

- Improve the profile screen to display, edit, remove, and reorder every per-device block and schedule rule.
- Show detailed possible-bypass alerts with plain-language explanations, timestamp/uptime, profile, provider category, and safe handling guidance.
- Improve in-page session recovery: preserve unsaved fields, show an in-page login overlay after expiry, and restore dashboard data after successful login.
- Show a visible session-expiry countdown and warning.
- Add clearer network diagnostics for router DNS proxying, time-sync failures, profile capacity, and Wi-Fi connection errors.

### Device discovery and router integration

- Add a passive “seen DNS clients” view that lists source IPv4 addresses observed sending requests directly to NetGuard, with last-seen time and request count.
- Allow a seen address to be converted into a device profile with one action.
- Explore optional router integrations where a router exposes DHCP lease or client-list APIs.
- Document gateway-mode options for deployments that need complete client visibility.

Passive DNS discovery will be useful, but it will never be a complete list of every device on the router unless NetGuard receives their DNS traffic directly.

### DNS engine reliability

- Add DNS-over-TCP support so clients can retry truncated DNS responses correctly.
- Expand transaction-safe upstream forwarding to handle several concurrent queries or tightly bounded queues without one slow upstream request delaying unrelated clients.
- Continue robust malformed-packet, EDNS, timeout, source-address, and DNS-ID validation testing.
- Add rate-limited debug telemetry that protects privacy and DNS throughput.

### Enforcement and security

- Add router-specific deployment guides for blocking outbound port 53, port 853, common DoH endpoints, and unauthorized DNS where router capabilities permit.
- Keep bypass indicator lists firmware-controlled and reviewed; do not add hidden cloud list updates.
- Provide a deliberate production provisioning guide for secure boot, flash encryption, and NVS encryption.
- Add firmware update and configuration-backup guidance that does not expose administrator passwords or browsing events.

### Testing and quality

- Unit test domain normalization, subdomain matching, malformed DNS packets, midnight schedules, time boundaries, and unsynchronized-clock behaviour.
- Test duplicate profile names/IP addresses, storage capacity, NVS failure rollback, and reboot persistence.
- Simulate multiple client IPs to verify profile precedence, counters, and alert isolation.
- Load test slow or unreachable upstream resolvers under concurrent DNS traffic.
- Validate deployment against routers that provide direct DHCP DNS and against routers that proxy DNS, documenting the different behaviour.

## Development and flash note

The project uses a custom partition table to provide a larger NVS area for persistent configuration. After partition-table changes, use a full ESP-IDF flash workflow so the board receives the application at the correct offset:

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Replace `/dev/ttyUSB0` with the ESP32-S3 serial device shown by your computer. A full flash is important when moving from an older build that used a different partition layout.

## Project principles

NetGuard by Mashatir is guided by four commitments:

1. **Local privacy by default:** no browsing-history upload and no required account.
2. **Clear controls:** understandable rules and honest explanation of what each rule can and cannot do.
3. **Family support, not surveillance:** privacy-safe metrics should help conversations and routines, not create hidden monitoring.
4. **Practical security:** stronger protection is achieved by combining NetGuard with correct router DNS deployment and router firewall rules.
