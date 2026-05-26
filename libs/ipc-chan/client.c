#include "ipc-chan/client.h"

#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#include "ipc-chan/common.h"

static int make_client(enum channel_type chan_ty,
                       struct sockaddr_un *cl_addr_out) {
    static bool srand_init = false;
    if (!srand_init) {
        srand_init = true;
        srand(time(NULL));
    }

    int client_fd = -1;

    const int kMaxAttempts = 3;
    int attemped = 0;

    // NOTE: we allow multiple attempts to connect because we see that the app
    // sometimes initiates multiple connections with the ucm.
    //
    // For example, this happens when interposing on a docker container, which
    // entails the following steps:
    //
    // 1. the container runs `runc` (which we interpose on)
    //    - `runc` connects to ucm
    // 2. `runc` does `execve` to change to executing the actual entrypoint
    //    program of the container
    //    - the newly `execve`d process also attempts to connect
    //
    // We should clean up the old connection (TODO: not sure how). For now,
    // we just reconnect with a different client id.
    while (attemped++ < kMaxAttempts) {
        /* unix socket client */
        client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (client_fd < 0) {
            perror("socket");
            return -1;
        }

        memset(cl_addr_out, 0, sizeof(*cl_addr_out));

        int client_id = rand();
        cl_addr_out->sun_family = AF_UNIX;
        snprintf(&cl_addr_out->sun_path[1], sizeof(cl_addr_out->sun_path) - 1,
                 "client-%d-%d-%d", chan_ty, getpid(), client_id);

        LOG("connecting at socket addr %s\n", &cl_addr_out->sun_path[1]);

        int ret = bind(client_fd, (struct sockaddr *)cl_addr_out,
                       sizeof(*cl_addr_out));
        if (ret < 0) {
            perror("client connect");
            close(client_fd);
            continue;
        }

        return client_fd;
    }

    LOG_ERR("tried connecting %d times; no go \n", kMaxAttempts);

    return -1;
}

static int make_server_conn(int client_fd, const char *chan_socket_path) {
    const int kMaxAttempts = 10;
    const int kRetryIntervalSecs = 1;

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sun_family = AF_UNIX;
    snprintf(server_addr.sun_path, sizeof(server_addr.sun_path), "%s",
             chan_socket_path);

    // NOTE: do retries to give some time for the ucm to boot up
    int attempts = 0;
    while (attempts++ < kMaxAttempts) {
        int ret = connect(client_fd, (struct sockaddr *)&server_addr,
                          sizeof(server_addr));
        if (ret < 0) {
            perror("connect to ucm");
            sleep(kRetryIntervalSecs);
            continue;
        }
        LOG("connected to ucm at '%s'\n", chan_socket_path);
        return 0;
    }

    LOG_ERR("failed to make connection with the ucm; %d attempts.\n", attempts);
    return -1;
}

int channel_client_init(enum channel_type chan_ty) {
    struct sockaddr_un client_addr;

    int client_fd = make_client(chan_ty, &client_addr);
    if (client_fd < 0) {
        return -1;
    }

    int ret = make_server_conn(client_fd, CHAN_SOCKET_PATH);
    if (ret < 0) {
        close(client_fd);
        return -1;
    }

    return client_fd;
}
