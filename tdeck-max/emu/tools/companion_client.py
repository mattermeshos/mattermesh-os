#!/usr/bin/env python3
"""Minimal MeshCore companion client over TCP (the app's "Wi-Fi" link), for emulator tests.
Frames: client -> device '<' len16le payload; device -> client '>' len16le payload (src/companion_app.inc)."""
import socket, struct, sys, time

class Companion:
    def __init__(self, host, port, timeout=5):
        self.s = socket.create_connection((host, port), timeout=timeout)
        self.buf = b""
    def send(self, payload):
        self.s.sendall(b"<" + struct.pack("<H", len(payload)) + payload)
    def recv(self, timeout=5):
        end = time.time() + timeout
        while True:
            if len(self.buf) >= 3 and self.buf[0:1] == b">":
                n = struct.unpack("<H", self.buf[1:3])[0]
                if len(self.buf) >= 3 + n:
                    frame, self.buf = self.buf[3:3 + n], self.buf[3 + n:]
                    return frame
            elif self.buf and self.buf[0:1] != b">":
                self.buf = self.buf[1:]
                continue
            left = end - time.time()
            if left <= 0:
                return None
            self.s.settimeout(left)
            try:
                chunk = self.s.recv(4096)
            except socket.timeout:
                return None
            if not chunk:
                return None
            self.buf += chunk
    def request(self, payload, want=None, timeout=5):
        self.send(payload)
        end = time.time() + timeout
        while time.time() < end:
            f = self.recv(end - time.time())
            if f is None:
                return None
            if want is None or f[0] in want:
                return f
        return None

def main():
    host, port = sys.argv[1], int(sys.argv[2])
    c = Companion(host, port)
    info = c.request(bytes([22, 3]), want={13})                     # DEVICE_QUERY -> RESP_DEVICE_INFO
    assert info, "no DEVICE_INFO"
    print("DEVICE_INFO fw_ver=%d max_contacts=%d max_channels=%d" % (info[1], info[2] * 2, info[3]))
    self_info = c.request(bytes([1]) + bytes(7) + b"emu-test", want={5})  # APP_START -> RESP_SELF_INFO
    assert self_info, "no SELF_INFO"
    name = self_info[58:].split(b"\0")[0].decode(errors="replace")
    print("SELF_INFO name=%s freq=%.3f" % (name, struct.unpack("<I", self_info[48:52])[0] / 1000))
    batt = c.request(bytes([20]), want={12})
    print("BATTERY mv=%d" % struct.unpack("<H", batt[1:3])[0])
    start = c.request(bytes([4]), want={2})                        # GET_CONTACTS -> CONTACTS_START
    count = struct.unpack("<I", start[1:5])[0]
    got = 0
    while True:
        f = c.recv(5)
        assert f, "contacts listing stalled"
        if f[0] == 3: got += 1
        elif f[0] == 4: break
    print("CONTACTS announced=%d received=%d" % (count, got))
    contact_edits(c)
    channel_import(c, info[3])
    print("PASS: companion over TCP (device query, app start, battery, contacts, contact add/export/remove/import, channel import beyond the limit)")

def channel_import(c, max_channels):
    """The app's "Import configuration": SET_CHANNEL for every channel, then contacts. More channels than fit must
    not stop the import (the app gives up at the first error)."""
    for i in range(max_channels + 8):
        secret = bytes([0x40 + (i % 64)]) + bytes([i]) * 15
        r = c.request(bytes([32, 0xFF]) + ("Imp%02d" % i).encode().ljust(32, b"\0") + secret, want={0, 1})
        assert r == bytes([0]), "SET_CHANNEL %d failed: %r" % (i, r)
    n = 0  # empty app slots read back with an empty name, as in MeshCore
    for i in range(max_channels):
        r = c.request(bytes([31, i]), want={1, 18}, timeout=2)
        n += bool(r and r[0] == 18 and r[2:34].strip(b"\0"))
    key = bytes([0xEE]) * 32
    frame = bytes([9]) + key + bytes([1, 0, 0xFF]) + bytes(64) + b"AfterChannels".ljust(32, b"\0") + struct.pack("<I", 1700000000)
    assert c.request(frame, want={0, 1}) == bytes([0]), "contact after a full channel list"
    c.request(bytes([15]) + key, want={0, 1})
    print("CHANNEL_IMPORT %d sent, %d channels on the device (max %d), contacts still accepted" % (max_channels + 8, n, max_channels))
    assert n == max_channels, "the device should be full after importing more channels than fit"

def contact_edits(c):
    """The app's contact commands (MyMesh.cpp): add, get by key, export, remove, import, reset path, export self."""
    key = bytes(range(1, 33))
    frame = bytes([9]) + key + bytes([1, 0, 0xFF]) + bytes(64) + b"EmuTest".ljust(32, b"\0") + struct.pack("<I", 1700000000)
    assert c.request(frame, want={0, 1}) == bytes([0]), "ADD_UPDATE_CONTACT failed"
    got = c.request(bytes([30]) + key, want={1, 3})
    assert got and got[0] == 3 and got[1:33] == key and got[100:107] == b"EmuTest", "GET_CONTACT_BY_KEY after add"
    assert c.request(bytes([13]) + key, want={0, 1}) == bytes([0]), "RESET_PATH failed"
    # A contact added by the app has no advert of its own to export (as MeshCore without a blob store): not found.
    assert c.request(bytes([17]) + key, want={1, 11}) == bytes([1, 2]), "EXPORT_CONTACT of an app-added contact"
    assert c.request(bytes([15]) + key, want={0, 1}) == bytes([0]), "REMOVE_CONTACT failed"
    assert c.request(bytes([30]) + key, want={1, 3}) == bytes([1, 2]), "contact still there after REMOVE"
    assert c.request(bytes([15]) + key, want={0, 1}) == bytes([1, 2]), "REMOVE of an unknown contact must say not found"
    self_card = c.request(bytes([17]), want={1, 11})
    assert self_card and self_card[0] == 11 and len(self_card) > 100, "EXPORT_CONTACT (self) failed"
    # IMPORT_CONTACT takes a contact card (an advert packet) and loops it back as if heard over the radio.
    assert c.request(bytes([18]) + self_card[1:], want={0, 1}) == bytes([0]), "IMPORT_CONTACT of a card failed"
    assert c.request(bytes([18]) + bytes(10), want={0, 1}) == bytes([1, 6]), "IMPORT_CONTACT of garbage must fail"
    print("CONTACT_EDITS add, get, reset path, remove, self card (%d bytes), import" % (len(self_card) - 1))

if __name__ == "__main__":
    main()
