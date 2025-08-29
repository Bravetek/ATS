
import argparse, json, time, sys
from pathlib import Path
try:
    import yaml
except Exception:
    yaml = None

from runners.efficiency_xload import run_efficiency
from utils.progress import Progress
import os

def sanity_check_writable(out: Path):
    out.mkdir(parents=True, exist_ok=True)
    probe = out / "_probe.txt"
    try:
        with open(probe, "w", encoding="utf-8", newline="\n") as f:
            f.write("ok")
        probe.unlink(missing_ok=True)
    except PermissionError as e:
        raise RuntimeError(f"Run folder not writable: {out} ({e})")
    
def load_plan(path: str):
    p = Path(path)
    s = p.read_text(encoding="utf-8")
    if p.suffix.lower() in (".yaml",".yml") and yaml:
        return yaml.safe_load(s)
    return json.loads(s)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--plan", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--events", default="events.ndjson")
    args = ap.parse_args()

    out = Path(args.out); out.mkdir(parents=True, exist_ok=True)
    sanity_check_writable(out)  #####
    prog = Progress(out, events_filename=args.events)
    prog.update(state="running", percent=0, message="Start")

    plan = load_plan(args.plan)
    try:
        run_efficiency(plan, out, prog)
        outs = plan["outputs"]
        prog.update(state="passed", percent=100, message="Done",
                    artifacts=[{"type":"csv","path":outs["csv"]},
                               {"type":"xlsx","path":outs["excel"]}])
        if plan["modes"] == "run" :
            prog.event({"type":"done","state":"passed"})
    except Exception as e:
        prog.update(state="failed", message=str(e))
        prog.event({"type":"done","state":"failed","error":str(e)})
        raise

if __name__ == "__main__":
    main()
