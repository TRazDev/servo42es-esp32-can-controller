# Network

- The ESP32 joins the home WiFi (2.4 GHz, Ubiquiti UniFi access points). It doesn't create its own network (D-008).
- Address: **http://servo-control.local**. Set with `WiFi.setHostname("servo-control")` and `MDNS.begin("servo-control")`. One ESP32 will drive all 6 joints on one CAN bus, so one hostname is enough.
- Fallback: the IP address is printed on the serial monitor at boot. Optionally, reserve a fixed IP for the ESP32 in UniFi (client → Settings → Fixed IP).
- No login in v1. Anyone on the home WiFi can open the page.
- WiFi credentials go in `firmware/servo_controller/secrets.h` (copied from `secrets.h.example`). It's gitignored and never committed. The user types them in themselves.
- The SSID must be the same network (and VLAN) the laptop or phone uses, or `.local` won't resolve.

## VERIFIED (2026-09-11)
- The ESP32 joins the home 2.4 GHz network and gets 192.168.1.139 by DHCP. `servo-control.local` resolves on the Mac, so mDNS works through the UniFi setup with no changes.
- **IPv6 must be enabled on the ESP32** (`WiFi.enableIPv6()` before `WiFi.begin`). Without it, every `.local` lookup on macOS took **5.0 s**, because it waits for an IPv6 (AAAA) answer that never comes. With it, the lookup takes 0.005 s and the page loads in under 1 s.
- WiFi signal at the bench: RSSI about −76 to −79 dBm, with ping 30–320 ms. That's weak. If the UI feels laggy, move the ESP32 closer to an access point.

## If servo-control.local doesn't resolve (UniFi settings to check)
- **Different networks:** the laptop and the ESP32 must be on the same network/VLAN, or the UniFi **mDNS** option (reflector) must be on. mDNS is how `.local` names are found, and it doesn't cross VLANs by default.
- **Client Device Isolation** must be **off** for that SSID. Otherwise the laptop can't reach the ESP32 at all.
- **Multicast/Broadcast blocking** options can drop mDNS traffic.
- **Connection problems:** if the ESP32 won't connect, try WPA2/WPA3 mixed instead of WPA3-only, and turn off Fast Roaming (802.11r) for that SSID.
