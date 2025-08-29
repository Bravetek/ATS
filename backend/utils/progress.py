
import json, time
from pathlib import Path
try:
    from typing import Any, Dict
except Exception:
    pass

import os, time

def open_with_retry(path: Path, mode="a", encoding="utf-8", newline="\n", retries=20, delay=0.05):
    last = None
    for i in range(retries):
        try:
            return open(path, mode, encoding=encoding, newline=newline)
        except PermissionError as e:
            last = e
            time.sleep(delay * (i + 1))
    return open(path, mode, encoding=encoding, newline=newline)


class Progress:
    def __init__(self, out_dir: Path, events_filename="events.ndjson"):
        self.out=out_dir; self.progress=out_dir/"progress.json"; self.events=out_dir/events_filename
        self.out.mkdir(parents=True, exist_ok=True)
        try:
            f = open_with_retry(self.events, "a"); f.close()
        except PermissionError:
            pass

    def update(self, **payload):
        base={"schema_version":1,"updated_at":time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
        base.update(payload)
        data = json.dumps(base, ensure_ascii=False, indent=2)
        f = open_with_retry(self.progress, "w", encoding="utf-8", newline="\n")
        try:
            f.write(data); f.flush()
            try: os.fsync(f.fileno())
            except Exception: pass
        finally:
            try: f.close()
            except Exception: pass

    def event(self, obj):
        f = open_with_retry(self.events, "a", encoding="utf-8", newline="\n")
        try:
            f.write(json.dumps(obj, ensure_ascii=False) + "\n")
            f.flush()
            try: os.fsync(f.fileno())
            except Exception: pass
        finally:
            try: f.close()
            except Exception: pass

