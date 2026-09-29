"""Preview or prune ACME grant versions after all store users have stopped."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import os
from pathlib import Path
import re
import stat


GENERATION = re.compile(r"\.grant-[0-9a-f]{16}\Z", re.ASCII)
TEMPORARY = re.compile(
    r"(?:\.xrt-write-|\.xacme-key-)[0-9a-f]{16}\.tmp\Z", re.ASCII
)
REQUIRED = frozenset({"key.pem", "fullchain.pem", "meta.txt"})


class PruneError(RuntimeError):
    """The store cannot be pruned without risking unrelated data."""


@dataclass(frozen=True)
class Generation:
    name: str
    path: Path
    files: tuple[str, ...]
    complete: bool
    modified_ns: int


@dataclass(frozen=True)
class Plan:
    current: str
    retained: tuple[Generation, ...]
    removable: tuple[Generation, ...]


def _domain_valid(domain: str) -> bool:
    if not domain or len(domain) > 253 or not domain.isascii():
        return False
    labels = domain[2:].split(".") if domain.startswith("*.") else domain.split(".")
    return all(
        1 <= len(label) <= 63
        and label[0].isalnum()
        and label[-1].isalnum()
        and all(char.isascii() and (char.isalnum() or char == "-") for char in label)
        for label in labels
    )


def _domain_directory(certificates: Path, domain: str,
                      legacy_wildcard: bool = False) -> Path:
    if legacy_wildcard:
        if os.name == "nt" or not domain.startswith("*."):
            raise PruneError("legacy wildcard layout requires a POSIX wildcard domain")
        return certificates / domain
    if not domain.startswith("*."):
        return certificates / domain
    mapped = certificates / ("%2A" + domain[1:])
    legacy = certificates / domain
    if (
        os.name != "nt"
        and not os.path.lexists(mapped / "current")
        and os.path.lexists(legacy / "current")
    ):
        return legacy
    return mapped


def _check_path(path: Path, expected: int) -> os.stat_result:
    if path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction()):
        raise PruneError(f"refusing link or junction: {path}")
    info = path.lstat()
    if not (stat.S_ISDIR(info.st_mode) if expected == stat.S_IFDIR
            else stat.S_ISREG(info.st_mode)):
        raise PruneError(f"unexpected file type: {path}")
    return info


def _current(directory: Path) -> str:
    pointer = directory / "current"
    _check_path(pointer, stat.S_IFREG)
    data = pointer.read_bytes()
    if len(data) != 24 or data[-1:] != b"\n":
        raise PruneError(f"invalid current pointer: {pointer}")
    try:
        name = data[:-1].decode("ascii")
    except UnicodeDecodeError as error:
        raise PruneError(f"invalid current pointer: {pointer}") from error
    if GENERATION.fullmatch(name) is None:
        raise PruneError(f"invalid current pointer: {pointer}")
    return name


def _generation(path: Path) -> Generation:
    info = _check_path(path, stat.S_IFDIR)
    files: list[str] = []
    with os.scandir(path) as entries:
        for entry in entries:
            name = entry.name
            if name not in REQUIRED and TEMPORARY.fullmatch(name) is None:
                raise PruneError(f"unknown generation entry: {entry.path}")
            _check_path(Path(entry.path), stat.S_IFREG)
            files.append(name)
    return Generation(
        path.name, path, tuple(sorted(files)),
        REQUIRED.issubset(files), info.st_mtime_ns,
    )


def plan(root: Path, domain: str, keep_previous: int,
         legacy_wildcard: bool = False) -> Plan:
    if not _domain_valid(domain) or keep_previous < 0:
        raise PruneError("invalid domain or keep_previous value")
    _check_path(root, stat.S_IFDIR)
    root = root.resolve(strict=True)
    certificates = root / "certs"
    directory = _domain_directory(certificates, domain, legacy_wildcard)
    _check_path(certificates, stat.S_IFDIR)
    _check_path(directory, stat.S_IFDIR)
    current = _current(directory)
    versions: list[Generation] = []
    with os.scandir(directory) as entries:
        for entry in entries:
            if GENERATION.fullmatch(entry.name) is not None:
                versions.append(_generation(Path(entry.path)))
    active = next((version for version in versions if version.name == current), None)
    if active is None or not active.complete:
        raise PruneError("current does not name a complete generation")
    older = sorted(
        (version for version in versions if version.name != current and version.complete),
        key=lambda version: (-version.modified_ns, version.name),
    )
    retained = (active, *older[:keep_previous])
    removable = tuple(sorted(
        (*older[keep_previous:],
         *(version for version in versions if not version.complete)),
        key=lambda version: version.name,
    ))
    return Plan(current, retained, removable)


def _lock(handle: object) -> None:
    if os.name == "nt":
        import msvcrt

        handle.seek(0)
        msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
    else:
        import fcntl

        fcntl.lockf(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)


def _sync_directory(directory: Path) -> None:
    if os.name != "nt":
        flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0)
        descriptor = os.open(directory, flags)
        try:
            os.fsync(descriptor)
        finally:
            os.close(descriptor)


def prune(root: Path, domain: str, keep_previous: int, apply: bool,
          legacy_wildcard: bool = False) -> Plan:
    if not _domain_valid(domain):
        raise PruneError("invalid domain")
    _check_path(root, stat.S_IFDIR)
    root = root.resolve(strict=True)
    certificates = root / "certs"
    directory = _domain_directory(certificates, domain, legacy_wildcard)
    _check_path(certificates, stat.S_IFDIR)
    _check_path(directory, stat.S_IFDIR)
    lock_path = directory / "current.lock"
    _check_path(lock_path, stat.S_IFREG)
    with lock_path.open("r+b", buffering=0) as handle:
        try:
            _lock(handle)
        except OSError as error:
            raise PruneError(f"store writer lock is busy: {lock_path}") from error
        result = plan(root, domain, keep_previous, legacy_wildcard)
        if apply:
            for version in result.removable:
                if _current(directory) != result.current:
                    raise PruneError("current changed during pruning")
                # Recheck every entry before unlinking; unknown files and links halt.
                actual = _generation(version.path)
                if actual.files != version.files:
                    raise PruneError(f"generation changed during pruning: {version.path}")
                for name in actual.files:
                    (version.path / name).unlink()
                version.path.rmdir()
                _sync_directory(directory)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--domain", required=True)
    parser.add_argument("--keep-previous", type=int, default=1)
    parser.add_argument(
        "--legacy-wildcard", action="store_true",
        help="select the old literal-* wildcard directory on POSIX",
    )
    parser.add_argument(
        "--apply", action="store_true",
        help="remove candidates; use only after all readers and writers have stopped",
    )
    args = parser.parse_args()
    try:
        result = prune(args.root, args.domain, args.keep_previous,
                       args.apply, args.legacy_wildcard)
    except (OSError, PruneError) as error:
        parser.exit(1, f"acme prune: {error}\n")
    mode = "removed" if args.apply else "would remove"
    for version in result.removable:
        print(f"[{mode}] {version.path}")
    print(
        f"current={result.current} retained={len(result.retained)} "
        f"{mode}={len(result.removable)}"
    )


if __name__ == "__main__":
    main()
