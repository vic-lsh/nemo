# Simulation

## Prerequisites

1. **Quartus + Questa.** `vsim` must be on your `PATH`. It ships under
   `<quartus_install>/questa_fse/bin/`.
2. **Questa license.** Generate one from the Intel Self-Service Licensing
   Center (this is a *Questa* license, distinct from the Quartus license).
   Set `LM_LICENSE_FILE` to its path — easiest to put in your shell rc.
3. **Quartus environment.** `run_sim.do` resolves the Altera primitive
   libraries through `$QUARTUS_ROOTDIR`. Source the Quartus environment
   script before running sim:
   ```bash
   source <quartus_install>/adm/qenv.sh
   ```
   If `QUARTUS_ROOTDIR` is unset, `run_sim.do` will abort with an
   explicit error.
4. **Intel example design checked out.** Simulation reads files listed in
   `fpga/filelist.txt`, many of which live in the Intel example design
   tree. Make sure you've completed the steps in
   [`../README.md`](../README.md) (Steps 1–3) so that
   `intel_rtile_cxl_top_0_ed/` is populated and patched.

## Running sim

`run_sim.sh` takes one argument: the Python testbench-sequence generator
to run. The script generates a `.sv` stimulus file from that Python
sequence, then invokes Questa via `run_sim.do`.

```bash
cd <project_root>/fpga
./run_sim.sh intel_rtile_cxl_top_0_ed/hardware_test_design/sim/sequences/vanilla.py
```

Available sequences live in
`intel_rtile_cxl_top_0_ed/hardware_test_design/sim/sequences/`:

- `vanilla.py` — basic Nemo pipeline pass-through.
- `cxl_controlpath.py` — exercises the CXL controlpath.
- `controlpath_translator.py` — controlpath with translation unit.
- `translation_unit.py` — translation-unit focused traffic.

Add a new sequence by dropping another Python file in that directory; it
just needs to import `TB_Translator` from `test_translator.py` and emit a
sequence to the path passed on the command line.

## Viewing results

After a run, inspect the waveform with:

```bash
./view_sim.sh        # opens vsim against the dumped .wlf
./view_wave.sh       # alternative entry point on machines with a
                     # different LM_LICENSE_FILE path
```

For interactive sim, run `vsim` directly and source `run_sim.do` from
the Questa transcript — see the `vsim -do run_sim.do -view vsim.wlf`
incantation in `view_sim.sh` for the pattern.

Happy coding!
