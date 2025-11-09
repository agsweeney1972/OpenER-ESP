### Updated repo for the Waveshare ESP32-P4-Nano | https://github.com/AGSweeney/ESP32p4-EIP

# ESP32 OpENer Ethernet/IP Adapter

This project ports the [OpENer](https://github.com/EIPStackGroup/OpENer) Ethernet/IP stack to the **Olimex ESP32-Gateway Rev. F** and adds the glue code required to use the module as a Micro850-class adapter.  It builds on Espressif’s ESP-IDF (v5.5.1) toolchain and targets a wired Ethernet installation (no Wi-Fi).

---

## Hardware Overview

- **Board**: Olimex ESP32-Gateway Rev. F  
  - PHY: LAN8720, MAC-to-PHY clock is supplied via GPIO17 (configured as RMII clock output).  
  - User LED: `GPIO33` (active high with pull-down).  
  - Ethernet connector already routes RMII signals; no extra wiring beyond USB power/programming is needed.

---

## Features

- OpENer 2.x exclusive-owner implementation with 32-byte input and output assemblies (`Instance 100` / `150`).  
- Configuration assembly (`Instance 151`) provisioned (10 bytes), but Micro850 scanners use a null config path.  
- Automatic run/idle header compensation: the stack tolerates Rockwell’s four-byte run/idle header even when disabled.  
- PLC output bit `O:[0].0` → drives the Gateway’s user LED on `GPIO33`.  
- Trace level trimmed to warnings/errors for low-noise monitoring.

---

## Repository Layout

- `components/opener/` – OpENer stack source with the ESP32 port.
  - `src/ports/ESP32/sample_application/sampleapplication.c` – assembly definitions, LED control, data mirroring.  
  - `src/cip/cipioconnection.c` – patched to tolerate implicit run/idle headers in both directions.
  - `CMakeLists.txt` – links against ESP-IDF driver component for GPIO access.
- `main/` – ESP-IDF entry point (unchanged from stock ESP32 OpENer example).

---

## Build Instructions

1. Install ESP-IDF 5.5.1 (or reuse an existing ESP-IDF environment).  
2. Set up environment variables, then change into the project directory:
   ```powershell
   cd <workspace>/esp32_opener_project
   <esp-idf>/export.ps1
   ```
3. Configure Ethernet pins if needed (defaults match Olimex Gateway).  
4. Build & flash:
   ```powershell
   idf.py build
   idf.py -p COMx flash
   idf.py -p COMx monitor
   ```
5. When monitoring, only warnings/errors are printed by default. Add `OPENER_TRACE_LEVEL_INFO` if deeper traces are required.

---

## PLC Configuration (Micro850 / CCW)

| Parameter | Value |
|-----------|-------|
| Device Type | Generic Ethernet/IP |
| IP Address | DHCP (default). A ready-to-use static-IP template is provided in `main/static_main.c`; rename it to `main.c` and update the literals to fix the address. |
| Comm Format | `Data – SINT` |
| Requested Packet Interval | e.g. 20 ms |
| Input Assembly | Instance `100`, Size `32` bytes |
| Output Assembly | Instance `150`, Size `32` bytes |
| Configuration | Instance `151`, Size ignored (PLC sends null config) |
| Connection | Unicast |

Notes:
- The Micro850 always prepends a 4-byte run/idle header even when the device advertises “no header”. The patched stack strips or inserts the header automatically, so you still get a full 32-byte payload in both directions.  
- Static IPs are handled via the alternate entry point described above; no edits to `networkconfig.c` are required.  

---

## Assembly Mapping

| Assembly | Direction | Buffer | Description |
|----------|-----------|--------|-------------|
| 100 | Adapter → PLC | `g_assembly_data064[32]` | Mirrors PLC output data and includes LED state in byte 0 bit 0. |
| 150 | PLC → Adapter | `g_assembly_data096[32]` | Source for 32-byte payload. Bit 0 of byte 0 drives GPIO33. |
| 151 | Config | `g_assembly_data097[10]` | Placeholder configuration buffer (unused by Micro850). |

`AfterAssemblyDataReceived()` copies the 32-byte output buffer into the input buffer so the PLC can verify written data. LED handling is performed immediately after the copy.

---

## Run/Idle Handling

Rockwell scanners reserve four bytes for a Run/Idle DINT. Even when the device disables headers, the Micro850 keeps sending them. Modifications in `cipioconnection.c` handle that scenario:

- `SetupIoConnectionOriginatorToTargetConnectionPoint()` tolerates a +4-byte size gap.  
- `HandleReceivedIoConnectionData()` discards the redundant header before delivering data to the input assembly.  
- Producing path continues to omit the header, keeping packet sizes at 32 data bytes plus sequence number.

---

## LED Output

- `GPIO33` is configured as an output during `ApplicationInitialization()` (edit `kStatusLedGpio` to target a different pin).  
- Bit 0 (`0x01`) of `g_assembly_data096[0]` controls the LED:
  - `1` → LED ON  
  - `0` → LED OFF  
- The LED state reflects in the mirrored input assembly so the PLC can confirm the device state.

---

## Troubleshooting

| Issue | Symptom | Resolution |
|-------|---------|------------|
| ForwardOpen fails with size mismatch | Monitor shows `O->T size mismatch` or `T->O size mismatch` | Ensure PLC sizes are 32/32. |
| LED does not change | No effect when writing `O:[0].0` | Confirm PLC output is being written (input mirror should match). Check that `GPIO33` (or your customized pin) is wired correctly. |
| Configuration assembly shows `399` in CCW | CCW dialog displays 399 bytes | Micro850 uses a null configuration path; value is cosmetic. |
| Excessive log chatter | Monitor floods with size traces | Default build now logs warnings/errors only. Re-enable INFO by editing `OPENER_TRACE_LEVEL` if deeper diagnostics are needed. |

---

## Credits

- OpENer stack maintained by the EIPStackGroup.  
- ESP32 port adapted from the official OpENer Source Code.  
- Ethernet/IP troubleshooting and LED integration developed during Micro850 interoperability testing.

---

## License

OpENer-ESP is released under the terms specified in the upstream project (see `LICENSE`). All modifications in this repository follow the same license unless stated otherwise.


