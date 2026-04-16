#!/usr/bin/env python3
import re
import sys

from datetime import datetime
from urllib.request import urlopen, Request
from urllib.error import HTTPError

MIXXX_DOWNLOAD_BASE = "https://downloads.mixxx.org/dependencies/"
DEFAULT_PATTERN = (
    r'BUILDENV_NAME="(mixxx-deps-[0-9]\.[0-9]-{triplet}-)'
    r'([a-z0-9]+)"\n(\W+)BUILDENV_SHA256="([^"]+)"'
)
DEFAULT_REPLACE = (
    r'BUILDENV_NAME="\g<1>{version}"\n\g<3>BUILDENV_SHA256="{shasum}"'
)
PLATFORMS = [
    {
        "host_os": "Windows",
        "triplet": "arm64-windows",
        "release_triplet": "arm64-windows-rel",
        "file": "tools/windows_buildenv.bat",
        "pattern": (
            r"SET BUILDENV_NAME=(mixxx-deps-[0-9]\.[0-9]-{triplet}-)"
            r"([a-z0-9]+)(\r?\n\s+)SET BUILDENV_SHA256=([a-f0-9]+)"
        ),
        "replace": (
            r"SET BUILDENV_NAME=\g<1>{version}\g<3>"
            r"SET BUILDENV_SHA256={shasum}"
        ),
    },
    {
        "host_os": "Windows",
        "triplet": "x64-windows",
        "release_triplet": "x64-windows-rel",
        "file": "tools/windows_buildenv.bat",
        "pattern": (
            r"SET BUILDENV_NAME=(mixxx-deps-[0-9]\.[0-9]-{triplet}-)"
            r"([a-z0-9]+)(\r?\n\s+)SET BUILDENV_SHA256=([a-f0-9]+)"
        ),
        "replace": (
            r"SET BUILDENV_NAME=\g<1>{version}\g<3>"
            r"SET BUILDENV_SHA256={shasum}"
        ),
    },
    {
        "host_os": "macOS",
        "triplet": "arm64-osx-cross",
        "release_triplet": "arm64-osx-cross-rel",
        "file": "tools/macos_buildenv.sh",
    },
    {
        "host_os": "macOS",
        "triplet": "x64-osx",
        "release_triplet": "x64-osx-rel",
        "file": "tools/macos_buildenv.sh",
    },
    {
        "host_os": "macOS",
        "triplet": "arm64-osx",
        "release_triplet": "arm64-osx-rel",
        "file": "tools/macos_buildenv.sh",
    },
    #  {
    #      "host_os": "Linux",
    #      "triplet": "arm64-android",
    #      "release_triplet": "arm64-android-release",
    #      "file": "tools/android_buildenv.sh",
    #  },
    #  {
    #      "host_os": "Linux",
    #      "triplet": "x64-linux",
    #      "release_triplet": "x64-linux-release",
    #      "file": "tools/linux_buildenv.sh",
    #  },
]


def detect_line_ending(content):
    """Detect the predominant line ending style in the content."""
    crlf_count = content.count("\r\n")
    lf_count = content.count("\n") - crlf_count

    if crlf_count > lf_count:
        return "\r\n"
    return "\n"


def update(host_os, channel, data):
    with open(host_os["file"], "r", newline="") as file:
        content = file.read()

    # Store EOL style (CRLF/LF of text files are set by Git at checkout)
    original_line_ending = detect_line_ending(content)

    # Normalize to LF for consistent pattern matching
    normalized_content = content.replace("\r\n", "\n")

    print(
        f"Updating {host_os['file']} for {data.get('triplet', 'unknown')} "
        f"to version {data.get('version', 'unknown')}"
    )

    # Perform the replacement on normalized content
    pattern = host_os.get("pattern", DEFAULT_PATTERN).format(**data)
    replace = host_os.get("replace", DEFAULT_REPLACE).format(**data)

    # Replace \r?\n with \n in pattern for normalized matching
    normalized_pattern = pattern.replace(r"\r?\n", r"\n")

    new_content, count = re.subn(
        normalized_pattern, replace, normalized_content
    )

    # Check if the pattern was found
    if count == 0:
        print(
            f"Warning: Pattern not found in {host_os['file']} for "
            f"{data.get('triplet', 'unknown')}. Pattern may need updating.",
            file=sys.stderr,
        )
        return False

    # Check if version already exists (content unchanged)
    if new_content == normalized_content:
        print(
            f"Skipping {host_os['file']}: "
            f"Version {data.get('version', 'unknown')} "
            f"already up-to-date for {data.get('triplet', 'unknown')}."
        )
        return True

    # Convert back to original line ending style
    if original_line_ending == "\r\n":
        new_content = new_content.replace("\n", "\r\n")

    # Write with newline="" to preserve exact bytes
    with open(host_os["file"], "w", newline="") as file:
        file.write(new_content)

    print(
        f"Updated {host_os['file']} for {data.get('triplet', 'unknown')} "
        f"to version {data.get('version', 'unknown')}"
    )
    return True


def get_raw_releases(platform, version):
    url = f"{MIXXX_DOWNLOAD_BASE}{version}/{platform}/"
    request = Request(
        url,
        headers={"User-Agent": "Mozilla/5.0 (Mixxx vcpkg updater)"},
    )
    with urlopen(request) as f:
        return f.read().decode()


def get_sha256sum(platform, version, filename):
    url = f"{MIXXX_DOWNLOAD_BASE}{version}/{platform}/" f"{filename}.sha256sum"
    request = Request(
        url,
        headers={"User-Agent": "Mozilla/5.0 (Mixxx vcpkg updater)"},
    )
    with urlopen(request) as f:
        return f.read().decode().split(" ")


def parse_files(data):
    return [
        re.split(r"([<>]|\W{2,})", d)
        for d in data.split("\n")
        if d.strip().startswith("<a href")
    ]


def extra_info(data):
    filename = data[4]
    components = filename.removesuffix(".zip").split("-")
    return dict(
        filename=filename,
        triplet="-".join(components[3:-1]),
        version=components[-1],
        timestamp=datetime.strptime(data[-3].strip(), "%d-%b-%Y %H:%M"),
    )


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(
            (
                "Usage: %s <channel>\n  <channel> is Mixxx's "
                "VCPKG release branch, e.g. 2.6."
            )
            % sys.argv[0],
            file=sys.stderr,
        )
        sys.exit(-1)

    channel_prefix = sys.argv[1]

    # Track commit hashes for validation
    release_commits = {}  # triplet -> (version, filename)
    non_release_commits = {}  # triplet -> (version, filename)

    for host_os in PLATFORMS:
        os = host_os["host_os"]
        data = None
        for is_release in [True, False]:
            triplet = host_os[
                "triplet" if not is_release else "release_triplet"
            ]
            channel = (
                channel_prefix if not is_release else f"{channel_prefix}-rel"
            )
            latest = None
            try:
                data = get_raw_releases(os, channel)
                files = list(map(extra_info, parse_files(data)))
                releases = list(
                    filter(
                        lambda d: not d["filename"].endswith("sha256sum")
                        and triplet == d["triplet"],
                        files,
                    )
                )
                releases.sort(key=lambda d: d["timestamp"])
                latest = releases[-1]
            except HTTPError as e:
                print(
                    f"HTTP Error {e.code} for channel {channel} "
                    f"on platform {os}/{triplet}: {e.reason}",
                    file=sys.stderr,
                )
                continue
            except IndexError:
                print(
                    f"No matching releases found for channel "
                    f"{channel} on platform {os}/{triplet}",
                    file=sys.stderr,
                )
                if data:
                    files = list(map(extra_info, parse_files(data)))
                    triplets = set(f["triplet"] for f in files)
                    print(
                        f"  Available triplets: {triplets}",
                        file=sys.stderr,
                    )
                continue

            latest.update(
                dict(
                    shasum=get_sha256sum(
                        host_os["host_os"], channel, latest["filename"]
                    )[0]
                )
            )

            # Track the commit hash
            if is_release:
                release_commits[triplet] = (
                    latest["version"],
                    latest["filename"],
                )
            else:
                non_release_commits[triplet] = (
                    latest["version"],
                    latest["filename"],
                )

            print(
                f"Latest version for {os}/{triplet}/{channel} is "
                f"{latest['filename']} (commit: {latest['version']})"
            )

            update(host_os, channel, latest)

    # Validate commit hash consistency
    print("\nCOMMIT HASH VALIDATION")

    has_warnings = False

    # Check release builds
    release_versions = set(v[0] for v in release_commits.values())
    print(f"\nRelease builds ({channel_prefix}-rel):")
    for triplet, (version, filename) in sorted(release_commits.items()):
        print(f"  {triplet:<25} {version}")

    if len(release_versions) > 1:
        has_warnings = True
        print(
            "\n⚠️  WARNING: Release builds have INCONSISTENT commit hashes!",
            file=sys.stderr,
        )
        print(f"    Found commits: {release_versions}", file=sys.stderr)

    # Check non-release builds
    non_release_versions = set(v[0] for v in non_release_commits.values())
    print(f"\nNon-release builds ({channel_prefix}):")
    for triplet, (version, filename) in sorted(non_release_commits.items()):
        print(f"  {triplet:<25} {version}")

    if len(non_release_versions) > 1:
        has_warnings = True
        print(
            "\n⚠️  WARNING: Non-release builds have INCONSISTENT "
            "commit hashes!",
            file=sys.stderr,
        )
        print(f"    Found commits: {non_release_versions}", file=sys.stderr)

    # Summary
    if has_warnings:
        print(
            "\n!!! BUILD ENVIRONMENT MISMATCH DETECTED !!!",
            file=sys.stderr,
        )
        print(
            "!!! Not all platforms were built from the same commit !!!",
            file=sys.stderr,
        )
        sys.exit(1)
    else:
        print("\n✅ All release builds use commit:", list(release_versions)[0])
        print(
            "✅ All non-release builds use commit:",
            list(non_release_versions)[0],
        )
        print("\n✅ Validation passed: All builds are consistent.")
