"""Matterhorn skyline traced from the user's photo (east face / Hornli ridge on the right, north face profile on
the left, Gornergrat side), in lock-crop coordinates: summit (118,88), mountain base y 184 (tools/make_lock_hero.py,
tools/make_lock_wallpapers.py). Left ridge: hooked tip, then the straight ~56 deg north-face profile down to the snow shoulder; right: the
~64 deg upper Hornli ridge, the shoulder step at ~55 % of the height, then the ~42 deg lower ridge. Shared by the stylized hero and the
raster wallpapers so every lock wallpaper carries the same real outline."""
SUMMIT=(118,88);BASE=184
LEFT=[(118,88),(116,93),(114,98),(110,105),(106,113),(101,121),(95,130),(90,138),(84,146),(78,154),(72,162),(67,169),(62,176),(57,180),(51,182),(42,179),(32,182),(22,184)]
RIGHT=[(118,88),(121,93),(123,99),(126,106),(130,114),(133,123),(137,132),(143,138),(148,141),(154,141),(158,145),(164,152),(170,159),(177,165),(186,172),(194,178),(202,184)]
def _interp(pts,x):
    for (x0,y0),(x1,y1) in zip(pts,pts[1:]):
        lo,hi=min(x0,x1),max(x0,x1)
        if lo<=x<=hi:return y0+(y1-y0)*(x-x0)/(x1-x0) if x1!=x0 else min(y0,y1)
    return None
def skyline(x):
    """Ridge height (crop y) of the real profile at column x, None outside the mountain."""
    if x<LEFT[-1][0] or x>RIGHT[-1][0]:return None
    return _interp(LEFT,x) if x<=SUMMIT[0] else _interp(RIGHT,x)
def outline():
    """Closed polygon of the mountain: left base up to the summit, down the Hornli ridge to the right base."""
    return list(reversed(LEFT))+RIGHT[1:]
