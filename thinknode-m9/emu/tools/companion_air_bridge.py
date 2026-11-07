#!/usr/bin/env python3
"""Real mesh -> emulator air: a MeshCore companion on USB hears the real mesh, the emulated radios hear it too.

The companion (MeshCore companion firmware, USB serial) pushes every packet its radio receives as PUSH_CODE_LOG_RX_DATA
(0x88: snr*4, rssi, raw packet) while an app is connected. Each one is sent into the emulator air broker
(emu/air/air_broker.cpp) as a transmission of the node --name, on the RF settings of the companion, so an emulated
M9 / Plus on that broker receives it like a packet off its own antenna. Receive only: the emulators' transmissions
stay in the emulated air.

  companion_air_bridge.py --serial /dev/serial/by-id/usb-Seeed_Studio_XIAO_...-if00 --air 7300 [--name RealMesh]

Give the by-id path of the companion, never a bare /dev/ttyACMn: those numbers move when boards are plugged in and
out, and a T-Deck on the same number would get the companion commands.

Needs pyserial (the PlatformIO venv has it).
"""
import argparse, select, socket, struct, sys, time
import serial

CMD_APP_START = 0x01
RESP_SELF_INFO = 0x05
PUSH_LOG_RX = 0x88


def frame(payload):
    return b'<' + struct.pack('<H', len(payload)) + payload


def airtime_us(sf, bw, cr, n, preamble=16, crc=True, implicit=False, ldro=False):
    """Same formula as emu/devices/lr1110.cpp airtime()."""
    symbol = (1000000 << sf) // bw
    divisor = 4 * (sf - (2 if ldro else 0))
    bits = 8 * n + 16 * crc - 4 * sf + (0 if sf <= 6 else 8) + (0 if implicit else 20)
    symbols = (max(0, bits) + divisor - 1) // divisor
    total = symbol * ((preamble + 8) * 4 + (25 if sf <= 6 else 17) + symbols * cr * 4) // 4
    header = min(total, symbol * ((preamble + 8) * 4 + (25 if sf <= 6 else 17)) // 4)
    return total, header


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--serial', required=True, help='/dev/serial/by-id/... of the companion')
    ap.add_argument('--air', type=int, default=7300, help='air broker TCP port on localhost')
    ap.add_argument('--name', default='RealMesh')
    a = ap.parse_args()

    air = socket.create_connection(('127.0.0.1', a.air))
    air.sendall(f'HELLO 1 {a.name}\n'.encode())
    if not air.recv(64).startswith(b'READY'):
        sys.exit('air broker refused the bridge')
    air.setblocking(False)

    # dsrdtr/rtscts off and the lines left alone: an nRF52 does not reset on them, but keep the port quiet anyway.
    port = serial.Serial(a.serial, 115200, timeout=0)
    port.write(frame(bytes([CMD_APP_START, 3]) + bytes(6) + b'mm-air-bridge'))
    rf = {'freq': 869618000, 'bw': 62500, 'sf': 8, 'cr': 8}
    buf = b''
    heard = 0
    print(f'BRIDGE serial={a.serial} air={a.air} name={a.name}', flush=True)
    while True:
        r, _, _ = select.select([port.fileno(), air], [], [], 1.0)
        if air in r:
            try:
                if not air.recv(65536):
                    sys.exit('air broker closed')
            except BlockingIOError:
                pass  # START / END of the emulators' own transmissions: not sent to the real mesh
        if port.fileno() not in r:
            continue
        buf += port.read(4096)
        while True:
            start = buf.find(b'>')
            if start < 0:
                buf = b''
                break
            if len(buf) < start + 3:
                buf = buf[start:]
                break
            n = struct.unpack('<H', buf[start + 1:start + 3])[0]
            if len(buf) < start + 3 + n:
                buf = buf[start:]
                break
            p = buf[start + 3:start + 3 + n]
            buf = buf[start + 3 + n:]
            if not p:
                continue
            if p[0] == RESP_SELF_INFO and len(p) >= 58:
                # code, type, tx power, max power, pubkey 32, lat 4, lon 4, 4 policy bytes, freq (kHz), bw (Hz), sf, cr
                khz, bw = struct.unpack('<II', p[48:56])
                rf.update(freq=khz * 1000, bw=bw, sf=p[56], cr=p[57])
                name = p[58:].split(b'\0')[0].decode('utf-8', 'replace')
                print(f'COMPANION name={name} freq={khz / 1000:.3f} MHz bw={bw / 1000:.2f} kHz sf={p[56]} cr={p[57]}', flush=True)
            elif p[0] == PUSH_LOG_RX and len(p) > 3:
                snr, rssi, raw = struct.unpack('b', p[1:2])[0] / 4, struct.unpack('b', p[2:3])[0], p[3:]
                total, header = airtime_us(rf['sf'], rf['bw'], rf['cr'], len(raw))
                line = f"TX {total} {header} {rf['freq']} {rf['bw']} {rf['sf']} {rf['cr']} {0x1424} 16 0 1 0 0 {raw.hex()}\n"
                try:
                    air.sendall(line.encode())
                except BlockingIOError:
                    pass
                heard += 1
                print(f'RX#{heard} {time.strftime("%H:%M:%S")} snr={snr:.1f} rssi={rssi} len={len(raw)} type={(raw[0] >> 2) & 15}', flush=True)


if __name__ == '__main__':
    main()
