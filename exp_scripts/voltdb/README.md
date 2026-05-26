# voltdb

Sequence for running voltdb:

```
./exp_scripts/voltdb/tpcc_run_server.sh       # wait until server startup complete
./exp_scripts/voltdb/tpcc_init_server.sh      # run in a separate pane
./exp_scripts/voltdb/tpcc_run_client.sh       # wait after init is done; run in a separate pane
```
