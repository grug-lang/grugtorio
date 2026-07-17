# grug factory

`grug-factory` is a Factorio-inspired top-down 2D simulation built to empirically measure how the actor model scales across multiple CPU cores using the [grug](https://github.com/grug-lang/grug) embedded programming language.

While `grug-bench` measures language and runtime performance, **`grug-factory` measures large-scale concurrent simulation performance.**

The C engine provides generic primitives (spawning, message mailboxes, basic rendering). All actual game logic (furnaces, belts, inserters) is written entirely in hot-reloadable `.grug` scripts.

## The Experiment

This repository answers key architecture questions for grug:
1. **Thread Scaling:** How much faster does an actor-based factory simulation become when scaling from 1 to 16 threads?
2. **Actor Overhead:** At what point does the scheduler's overhead (mailbox locking, context switching) dominate?
3. **Data Ownership (Deep Copy vs. CoW):** Because actors cannot share mutable state, sending an item from an Inserter to a Furnace requires memory boundaries. We benchmark the cost of strictly deep-copying `List` and `Dict` payloads vs. using atomic Copy-on-Write (CoW).

## Building

Requires CMake, a C11 compiler, Raylib, and the `grug-rs` shared library.

```bash
cmake -B build
cmake --build build
```

### Interactive Mode

Run the visual factory simulation to place machines, test blueprints (Ctrl+C/Ctrl+V), and observe the actor model working in real-time.

```bash
./grug-factory
```

### Headless Benchmark Mode

Run the CI-ready headless benchmarking tool. This loads a `.factory` save file, runs it for `N` ticks across different thread counts, and outputs a `profiling.json` file.

```bash
./grug-factory-bench --save ../saves/megafactory.factory --ticks 1000 --threads 1,2,4,8,16
```

## Profiling Output

The profiler outputs precise nanosecond-level telemetry:

```json
{
    "scenario": "megafactory.factory",
    "actors": 250000,
    "metrics": [
        {
            "threads": 8,
            "avg_tick_ms": 14.2,
            "grug_execution_ms": 9.1,
            "message_routing_ms": 2.4,
            "deep_copy_overhead_ms": 1.5,
            "scheduler_overhead_ms": 1.2
        }
    ]
}
```

Scripts in `/benchmarks/` automatically convert this JSON into SVG graphs to track LLVM/Actor performance regressions on every pull request.

## Modding

To add a new machine, you don't touch C. Just create a new `.grug` file in the `mods/` directory.

The engine exposes a strict API via `mod_api.json`. Because host functions (like `draw_sprite()` and `send_message()`) are compiled to grug IR ahead of time via `grug-rs`, there is zero FFI overhead when a grug actor calls them.
