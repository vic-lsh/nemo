# microbenchmarks

This directory contains microbenchmarks used to evaluate our system.

## Build

Every file within `microbenchmarks/bin` is a compilation target. The compiled
binaries can be found at `build/microbenchmarks`.

Every binary can include source files under `microbenchmarks/`. They can also
directly use shared libraries under `libs/`.
