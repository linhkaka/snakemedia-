"""Archive the exact checked-out build sources including initialized public submodules."""
import argparse
import json
import subprocess
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output")
    args = parser.parse_args()
    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    tracked = subprocess.check_output(["git", "ls-files", "--recurse-submodules", "-z"], cwd=ROOT)
    with tarfile.open(output, "w:gz") as archive:
        for raw in tracked.split(b"\0"):
            if not raw:
                continue
            relative = raw.decode("utf-8")
            path = (ROOT / relative).resolve()
            if ROOT not in path.parents or ".git" in Path(relative).parts or path == output or not path.is_file():
                continue
            archive.add(path, arcname="SNAKEMEDIA_Studio_Source/" + relative, recursive=False)
    print(json.dumps({"source_archive": str(output), "bytes": output.stat().st_size}))


if __name__ == "__main__":
    main()
