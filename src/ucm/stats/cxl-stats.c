#include "cxl-stats.h"

#include <stdbool.h>
#include <string.h>

#include "hemem-shared.h"
#include "telem/source/cxl.h"
#include "util/log.h"

access_counter_t last_printed_access_cnts[N_COUNTERS];

int format_uint64_t(uint64_t value, char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return -1;
    }

    // Handle zero case
    if (value == 0) {
        if (buffer_size >= 2) {
            strcpy(buffer, "0");
            return 1;
        }
        return -1;
    }

    // Convert to string without commas first
    char temp[32];  // Max digits for uint64_t is 20, plus null terminator
    int temp_len =
        snprintf(temp, sizeof(temp), "%llu", (unsigned long long)value);

    // Calculate required buffer size with commas
    size_t commas_needed = (temp_len - 1) / 3;
    size_t total_len = temp_len + commas_needed;

    if (buffer_size <= total_len) {
        return -1;  // Buffer too small
    }

    // Build the formatted string from right to left
    buffer[total_len] = '\0';
    int buf_pos = total_len - 1;
    int digit_count = 0;

    for (int i = temp_len - 1; i >= 0; i--) {
        if (digit_count == 3) {
            buffer[buf_pos--] = ',';
            digit_count = 0;
        }
        buffer[buf_pos--] = temp[i];
        digit_count++;
    }

    return total_len;
}

void cxl_stats_print() {
    LOG_STATS("CXL memory access counts (during last second, per GB):\n[");

    size_t n_gbs = N_COUNTERS / N_HUGEPAGES_IN_GB;

    const size_t pprint_buflen = 64;
    char pprint_buf[pprint_buflen];

    // print actual counts
    for (size_t i = 0; i < n_gbs; i++) {
        // bool expand = i == 0;
        bool expand = 0;
        if (i != 0 && i % 8 == 0) {
            LOG_STATS("\n ");
        }

        unsigned long long sum = 0;
        for (size_t j = 0; j < N_HUGEPAGES_IN_GB; j++) {
            size_t idx = i * N_HUGEPAGES_IN_GB + j;
            access_counter_t cnt = cxl_count_get(idx);

            access_counter_t last = last_printed_access_cnts[idx];
            // overflow-handling workaround.
            // Right now, we can't assume that the counter will overflow at
            // 2^32-1. This is because of two quirks in the counters impl:
            //
            // 1. the max counter value is 2 * 2^16, rather than 2^32.
            //
            // 2. because there's a counter per channel (2 channels total),
            //    each counter can overflow at any time, causing overflow timing
            //    inconsistency across counters.
            //
            // For now, we just take the latest counter as-is when overflow.
            access_counter_t diff = (cnt < last) ? (cnt) : (cnt - last);
            sum += diff;

            if (expand) {
                LOG_STATS("%lu,", diff);
            }

            last_printed_access_cnts[idx] = cnt;
        }

        if (expand) {
            LOG_STATS("\n");
        }

        if (expand) {
        } else {
            format_uint64_t(sum, pprint_buf, pprint_buflen);
            LOG_STATS("%10s", pprint_buf);
        }

        if (i + 1 == n_gbs) {
            LOG_STATS("]\n");
        } else {
            LOG_STATS("; ");
        }
    }
}
