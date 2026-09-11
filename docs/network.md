# Network

- The ESP32 joins the home WiFi (2.4 GHz, Ubiquiti UniFi access points). It doesn't create its own network (D-008).
- Address: **http://servo-control.local**. Set with `WiFi.setHostname("servo-control")` and `MDNS.begin("servo-control")`. One ESP32 will drive all 6 joints on one CAN bus, so one hostname is enough.
- Fallback: the IP address is printed on the serial monitor at boot. Optionally, reserve a fixed IP for the ESP32 in UniFi (client → Settings → Fixed IP).
- No login in v1. Anyone on the home WiFi can open the page.
- WiFi credentials go in `firmware/.../secrets.h`, which is gitignored and never committed. The user types them in themselves.

## If servo-control.local doesn't resolve (UniFi settings to check)
- **Different networks:** the laptop and the ESP32 must be on the same network/VLAN, or the UniFi **mDNS** option (reflector) must be on. mDNS is how `.local` names are found, and it doesn't cross VLANs by default.
- **Client Device Isolation** must be **off** for that SSID. Otherwise the laptop can't reach the ESP32 at all.
- **Multicast/Broadcast blocking** options can drop mDNS traffic.
- **Connection problems:** if the ESP32 won't connect, try WPA2/WPA3 mixed instead of WPA3-only, and turn off Fast Roaming (802.11r) for that SSID.
