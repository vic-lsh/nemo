# docker support

We support docker workloads by adding our library to the spawn path of docker
containers. This requires modifying the `runc` repository to add our library
as one of the `LD_PRELOAD` libraries.

## Notes on supported versions

TL;DR: We have tested on the following `dockerd` and `containerd` versions:

```bash
$ dockerd --version
Docker version 28.1.1, build 01f442b

$ containerd --version
containerd github.com/containerd/containerd/v2 2.1.0
```

Running docker requires coordination among three systems: roughly speaking,
`dockerd` calls `containerd` which calls `runc`. Because they communicate via
RPC, they must be on binary-compatible versions to work together.

## Building and installing

### Note on toggling interposition

NOTE: we have not built support for toggling interposition on / off in `runc`.
Therefore, once our `runc` fork is installed, _all_ docker containers would
run with interposition on.

To toggle now, make a copy of the original `runc` installation. Copy the
original installation back to the `runc` installed path (typcially
`/usr/bin/runc`) if you want to disable interposition. To re-enable,
simply reinstall our fork.

### Installation instructions

You will need to build and install `runc`. In the `runc` directory:

```bash
$ make
$ sudo make install
```

Check the `runc` README for build dependencies that need to be installed.

## Running applications

### Setup mount directory

To enable communication and library-sharing with containers, we set up a
mount point for each container. Currently, this mount point is hard-coded
to be `/tmp/nemo` inside the container. Below, we assume that path `/tmp/nemo`
is used both inside and outside the container.

You need to manually copy the library files to `/tmp/nemo`. Note that the
dynamic libraries that hemem depends on also need to be copied.

```
$ ls -la /tmp/nemo
drwxr-xr-x  2 root root    4096 Jun  9 20:51 .
drwxrwxrwt 99 root root   36864 Jun 10 01:25 ..
-rwxrwxrwx  1 root root 6663072 Jun  9 16:10 libcapstone.so.4  # libsyscall_intercept dependency
-rwxrwxrwx  1 root root  299480 Jun  9 20:52 libhemem.so
-rwxrwxrwx  1 root root   83720 Jun  9 15:53 libsyscall_intercept.so.0
```

NOTES:

1. Remember to copy `libhemem.so` after recompilation. (TODO: automate this)
2. Copy the file, rather than use a symlink. Symlinks are error-prone in
   docker mounts.

### Running

If you have installed compatible `dockerd` and `containerd` versions on your
system, you should be able to start applications without manually starting
`dockerd` and `containerd`.

For ease of debugging, the steps below assume you run both `dockerd` and
`containerd` manually. Logs from these processes may help with debugging.

```bash
# 1. Start containerd (this needs sudo)
$ sudo containerd --log-level debug
$ sudo go run ./cmd/containerd --log-level debug # if on containerd clone

# 2. Start dockerd (this needs sudo) (in a separate terminal)
# data root argument is optional
$ sudo  dockerd --data-root /path/to/docker-nemo-data/ --debug

# 3. Start ucm (this needs sudo) (in a separate terminal)
$ sudo ./build/ucm # path assumes you're running from project root

# 4. Start docker container (in a separate terminal)
$ docker run \
    -v /tmp/nemo:/tmp/nemo \       # expose nemo files
    -v /dev/dax0.0:/dev/dax0.0 \   # expose DAX for memory
    -v /dev/dax1.0:/dev/dax1.0 \
    --cpuset-cpus=8 \              # optional: pin workload to CPU(s)
    --privileged --user root \     # double check if these are needed
    <docker-image-name>
```

If you see that the interposition works, you should see the PID of the container
process listed in the terminal that runs `ucm`. To know the container's PID,
do the following:

```bash
$ docker container ls         # get your container's ID
$ docker inspect -f '{{.State.Pid}}' <container-id>
```
