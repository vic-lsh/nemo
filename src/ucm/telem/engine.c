#include "engine.h"

#include <string.h>

#include "config.h"
#include "telem/handler/cxl.h"
#include "telem/handler/pebs.h"
#include "telem/handler/pebs_printer.h"
#include "telem/source/cxl.h"
#include "telem/source/pebs.h"
#include "util/timer.h"

typedef struct {
    cxl_counter_update_handler_t cxl;
    pebs_sample_handler_t pebs;
} telem_handlers_t;

telem_handlers_t g_telem_handlers;

#ifdef CONFIG_PEBS
__maybe_unused static void poll_pebs(telem_poll_ctx_t* ctx,
                                     telem_handlers_t* handlers) {
    TIME_OP(ctx->pebs, { pebs_epoch_scan(handlers->pebs); });
}
#endif

__maybe_unused static void poll_cxl(telem_poll_ctx_t* ctx,
                                    telem_handlers_t* handlers) {
    TIME_OP(ctx->cxl, { cxl_epoch_scan(handlers->cxl); });
}

static void telem_engine_poll_internal(telem_poll_ctx_t* ctx,
                                       telem_handlers_t* handlers) {
#ifdef CONFIG_PEBS
    poll_pebs(ctx, handlers);
#endif
#ifdef CONFIG_CXL_TELEM
    poll_cxl(ctx, handlers);
#endif
}

static void ctx_reset(telem_poll_ctx_t* ctx) { memset(ctx, 0, sizeof(*ctx)); }

void telem_engine_poll(telem_poll_ctx_t* ctx) {
    ctx_reset(ctx);
    telem_engine_poll_internal(ctx, &g_telem_handlers);
}

int telem_engine_init() {
#ifdef CONFIG_PEBS
    pebs_init();

#ifdef CONFIG_PEBS_LOG
    char* log_filename = "pebs_out.csv";
    pebs_printer_handler_init(log_filename);
    g_telem_handlers.pebs = log_pebs_sample;
#else
    pebs_handler_init();
    g_telem_handlers.pebs = on_pebs_sample_received;
#endif /* CONFIG_PEBS_LOG */
#endif /* CONFIG_PEBS */

#ifdef CONFIG_CXL_TELEM
    if (cxl_init() < 0) {
        return -1;
    }
    g_telem_handlers.cxl = on_cxl_counter_update;
#endif

    return 0;
}
