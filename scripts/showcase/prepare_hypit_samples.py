#!/usr/bin/env python3
"""Prepare official Hypit samples without modifying the external distribution.

Catalog v1: {schemaVersion:1,samples:[{name,path,sha256,sizeBytes,
durationSeconds,width,height,sourceUrl,archiveMember?}]}. Video paths are relative
to the catalog directory. All output is local; no rendering/providers are called.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tarfile
from datetime import datetime, timezone

MAX_VIDEO = 64 * 1024 * 1024
MAX_ARCHIVE = 600 * 1024 * 1024
SELECTED = {
    "productions/explainer/assets/examples/ranking-hypit-final.mp4": "Hypit 官方样例 · 排行榜",
    "productions/explainer/assets/examples/podcast-hypit-final.mp4": "Hypit 官方样例 · 播客",
    "productions/explainer/assets/examples/interview-hypit-final.mp4": "Hypit 官方样例 · 访谈",
}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def save_json(path, value):
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n")
    temp.replace(path)


def download(url, path, limit):
    if path.is_symlink():
        raise ValueError(f"Refusing symlink download: {path}")
    if path.is_file() and 0 < path.stat().st_size <= limit:
        return "reused local download"
    temporary = path.with_suffix(path.suffix + ".part")
    if temporary.is_symlink():
        raise ValueError(f"Refusing symlink download: {temporary}")
    subprocess.run(["curl", "--fail", "--location", "--proto", "=https", "--proto-redir", "=https",
                    "--retry", "1", "--max-time", "600", "--max-filesize", str(limit),
                    url, "--output", str(temporary)], check=True)
    if not 0 < temporary.stat().st_size <= limit:
        raise ValueError(f"Download size outside bounds: {url}")
    temporary.replace(path)
    return "downloaded via HTTPS"


def selected_member_name(member):
    raw = member.name
    normalized = raw[2:] if raw.startswith("./") else raw
    if member.isdir() and normalized in (".", ""):
        return None
    if member.isdir():
        normalized = normalized.rstrip("/")
    parts = normalized.split("/")
    if "\\" in raw or "\0" in raw or PurePosixPath(normalized).is_absolute() or any(p in ("", ".", "..") for p in parts):
        raise ValueError(f"Unsafe archive member: {raw}")
    if normalized not in SELECTED:
        return None
    if not member.isfile() or member.issym() or member.islnk() or not 0 < member.size <= MAX_VIDEO:
        raise ValueError(f"Selected member is not a bounded regular video: {raw}")
    return normalized


def inspect_video(path, ffprobe, ffmpeg):
    probe = subprocess.run([ffprobe, "-v", "error", "-show_streams", "-show_format", "-of", "json", str(path)],
                           check=True, capture_output=True, text=True, timeout=60)
    data = json.loads(probe.stdout)
    video = next((s for s in data["streams"] if s.get("codec_type") == "video"), None)
    if not video or not 0 < path.stat().st_size <= MAX_VIDEO:
        raise ValueError(f"No bounded video: {path}")
    duration = float(data["format"]["duration"])
    if not 0 < duration <= 86400 or not 0 < video["width"] <= 16384 or not 0 < video["height"] <= 16384:
        raise ValueError(f"Invalid video dimensions/duration: {path}")
    decoded = subprocess.run([ffmpeg, "-nostdin", "-v", "error", "-xerror", "-i", str(path),
                              "-map", "0:v:0", "-map", "0:a?", "-f", "null", "-"],
                             check=True, capture_output=True, text=True, timeout=300)
    return {"sha256": sha256(path), "sizeBytes": path.stat().st_size,
            "durationSeconds": duration, "width": video["width"], "height": video["height"],
            "validation": {"ffprobeExitCode": probe.returncode, "fullDecodeExitCode": decoded.returncode,
                           "fullDecodeStderr": decoded.stderr}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    repository = Path(__file__).resolve().parents[2]
    parser.add_argument("--distribution", type=Path, default=repository.parent / "hypit")
    parser.add_argument("--output", type=Path, default=repository / ".workbench/showcase")
    parser.add_argument("--ffmpeg", default=shutil.which("ffmpeg"))
    parser.add_argument("--ffprobe", default=shutil.which("ffprobe"))
    args = parser.parse_args()
    output = args.output.resolve()
    distribution = args.distribution.resolve()
    if output == distribution or distribution in output.parents:
        parser.error("Output must not modify the external distribution")
    output.mkdir(parents=True, exist_ok=True)
    downloads = output / "downloads"
    videos = output / "videos"
    for folder in (downloads, videos):
        if folder.is_symlink():
            raise ValueError(f"Refusing symlink output folder: {folder}")
        folder.mkdir(exist_ok=True)
    manifest = {"preparedAt": datetime.now(timezone.utc).isoformat(), "status": "running",
                "distribution": str(distribution), "downloads": [], "samples": [], "failures": [],
                "attribution": "Official Hypit example media; reused for demonstration, not original work of this workbench.",
                "sourceReadme": "examples/complex-explainer/README.md",
                "selectionEvidence": "examples/complex-explainer/productions/explainer/authors/assets.svml:46-48",
                "scope": "Only the published final film and three explicitly named Hypit example clips are extracted. No provider calls."}
    try:
        if not args.ffmpeg or not args.ffprobe:
            raise ValueError("ffmpeg and ffprobe must be available or explicitly provided")
        readme = distribution / manifest["sourceReadme"]
        manifest["readmeSha256"] = sha256(readme)
        manifest["upstreamCommit"] = subprocess.run(["git", "-C", str(distribution), "rev-parse", "HEAD"],
                                                    capture_output=True, text=True, check=True).stdout.strip()
        manifest["existingLocalSamples"] = []
        local_paths = [distribution / "output/chat-demo.mp4"]
        local_paths += sorted((distribution / "examples/semantic-composition/.hypit/results").glob("*/bld_*/files/file-0001.mp4"))
        for local in local_paths:
            if local.is_file() and not local.is_symlink() and 0 < local.stat().st_size <= MAX_VIDEO:
                manifest["existingLocalSamples"].append({"path": str(local.relative_to(distribution)),
                    "sha256": sha256(local), "sizeBytes": local.stat().st_size})
        urls = re.findall(r"https://storage\.googleapis\.com/hypit-public-assets/[^\s)]+", readme.read_text())
        selected_urls = {}
        for filename in ("final.mp4", "media.tar.gz"):
            matches = sorted({url for url in urls if url.endswith("/" + filename)})
            if len(matches) != 1:
                raise ValueError(f"Expected one official {filename} URL in pinned README")
            selected_urls[filename] = matches[0]
            path = downloads / filename
            action = download(matches[0], path, MAX_VIDEO if filename.endswith(".mp4") else MAX_ARCHIVE)
            manifest["downloads"].append({"url": matches[0], "path": str(path.relative_to(output)),
                                          "sizeBytes": path.stat().st_size, "sha256": sha256(path), "action": action})
        candidates = [(downloads / "final.mp4", "Hypit 官方作品 · 复杂口播讲解（137秒）", selected_urls["final.mp4"], None)]
        inventory = []
        found = set()
        with tarfile.open(downloads / "media.tar.gz", "r:gz") as archive:
            for member in archive:
                inventory.append({"name": member.name, "sizeBytes": member.size, "type": member.type.decode("ascii", errors="replace")})
                name = selected_member_name(member)
                if name is None:
                    continue
                if name in found:
                    raise ValueError(f"Duplicate selected archive member: {name}")
                found.add(name)
                target = videos / PurePosixPath(name).name
                if target.is_symlink():
                    raise ValueError(f"Refusing symlink target: {target}")
                stream = archive.extractfile(member)
                if stream is None:
                    raise ValueError(f"Unreadable member: {name}")
                with stream, target.open("wb") as dest:
                    shutil.copyfileobj(stream, dest)
                if target.stat().st_size != member.size:
                    raise ValueError(f"Extracted size mismatch: {name}")
                candidates.append((target, SELECTED[name], selected_urls["media.tar.gz"], member.name))
        save_json(output / "archive-inventory.json", inventory)
        if found != set(SELECTED):
            raise ValueError(f"Missing selected clips: {sorted(set(SELECTED) - found)}")
        samples = []
        for path, name, url, member in candidates:
            entry = {"name": name, "path": str(path.relative_to(output)), "sourceUrl": url,
                     **inspect_video(path, args.ffprobe, args.ffmpeg)}
            if member:
                entry["archiveMember"] = member
            samples.append(entry)
            print(f"Verified {entry['path']}: {entry['durationSeconds']:.3f}s, {entry['width']}x{entry['height']}, {entry['sha256']}", flush=True)
            manifest["samples"] = samples
            save_json(output / "preparation-manifest.json", manifest)
        if len({entry["sha256"] for entry in samples}) < 3:
            raise ValueError("Fewer than three distinct official video hashes")
        save_json(output / "catalog.json", {"schemaVersion": 1, "samples": samples})
        manifest["status"] = "verified"
    except Exception as error:
        manifest["status"] = "failed"
        manifest["failures"].append(f"{type(error).__name__}: {error}")
        raise
    finally:
        save_json(output / "preparation-manifest.json", manifest)


if __name__ == "__main__":
    main()
