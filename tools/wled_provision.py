#!/usr/bin/env python3
"""Push WiFi credentials to WLED via Improv Wi-Fi Serial protocol."""
import serial, sys, time, struct

IMPROV_HEADER   = b'IMPROV'
CMD_WIFI_SETTINGS = 0x01
TYPE_RPC_COMMAND  = 0x04
TYPE_RPC_RESULT   = 0x05
TYPE_CURRENT_STATE = 0x01

def checksum(data: bytes) -> int:
    return sum(data) & 0xFF

def build_rpc(cmd: int, payload: bytes) -> bytes:
    inner = bytes([cmd, len(payload)]) + payload
    outer = IMPROV_HEADER + bytes([1, TYPE_RPC_COMMAND, len(inner)]) + inner
    return outer + bytes([checksum(outer)])

def encode_string(s: str) -> bytes:
    b = s.encode()
    return bytes([len(b)]) + b

def provision(port: str, ssid: str, password: str, baud: int = 115200):
    payload = encode_string(ssid) + encode_string(password)
    packet  = build_rpc(CMD_WIFI_SETTINGS, payload)

    print(f"Opening {port} at {baud} baud...")
    with serial.Serial(port, baud, timeout=1, dsrdtr=False, rtscts=False) as s:
        s.dtr = False  # don't assert DTR — prevents resetting the device on open
        time.sleep(5)  # let device finish booting
        s.reset_input_buffer()

        print(f"Sending WiFi credentials for '{ssid}'...")
        for _ in range(3):  # retry a few times in case device is still starting
            s.write(packet)
            s.flush()
            time.sleep(0.5)

        print("Waiting for device to connect (up to 30s)...")
        deadline = time.time() + 30
        buf = b""
        while time.time() < deadline:
            chunk = s.read(s.in_waiting or 1)
            if chunk:
                buf += chunk
                idx = buf.find(IMPROV_HEADER)
                while idx != -1:
                    if len(buf) >= idx + 9:
                        pkt_type = buf[idx + 7]
                        length   = buf[idx + 8]
                        end      = idx + 9 + length + 1
                        if len(buf) >= end:
                            if pkt_type == TYPE_RPC_RESULT:
                                data = buf[idx + 9: idx + 9 + length]
                                # result payload: status(1) + strings
                                if data and data[0] == 0x01:
                                    # parse URL strings
                                    offset = 1
                                    while offset < len(data):
                                        slen = data[offset]; offset += 1
                                        val  = data[offset:offset+slen].decode(errors='replace')
                                        offset += slen
                                        if val.startswith("http"):
                                            print(f"Connected! Device at: {val}")
                                            return
                                else:
                                    print(f"Got result, status={data[0] if data else '?'}")
                            buf = buf[end:]
                            idx = buf.find(IMPROV_HEADER)
                        else:
                            break
                    else:
                        break
            time.sleep(0.1)
        print("Timed out waiting for connection confirmation.")
        print("Check your router for a device named 'hockeystick'.")

if __name__ == "__main__":
    env = {}
    try:
        with open(".env") as f:
            for line in f:
                line = line.strip()
                if "=" in line and not line.startswith("#"):
                    k, v = line.split("=", 1)
                    env[k.strip()] = v.strip()
    except FileNotFoundError:
        print("No .env file found. Create one with WIFI_SSID and WIFI_PASSWORD.")
        sys.exit(1)

    port     = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.SLAB_USBtoUART"
    ssid     = env.get("WIFI_SSID", "")
    password = env.get("WIFI_PASSWORD", "")

    if not ssid:
        print("WIFI_SSID not set in .env")
        sys.exit(1)

    provision(port, ssid, password)
