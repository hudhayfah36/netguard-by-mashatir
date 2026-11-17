# NetGuard by Mashatir

> A private, offline-first DNS security appliance for every device on your home network.

![NetGuard by Mashatir](images/netguard_hero.svg)

NetGuard is a small USB-C-powered ESP32-S3 device that sits on a local network and acts as its DNS server. It intercepts DNS queries from phones, laptops, TVs, consoles, and IoT devices, blocking known ad, tracker, and malware domains before a connection is made. No software is installed on client devices.

NetGuard runs offline: DNS resolution happens locally and no browsing data is sent to a cloud service. The companion mobile app provides the user interface; the device exposes a local HTTP API for setup and configuration and does **not** serve a web frontend.

**Private by design · No accounts · No cloud dashboard · No subscription**

## Features

- Local DNS filtering for the whole network.
- Blocklist mode for known advertising, tracking, and malware domains.
- Allowlist mode for tightly controlled networks, including blocking OS-level telemetry unless explicitly allowed.
- Per-person rules for blocking or allowing domains.
- Time-based rules, such as blocking selected sites during study or sleep hours.
- Bluetooth-assisted initial setup, followed by local-network communication through the device’s local IP address.
- Built-in Wi-Fi and Bluetooth, three physical buttons (reset, boot, and Bluetooth pairing), and USB-C power.
- Open-source firmware, PCB schematics, and companion mobile app.

## How it works

![NetGuard DNS filtering architecture](images/architecture_diagram.svg)

1. During initial setup, the mobile app pairs with NetGuard over Bluetooth and helps configure Wi-Fi.
2. The app discovers or receives the device’s local IP address.
3. The router gives that address to network clients as their DNS server.
4. NetGuard evaluates each DNS query against the selected mode, domain rules, and schedules.
5. Blocked domains receive `NXDOMAIN`; permitted domains are resolved locally or through the configured upstream resolver.
6. The mobile app sends authenticated HTTP requests to the device’s local API for configuration and status.

The API is intended to be reachable only from the trusted local network. NetGuard does not expose a cloud service or require an account.

## Hardware

The PCB design is complete and the 3D enclosure design is in progress. The repository includes the [PCB render](images/netguard_pcb_render.png) and the [KiCad PCB design](hardware/pcb/netguard_compact_r2.kicad_pcb).

![NetGuard PCB render](images/netguard_pcb_render.png)

## Firmware and mobile app

The ESP32 firmware provides the DNS engine, Wi-Fi/Bluetooth setup support, local configuration storage, rule enforcement, and HTTP API endpoints. The companion mobile app owns the frontend and communicates with the device over Bluetooth during setup and over the local IP address afterward.

The API includes setup, authentication, status, network configuration, blocklist/allowlist, schedules, and per-person profile operations. The device should never be exposed directly to the public Internet.

## Network limits

DNS filtering is not a complete firewall. Manually configured DNS, DNS-over-HTTPS, DNS-over-TLS, IPv6 DNS, VPNs, proxies, Private Relay, and direct IP connections can bypass or reduce visibility unless the router also enforces corresponding network rules.

## Development

This is an ESP-IDF project targeting the ESP32-S3. See [docs/FLASHING.md](docs/FLASHING.md) for firmware flashing instructions and [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines.

## Crowdfunding status

NetGuard is being prepared for crowdfunding. The firmware and PCB schematics are complete; enclosure development and the companion mobile app are continuing toward a production-ready device.

## Contributing

Contributions to firmware, hardware documentation, the mobile app, testing, and translations are welcome. Start with [CONTRIBUTING.md](CONTRIBUTING.md).

## License

NetGuard by Mashatir is open source under the [MIT License](LICENSE).

<div align="center"><strong>Local privacy · Open hardware · No subscription</strong></div>
