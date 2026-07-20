# bitmap-comp — starting framework

Benchmark harness for the laboratory project **“Benchmarking Bitmap
Compression Methods”**. It measures how the characteristics of bitmap data —
density, clustering, run length — affect the compression ratio and the query
performance of different bitmap representations.

The infrastructure is here already: three bitmap backends, a query generator, a
benchmark driver that writes CSV, and a CMake build with no external
dependencies. **What is missing is the dataset generator — that is Task 1.**

---

## What you get and what you write

| | |
|---|---|
| given | `raw`, `roaring`, `wah` backends; `bitmap_bench`; `generate_queries`; CMake build; one small example dataset |
| you write | `tools/generate_dataset.cpp` (Task 1), the experiment sweep (Task 2), the plots and the report (Task 3) |

You are not expected to modify the backends. You may, if a measurement you want
is not exposed — but say so in the report if you do.

## Requirements

A C++17 compiler (GCC ≥ 9, Clang ≥ 10, MSVC 2019) and CMake ≥ 3.16. Nothing
else: every representation is implemented inside this repository.

## Build

```
scripts/build.sh          # Release build into build/
```

or directly:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Always measure a **Release** build. A Debug build is 10–100× slower and does not
slow all backends down by the same factor, so Debug numbers say nothing about
the representations.

## Check that it works

```
scripts/run_example.sh
```

Generates 200 queries, runs all three backends over `data/example` with
`--verify=1`, writes one CSV per backend into `results/example/`. Expect
`0 mismatches` from every backend.

`data/example` is a single small uniform dataset. It exists so the pipeline can
be checked before Task 1 is finished, and as a worked example of the on-disk
format. It answers none of the project's questions — do not report numbers from
it.

## Repository layout

```
backends/     the three bitmap representations behind a common interface
common/       dataset I/O, query file I/O, timing, bit helpers
tools/        generate_queries (generate_dataset is yours to write)
benchmarks/   bitmap_bench — the example benchmark driver
scripts/      build / example run / clean
data/example  small committed dataset, format reference
```

---

## Backends

All three implement `bench::BitmapBackend` (`backends/bitmap_backend.hpp`).
`build()` receives the whole dataset uncompressed and converts it; `query_and()`
and `query_or()` then operate **in the backend's own representation** and return
a still-compressed result. Decoding that result back to plain words happens
outside the timed region, so `query_time_ns` measures the operation and not the
decompression.

**`raw`** — one packed `uint64_t` array per bitmap, no compression. AND/OR are
word loops over the full length. It is the baseline for the compression ratio
and the correctness oracle: under `--verify=1` every other backend's answer is
compared against it bit for bit.

**`roaring`** — Roaring bitmaps. The universe is split into chunks of 2¹⁶ bits;
each non-empty chunk is stored in whichever of three encodings is smallest —
sorted 16-bit *array* (2 bytes per bit set), flat *bitset* (8192 bytes), or
*run* list of intervals (4 bytes per run). AND/OR are performed chunk by chunk,
and a chunk absent from one operand can be skipped entirely.

**`wah`** — Word-Aligned Hybrid run-length compression. Bits are cut into groups
of 63; each 64-bit code word is either one group verbatim (*literal*) or a count
of consecutive all-zero / all-one groups (*fill*). AND/OR walk the two
compressed streams in one pass, consuming whole fills at a time.

A note you may find useful for the report: `roaring` and `wah` are compact
implementations written for this project, not the reference libraries
(CRoaring, EWAHBoolArray). They follow the published designs and the same
serialized layouts, but they are portable scalar C++ with no SIMD, and they take
two documented shortcuts:

* Roaring re-picks the smallest encoding after *every* operation, where CRoaring
  converts to run encoding only on an explicit `runOptimize()` call;
* an operation between a run container and an array/bitset container expands
  both sides to a bitset instead of using a dedicated algorithm (`container_and`
  / `container_or` in `backends/roaring_bitmap.cpp`).

Absolute timings are therefore slower than a production library's. The *shape*
of the curves — which data characteristic helps or hurts which representation —
is what this framework is built to show.

## Dataset format (what Task 1 must produce)

A dataset is a directory:

```
metadata.txt
bitmap_000.bin
bitmap_001.bin
...
```

`metadata.txt` is `key=value`, one per line:

| key | required | meaning |
|---|---|---|
| `num_bits` | yes | length of every bitmap, in bits |
| `num_bitmaps` | yes | how many `bitmap_NNN.bin` files follow |
| `word_bits` | yes | must be `64` |
| `format` | yes | must be `raw_uint64_le` |
| `kind` | no | name of the distribution: `uniform`, `sparse`, `clustered`, … |
| `params` | no | free-form description of the generator parameters |
| `density` | no | fraction of set bits |
| `seed` | no | RNG seed |

`kind`, `params`, `density` and `seed` are copied into every CSV row, which is
how you group runs during analysis — fill them in. Unknown keys are ignored, so
you can record more if you want.

Each `bitmap_NNN.bin` holds `ceil(num_bits / 64)` little-endian `uint64_t` words;
bit *b* lives in word *b*/64 at position *b*%64. **Bits past `num_bits` in the
last word must be zero** — otherwise two representations of the same set can
disagree and `--verify=1` will report mismatches that are your generator's fault.

`common/bitmap_io.hpp` already provides `write_metadata()`, `write_bitmap()` and
`bitmap_filename()`, so a generator only has to fill in the words. Add your
target to `CMakeLists.txt` (there is a comment marking the spot).

Parameters must come from the command line — the existing tools show the
convention (`--out=`, `--num-bits=`, …) but nothing forces you to follow it.

## Query format

One query per line: operation, number of operands, then bitmap indices.

```
AND 3 0 1 2
OR 2 3 4
```

Generate one with:

```
build/generate_queries --out=queries/q.txt --num-bitmaps=16 \
    --num-queries=200 --min-width=2 --max-width=5 --seed=43
```

Query width (the number of operands) is a parameter worth varying: it changes
how much of the cost is per-operation overhead versus per-bit work. Watch
`result_cardinality` when you do — on sparse data a wide AND is empty, and the
time it takes says more about how fast each representation discovers that than
about intersection itself.

## Running the benchmark

```
build/bitmap_bench --backend=roaring --dataset=data/mydata \
    --queries=queries/q.txt --out=results/roaring.csv \
    --repetitions=5 --warmup=1 --verify=0
```

| flag | meaning |
|---|---|
| `--backend=` | `raw`, `roaring` or `wah` |
| `--repetitions=N` | timed runs per query; every run is one CSV row |
| `--warmup=N` | untimed runs before the timed ones (cache warming) |
| `--verify=1` | check every answer against `raw`; exits non-zero on mismatch |

Run with `--verify=1` once per dataset — a wrong result is not a fast result —
then measure with `--verify=0`.

## Output CSV

One row per (query, repetition):

```
backend, dataset, kind, params, num_bits, num_bitmaps, density, seed,
query_file, query_index, query_type, query_width, repetition,
build_time_ms, query_time_ns, result_cardinality,
raw_bytes, compressed_bytes, compression_ratio
```

* `build_time_ms` — the whole `build()` call: converting the uncompressed
  dataset into this representation. Constant per run, repeated on every row.
* `query_time_ns` — one `query_and()` / `query_or()` call. Split AND from OR
  with `query_type`; they behave differently and averaging them together hides
  the effect.
* `compressed_bytes` — the size of the **whole dataset** in this
  representation, serialized: payload only, no allocator slack and no
  `std::vector` capacity overshoot. `raw_bytes` is `num_bitmaps *
  ceil(num_bits/64) * 8`.
* `compression_ratio` — `raw_bytes / compressed_bytes`. Above 1 means the
  representation is smaller than the uncompressed bitmap; **below 1 means it is
  larger**, which does happen and is worth explaining.
* `result_cardinality` — set bits in the answer; also a cheap sanity check that
  a query did what you expected.

## Measurement notes

* Report the **median** over repetitions, not the mean — one descheduled run
  skews a mean badly.
* Compare only numbers produced by the same binary on the same machine.
  `BENCH_NATIVE=1` (i.e. `-march=native`) makes binaries machine-specific.
* Close other work while measuring; a laptop on battery will throttle and the
  effect is larger than most of the differences you are looking for.
* Keep the seeds. Every number in the report should be reproducible from the
  generator parameters recorded in `metadata.txt`.
* Generated `data/`, `queries/`, `results/` and `build/` are not meant to be
  committed — the seeds and the scripts are enough to reproduce them.
