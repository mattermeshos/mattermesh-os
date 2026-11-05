"""PackBits for the 1 bpp lock-screen art (decoded row by row by src/ui/packbits.h).

Header byte h: 0..127 -> h+1 literal bytes follow; 129..255 -> the next byte repeated 257-h times.
"""


def pack(data):
    out, i, n = [], 0, len(data)
    while i < n:
        run = 1
        while i + run < n and run < 128 and data[i + run] == data[i]:
            run += 1
        if run >= 3:
            out += [257 - run, data[i]]
            i += run
            continue
        start = i
        while i < n and i - start < 128:
            if i + 2 < n and data[i] == data[i + 1] == data[i + 2]:
                break
            i += 1
        out += [i - start - 1] + list(data[start:i])
    assert unpack(out, n) == list(data)
    return out


def unpack(packed, size):
    out, i = [], 0
    while len(out) < size:
        h = packed[i]; i += 1
        if h < 128:
            out += packed[i:i + h + 1]; i += h + 1
        elif h > 128:
            out += [packed[i]] * (257 - h); i += 1
    return out[:size]
