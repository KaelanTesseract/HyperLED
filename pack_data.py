# HyperLED - Open Source LED Controller
#
# Copyright (c) 2026 Dennis Guse
#
# Licensed under the EUPL, Version 1.2 or - as soon they will be approved by
# the European Commission - subsequent versions of the EUPL (the "Licence");
# You may not use this work except in compliance with the Licence.
# You may obtain a copy of the Licence at:
#
# https://joinup.ec.europa.eu/software/page/eupl
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Licence is distributed on an "AS IS" basis,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the Licence for the specific language governing permissions and
# limitations under the Licence.

"""
Builds the LittleFS image from a packed copy of data/ instead of data/ itself.

The web server answers a request for /app.js from /app.js.gz when that exists, so the
uncompressed copy next to it is dead weight in the image: about 490 KB of the controller's flash.
This script gzips the text assets (.html, .js, .css) into .pio/data_packed/ - always fresh, so the
old manual "gzip -9 -k -f" step is no longer needed - copies everything else as it is, and points
the filesystem build at that folder. data/ stays the editable source and is never modified.
"""
Import("env")
import gzip
import os
import shutil

TEXT_ASSETS = (".html", ".js", ".css")

source = env.subst("$PROJECT_DATA_DIR")
target = os.path.join(env.subst("$PROJECT_DIR"), ".pio", "data_packed")

shutil.rmtree(target, ignore_errors=True)
os.makedirs(target)

for name in sorted(os.listdir(source)):
    path = os.path.join(source, name)
    if not os.path.isfile(path):
        continue
    if name.endswith(".gz"):
        continue  # a stale copy from the manual workflow; rebuilt below from the source file
    if name.endswith(TEXT_ASSETS):
        with open(path, "rb") as f:
            raw = f.read()
        # mtime=0: the same source gives the same bytes, so an unchanged file is not a change
        with open(os.path.join(target, name + ".gz"), "wb") as out:
            with gzip.GzipFile(filename="", mode="wb", fileobj=out, compresslevel=9, mtime=0) as gz:
                gz.write(raw)
    else:
        shutil.copy2(path, os.path.join(target, name))

env.Replace(PROJECT_DATA_DIR=target)
print("pack_data: web assets packed into", target)
