"""Check uptime and awake time for the Redis and MQTT mixed-load journals."""
from __future__ import annotations

import ctypes
from ctypes import wintypes
import math
import sys
import time

CLOCK_BASIS = "boot-and-active"
FIRST_WINDOW_SECONDS = 25
MAX_GAP_SECONDS = 15
MAX_CLOCK_DELTA_SECONDS = 5
MAX_SUSPEND_SECONDS = 0.25


def reference_clock():
    if sys.platform == "win32":
        kernel = ctypes.WinDLL("kernel32")
        boot = kernel.GetTickCount64
        boot.argtypes = ()
        boot.restype = ctypes.c_uint64
        active = kernel.QueryUnbiasedInterruptTime
        active.argtypes = (ctypes.POINTER(ctypes.c_uint64),)
        active.restype = wintypes.BOOL
        ticks = ctypes.c_uint64()

        def read():
            uptime = boot() / 1_000
            if not active(ctypes.byref(ticks)):
                raise OSError("cannot read Windows awake time")
            return uptime, ticks.value / 10_000_000

        return read
    if sys.platform == "linux":
        return lambda: (time.clock_gettime(time.CLOCK_BOOTTIME), time.clock_gettime(time.CLOCK_MONOTONIC))
    raise ValueError("mixed-load continuity requires Windows or Linux")


def validate_clock(current, previous, duration=None):
    elapsed = current["monotonic_elapsed_seconds"]
    boot = current["boot_elapsed_seconds"]
    active = current["active_elapsed_seconds"]
    wall = current["wall_elapsed_seconds"]
    offset = current["offset_seconds"]
    if not (all(math.isfinite(value) and value >= 0 for value in (elapsed, boot, active)) and
            math.isfinite(wall) and math.isclose(wall - elapsed, offset, rel_tol=0, abs_tol=1e-6)):
        raise ValueError("invalid journal clock")
    if abs(boot - active) > MAX_SUSPEND_SECONDS:
        raise ValueError("host suspend detected")
    if abs(active - elapsed) >= MAX_CLOCK_DELTA_SECONDS:
        raise ValueError("active clock diverged")
    if previous is None:
        if max(elapsed, boot, active) > FIRST_WINDOW_SECONDS:
            raise ValueError("initial journal observation gap")
    else:
        gap = elapsed - previous["monotonic_elapsed_seconds"]
        if not (0 <= gap < MAX_GAP_SECONDS and
                math.isclose(gap, current["tick_gap_seconds"], rel_tol=0, abs_tol=1e-6) and
                0 <= boot - previous["boot_elapsed_seconds"] < MAX_GAP_SECONDS and
                0 <= active - previous["active_elapsed_seconds"] < MAX_GAP_SECONDS):
            raise ValueError("journal observation gap")
    if duration is not None and active < duration:
        raise ValueError("incomplete awake duration")


class WorkloadClock:
    def __init__(self):
        self.reference = reference_clock()
        self.started = time.perf_counter()
        self.wall = time.time()
        self.boot, self.active = self.reference()
        self.previous = None
        self.failure = None
        self.largest_gap = 0

    def sample(self, duration=None):
        elapsed = time.perf_counter() - self.started
        wall = time.time() - self.wall
        boot, active = self.reference()
        current = dict(monotonic_elapsed_seconds=elapsed, wall_elapsed_seconds=wall,
                       offset_seconds=wall - elapsed, boot_elapsed_seconds=boot - self.boot,
                       active_elapsed_seconds=active - self.active)
        if self.previous is not None:
            current["tick_gap_seconds"] = elapsed - self.previous["monotonic_elapsed_seconds"]
        try:
            validate_clock(current, self.previous, duration)
        except ValueError:
            self.failure = current
            raise
        self.previous = current
        self.largest_gap = max(self.largest_gap, current.get("tick_gap_seconds", 0))
        return current
