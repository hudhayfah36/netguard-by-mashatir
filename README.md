# 🛡️ NetGuard by Mashatir

## Privacy-First DNS Safety for Families & Schools

![NetGuard Hero](images/netguard_hero.svg)

<div align="center">

**A local-first DNS safety appliance built for the ESP32-S3**

[Features](#-features) • [How It Works](#-how-it-works) • [Getting Started](#-getting-started) • [Buy Device](#-buy-pre-built-device) • [Documentation](#-documentation)

</div>

---

## 🌟 What is NetGuard?

**NetGuard by Mashatir** is a privacy-respecting, low-cost network controller that helps families, parents, schools, and small networks maintain understandable internet controls **without sending browsing history to the cloud** or paying for monthly subscriptions.

NetGuard becomes your network's DNS server, intelligently filtering requests before they reach the internet. It's transparent about what it can and cannot do, putting you in control without false promises.

### 💡 Perfect For:
- 👨‍👩‍👧‍👦 **Families** wanting screen-time boundaries
- 🏫 **Schools** needing simple content filtering
- 🏢 **Small offices** requiring basic web controls
- 🔒 **Privacy advocates** who refuse cloud-based monitoring

---

## ✨ Key Features

### 🎯 Core Capabilities

| Feature | Description |
|---------|-------------|
| **🏠 Local-First** | All configuration stays on-device. No cloud account required. |
| **🔒 Privacy-Safe** | No browsing history uploads. Ever. |
| **💰 No Subscription** | One-time setup, lifetime usage. |
| **👤 Per-Device Profiles** | Up to 16 individual device rules with custom schedules. |
| **⏰ Daily Access Windows** | Allow sites only during specific times (e.g., after homework). |
| **🚫 Global Blocklists** | Permanent website blocks for the entire network. |
| **⚠️ Bypass Alerts** | Notifications when VPN/proxy domains are detected (signals, not proof). |
| **🛡️ Secure Dashboard** | Salted password hashing, CSRF protection, RAM-only sessions. |

### 🎛️ What You Can Control

- ✅ **Permanent website blocks** (e.g., block `youtube.com` network-wide)
- ✅ **Time-based access windows** (e.g., allow educational sites 4 PM - 8 PM)
- ✅ **Per-device restrictions** (different rules for child's tablet vs. study computer)
- ✅ **Midnight-crossing schedules** (e.g., 10 PM - 6 AM overnight rules)
- ✅ **Subdomain matching** (blocking `youtube.com` also blocks `www.youtube.com`)

---

## 🔄 How It Works

![Architecture Diagram](images/architecture_diagram.svg)

```
┌─────────────────┐
│  Client Device  │ (Phone, Laptop, Tablet)
│  (DNS Request)  │
└────────┬────────┘
         │ Router DHCP gives client NetGuard's LAN IP
         ▼
┌─────────────────────────────────────────┐
│     NetGuard by Mashatir (ESP32-S3)     │
│  ┌───────────────────────────────────┐  │
│  │ • Global permanent block rules    │  │
│  │ • Device-profile rules            │  │
│  │ • Daily schedules                 │  │
│  │ • Possible-bypass indicators      │  │
│  └───────────────────────────────────┘  │
└────────┬────────────────────────────────┘
         │ Filtered DNS requests only
         ▼
┌─────────────────────┐
│ Cloudflare Family   │ (Upstream DNS Resolver)
│ DNS Resolver        │ (1.1.1.3)
└────────┬────────────┘
         ▼
┌─────────────────────┐
│     Internet        │
│   DNS Answer        │
└─────────────────────┘
```

### Policy Evaluation Order

When NetGuard receives a DNS query from a known profile:

1. **Global permanent block** → Always blocks the domain
2. **Profile permanent block** → Blocks for that specific device
3. **Global schedule** → Enforced time windows
4. **Profile schedule** → Additional per-device time windows
5. **Forward upstream** → Only if allowed by all applicable rules

> ⚠️ **Important:** Profile rules are *additive* — they can add restrictions but cannot weaken global restrictions.

---

## 🚀 Getting Started

### Option 1: Build Your Own (DIY)

#### Hardware Requirements
- **ESP32-S3** development board (with PSRAM recommended)
- USB cable for flashing
- Wi-Fi network with router access

#### Software Requirements
- ESP-IDF v5.x or later
- Python 3.x
- Git

#### Flash Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/netguard-mashatir.git
cd netguard-mashatir

# Build the project
idf.py build

# Flash to ESP32-S3 (replace /dev/ttyUSB0 with your device)
idf.py -p /dev/ttyUSB0 flash monitor
```

> ⚠️ **Note:** This project uses a custom partition table. After partition-table changes, always perform a **full flash** instead of just flashing the app.

#### Initial Setup

1. **Power on** your ESP32-S3 device
2. **Connect to Wi-Fi:** Look for `NetGuard-Mashatir Setup` access point
3. **Enter password:** `netguard@1234`
4. **Access dashboard:** Navigate to the device's IP address in your browser
5. **Configure admin password** and set up your network rules

### Option 2: Buy Pre-Built Device 🛒

<div align="center">

**Don't want to build it yourself?**

We offer ready-to-use NetGuard appliances with pre-configured firmware.

[**🛍️ Order Now**](mailto:contact@mashatir.com) | [**📧 Contact Us**](mailto:sales@mashatir.com)

</div>

**Pre-built devices include:**
- ✅ Fully assembled ESP32-S3 hardware
- ✅ Pre-flashed firmware
- ✅ Quick-start guide
- ✅ Email support for setup assistance
- ✅ 1-year hardware warranty

*Contact us for bulk pricing for schools and organizations.*

---

## 📋 Deployment Requirements

### Router Configuration (Critical for Full Functionality)

For **per-device profiles** to work correctly, your router must be configured properly:

1. **Assign static IP** to NetGuard via DHCP reservation
2. **Set router's DNS server** to NetGuard's LAN IP (not router's IP)
3. **Reserve IPv4 addresses** for each device needing a profile
4. **Test connectivity:** `nslookup example.com <netguard-lan-ip>`

> ⚠️ **Common Issue:** If your router acts as a DNS proxy, all requests appear to come from the router itself, breaking per-device identification. Check your router's documentation for "DNS forwarding" or "DNS proxy" settings.

### Understanding Limitations

DNS filtering **cannot** reliably stop all access by itself. The following can bypass DNS-only filtering:

| Bypass Method | Description | Mitigation |
|---------------|-------------|------------|
| **Manual DNS** | User sets custom DNS (8.8.8.8, etc.) | Block port 53 outbound on router |
| **DoH/DoT** | Encrypted DNS (HTTPS/TLS) | Block known DoH endpoints |
| **VPNs** | Tunnel all traffic externally | Requires firewall/DPI controls |
| **IPv6 DNS** | Uses IPv6 nameservers | Deploy IPv6 filtering too |
| **Direct IP** | Access via IP address (no DNS) | Limited practical use |

**NetGuard provides possible-bypass alerts**, not confirmed VPN detection. Reliable enforcement requires router/firewall policy alongside NetGuard.

---

## 🎛️ Dashboard Security

NetGuard takes security seriously:

- 🔐 **Salted iterative hashing** for passwords (never stored in plain text)
- 🍪 **RAM-only sessions** (logout/reboot invalidates all sessions)
- 🛡️ **CSRF protection** on all state-changing requests
- ⏱️ **12-hour sliding idle window** for authenticated sessions
- 🔒 **Cookie-based authentication** with secure flags

---

## 📊 What Makes NetGuard Different?

Many consumer routers offer basic website blocking. Here's why NetGuard adds value:

| Feature | Typical Router | NetGuard by Mashatir |
|---------|---------------|---------------------|
| Per-device rules | ❌ Rarely | ✅ Up to 16 profiles |
| Time-based schedules | ⚠️ Basic | ✅ Complex windows (midnight crossing) |
| Privacy | ❌ Often cloud-based | ✅ 100% local, no uploads |
| Cost | 💰 Subscription fees | 💵 One-time purchase |
| Transparency | ❌ Hidden lists | ✅ Honest about limitations |
| Hardware | 💸 Expensive proprietary | 💡 Affordable ESP32-S3 |
| Repairability | ❌ Sealed units | ✅ Open source, fixable |

---

## 🔮 Roadmap

### Coming Soon

- 📱 **Enhanced Dashboard**: Better profile editing, alert details, session management
- 🔍 **Device Discovery**: "Seen DNS clients" view for easy profile creation
- 🌐 **Router Integration**: Optional APIs for DHCP lease visibility
- 🔌 **DNS-over-TCP**: Support for truncated response retries
- 🧪 **Testing Suite**: Unit tests for schedules, domain matching, packet handling
- 🔐 **Production Security**: Secure boot, flash encryption guides

### What We Won't Do

- ❌ Cloud-based browsing history collection
- ❌ Hidden subscription features
- ❌ False "VPN detection" claims
- ❌ Closed-source list updates

---

## ⚙️ Technical Specifications

| Specification | Detail |
|--------------|--------|
| **Platform** | ESP32-S3 (with PSRAM recommended) |
| **Wi-Fi Hostname** | `netguard-mashatir` |
| **Setup AP** | `NetGuard-Mashatir Setup` (WPA2) |
| **Setup Password** | `netguard@1234` |
| **Max Profiles** | 16 devices |
| **Rules per Profile** | 16 blocked domains + 16 schedules |
| **Alert Storage** | 20 newest alerts per profile (RAM, cleared on reboot) |
| **Upstream DNS** | Cloudflare Family (1.1.1.3, 1.0.0.3) |
| **Default Timezone** | UTC+05:00 (configurable) |
| **Storage** | NVS (Non-Volatile Storage) for config |

---

## 🤝 Contributing

We welcome contributions! Areas we need help with:

- 🧪 Testing (especially multi-client scenarios)
- 📚 Documentation improvements
- 🌐 Translations for non-English speakers
- 💡 Feature suggestions aligned with our principles

Please read our contributing guidelines before submitting PRs.

---

## 📄 License

This project is licensed under the [MIT License](LICENSE) - see the LICENSE file for details.

**Commercial use:** You may use this firmware freely. If you sell pre-built devices, we appreciate attribution but don't require royalties. Consider contributing back improvements!

---

## 🙏 Acknowledgments

- **Cloudflare** for their free Family DNS resolvers (1.1.1.3)
- **Espressif** for the ESP32-S3 platform and ESP-IDF
- **The open-source community** for countless dependencies and inspiration

---

## 📞 Support & Contact

| Need | Contact |
|------|---------|
| **Technical Issues** | [Open a GitHub Issue](https://github.com/yourusername/netguard-mashatir/issues) |
| **Buy Device** | [Email Sales](mailto:sales@mashatir.com) |
| **Partnerships** | [Email Contact](mailto:contact@mashatir.com) |
| **General Questions** | [Discussions Tab](https://github.com/yourusername/netguard-mashatir/discussions) |

---

<div align="center">

### 🛡️ **NetGuard by Mashatir**

*Local Privacy. Clear Controls. Family Support. Practical Security.*

**Built with ❤️ for safer home networks**

[⬆️ Back to Top](#-netguard-by-mashatir)

</div>
