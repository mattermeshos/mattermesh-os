Import('env')
# Default OTA server list of the Plus firmware, from a private file outside the repository
# (~/.config/mattermesh/ota-default-servers.txt, one URL per line); without it there is no default server.
# The device never shows it (serial prints only the server count).
from pathlib import Path
import os
src=Path(os.path.expanduser('~/.config/mattermesh/ota-default-servers.txt'))
urls=' '.join(l.strip() for l in src.read_text().splitlines() if l.strip()) if src.exists() else ''
env.Append(CPPDEFINES=[('MM_OTA_DEFAULT_SERVERS',env.StringifyMacro(urls))])
