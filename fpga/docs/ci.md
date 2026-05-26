# CI

The project has continuous integration (CI) scripts to make sure every commit
builds successfully and passes lint checks.

The project uses Github actions. All CI workflows are defined in
`.github/workflows`. Refer to [Github action documentation](https://docs.github.com/en/actions)
on how Github action works.

Below, we describe how we use Github CI to automate FPGA and host-side development.

## FPGA-side CI

CI for FPGA-related tasks run on a UW server instead of on Github. Our FPGA
designs rely on proprietary licenses that only exist on internal servers.

### Compile-time checks

On each push, CI checks whether there's syntax errors. Github can be configured
to [notify](https://docs.github.com/en/actions/monitoring-and-troubleshooting-workflows/notifications-for-workflow-runs)
when a CI job fails.

### Building the project

When the implementation is ready, create a new branch with a name starting with
`build/` (e.g., `build/feature-x`). For branches starting with `build/`, CI
builds the full design on each push.

You may configure Github to notify if a build fails.

### Retrieving build output

The build result is saved on a UW server (see project settings for the server
name). See the CI build yml file for where the build results are saved.

Build result folders are named by the time of a CI run, as well as the commit
hash that triggered the CI build.

### Managing build queuees

Our on-prem CI server runs one job at a time, for now. Because builds can take
more than an hour, build CI tasks can easily queue up. Therefore, only push to
`build/*` branches if you believe design is ready for on-board testing. If a
later fix invalidates a prior commit, you may cancel an ongoing CI build from
our project's Action tab on Github.

## Host-side CI

Host-side CI runs on Github-managed servers.

On each push to the host-side project (currently under `host/`), Github checks
whether the project compiles.
