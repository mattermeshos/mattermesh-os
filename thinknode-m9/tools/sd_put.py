"""Upload a file to the T-Deck SD card over USB using the firmware's `sd put` command.
Acknowledged 8 KiB chunks with a running CRC-32; verifies the final CRC and optionally
re-reads the file on the device (`sd crc`). Never flashes, never touches other files."""
import argparse, re, sys, time, zlib
from pathlib import Path
import serial


def readline(port, timeout):
    end = time.monotonic() + timeout; data = b""
    while time.monotonic() < end:
        line = port.readline()
        if line:
            data += line
            if data.endswith(b"\n"):
                return data.decode(errors="replace").strip()
    raise TimeoutError("no line from device: " + data.decode(errors="replace"))


def expect(port, prefix, timeout):
    while True:
        line = readline(port, timeout)
        if line.startswith(prefix):
            return line
        if line.startswith("SD_PUT_") or line.startswith("SD_CRC"):
            raise RuntimeError(line)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("source"); p.add_argument("dest", help="absolute path on the SD card, e.g. /mattermesh/asr-it.mmq")
    p.add_argument("--port", default="/dev/ttyACM0"); p.add_argument("--verify", action="store_true", help="re-read the file on the device and compare CRC")
    a = p.parse_args()
    data = Path(a.source).read_bytes(); crc_all = zlib.crc32(data)
    # DTR/RTS left as the kernel sets them on open: driving them low (before or after open) resets the ESP32-S3
    # through its USB-JTAG bridge (6 of 6 opens when set before open, none when left alone).
    port = serial.Serial(a.port, 115200, timeout=0.4)
    # Wake the device's USB serial first: after a while with no reader the Arduino HWCDC marks itself disconnected and
    # holds its output until the host sends something. A bare newline is an empty command; drain what was queued.
    port.write(b"\n"); port.flush(); end = time.monotonic() + 1.0
    while time.monotonic() < end:
        if not port.read(4096): break
    port.reset_input_buffer()
    port.write(f"sd put {a.dest} {len(data)}\n".encode()); port.flush()
    ready = expect(port, "SD_PUT_READY", 10); chunk = int(ready.split()[2])
    sent = 0; crc = 0; t0 = time.monotonic(); last = 0; resends = 0
    while sent < len(data):
        block = data[sent:sent + chunk]; port.write(block); port.flush()
        ack = expect(port, ("SD_PUT_ACK", "SD_PUT_RESEND"), 20)
        if ack.startswith("SD_PUT_RESEND"):  # bytes lost on the USB link: the device dropped the chunk, send it again
            if int(ack.split()[1]) != sent:
                raise RuntimeError(f"resend mismatch at {sent}: {ack}")
            resends += 1; continue
        _, done, got = ack.split()
        sent += len(block); crc = zlib.crc32(block, crc)
        if int(done) != sent or int(got, 16) != crc:
            raise RuntimeError(f"ack mismatch at {sent}: {ack}")
        if time.monotonic() - last > 2:
            last = time.monotonic(); print(f"\r{sent / 1e6:7.2f} / {len(data) / 1e6:.2f} MB  {sent / 1e3 / (time.monotonic() - t0):6.1f} kB/s", end="", flush=True)
    final = expect(port, "SD_PUT_", 30); print(); print(final)
    if not final.startswith("SD_PUT_DONE") or int(final.split()[2], 16) != crc_all:
        sys.exit("upload failed")
    print(f"uploaded {len(data)} bytes in {time.monotonic() - t0:.1f}s, crc {crc_all:08x}, {resends} chunk(s) resent")
    if a.verify:
        port.write(f"sd crc {a.dest}\n".encode()); port.flush()
        line = expect(port, "SD_CRC", 180); print(line)
        m = re.search(r"bytes=(\d+) crc=([0-9a-f]+)", line)
        if not m or int(m.group(1)) != len(data) or int(m.group(2), 16) != crc_all:
            sys.exit("verification failed")
        print("verified on device")


if __name__ == "__main__":
    main()
