#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

#define KB (1 << 10)
#define MB (1 << 20)
#define GB (1 << 30)

#define RESOURCE_PATH "/sys/bus/pci/devices/0000:40:00.1/resource2"
#define BYTES_TO_READ (2 * MB)
#define START_ADDR 0x4080000000
#define DDR_SIZE (16UL * GB)
#define END_ADDR (START_ADDR + DDR_SIZE)

void print_ul_in_binary(unsigned long n) {
    int bits = sizeof(unsigned long) *
               8; // Calculate the number of bits in an unsigned long
    for (int i = bits - 1; i >= 0; i--) {
        unsigned long mask = 1UL << i; // Create a mask to isolate each bit
        putchar((n & mask)
                    ? '1'
                    : '0'); // Print '1' if the bit is set, otherwise print '0'
    }
    putchar('\n'); // Print a newline at the end
}

uint64_t divup(uint64_t a, uint64_t b) {
    return (a + b - 1) / b;
}

#define AR_S (0)
#define AR_ID (12)
#define AR_ADDR (64)
#define AR_LEN (10)
#define AR_SIZE (3)
#define AR_BURST (2)
#define AR_PROT (3)
#define AR_QOS (4)
#define AR_VALID (1)
#define AR_CACHE (4)
#define AR_LOCK (2)
#define AR_REGION (4)
#define AR_USER (6)

#define AR_S_ACC (0)
#define AR_ID_ACC (AR_S_ACC + AR_ID)
#define AR_ADDR_ACC (AR_ID_ACC + AR_ADDR)
#define AR_LEN_ACC (AR_ADDR_ACC + AR_LEN)
#define AR_SIZE_ACC (AR_LEN_ACC + AR_SIZE)
#define AR_BURST_ACC (AR_SIZE_ACC + AR_BURST)
#define AR_PROT_ACC (AR_BURST_ACC + AR_PROT)
#define AR_QOS_ACC (AR_PROT_ACC + AR_QOS)
#define AR_VALID_ACC (AR_QOS_ACC + AR_VALID)
#define AR_CACHE_ACC (AR_VALID_ACC + AR_CACHE)
#define AR_LOCK_ACC (AR_CACHE_ACC + AR_LOCK)
#define AR_REGION_ACC (AR_LOCK_ACC + AR_REGION)
#define AR_USER_ACC (AR_REGION_ACC + AR_USER)

#define ar_id(n) get_bits(n, AR_ID_ACC-1, AR_S_ACC)
#define ar_addr(n) get_bits(n, AR_ADDR_ACC-1, AR_ID_ACC)
#define ar_len(n) get_bits(n, AR_LEN_ACC-1, AR_ADDR_ACC)
#define ar_size(n) get_bits(n, AR_SIZE_ACC-1, AR_LEN_ACC)
#define ar_burst(n) get_bits(n, AR_BURST_ACC-1, AR_SIZE_ACC)
#define ar_prot(n) get_bits(n, AR_PROT_ACC-1, AR_BURST_ACC)
#define ar_qos(n) get_bits(n, AR_QOS_ACC-1, AR_PROT_ACC)
#define ar_valid(n) get_bits(n, AR_VALID_ACC-1, AR_QOS_ACC)
#define ar_cache(n) get_bits(n, AR_CACHE_ACC-1, AR_VALID_ACC)
#define ar_lock(n) get_bits(n, AR_LOCK_ACC-1, AR_CACHE_ACC)
#define ar_region(n) get_bits(n, AR_REGION_ACC-1, AR_LOCK_ACC)
#define ar_user(n) get_bits(n, AR_USER_ACC-1, AR_REGION_ACC)

//#define ar_fifo_full(n) get_bits(n, 126, 126)
//#define ar_fifo_empty_n(n) get_bits(n, 127, 127)

#define ar_fifo_full(n) (uint64_t)((n[15] & 0x40) >> 6)
#define ar_fifo_empty_n(n) (uint64_t)((n[15] & 0x80) >> 7)

char get_bit(char* in, int bit) {
    int byte = bit / 8;
    int bitoff = bit % 8;
    return (in[byte] & (1 << bitoff)) != 0;
}
void set_bit(char* in, int bit) {
    int byte = bit / 8;
    int bitoff = bit % 8;
    in[byte] |= (1 << bitoff);
}

// start, end inclusive
uint64_t get_bits(char* b, uint64_t end, uint64_t start) {
    uint64_t oldend = end;
    uint64_t oldstart = start;
    start = AR_USER_ACC-oldend-1;
    end = AR_USER_ACC-oldstart-1;
    printf("old: [%ld:%ld]\n", oldend, oldstart);
    printf("new: [%ld:%ld]\n", end, start);
    if (start > end) {
        perror("oh no1");
        exit(1);
    }
    uint64_t len = end - start + 1;
    if (len > 64) {
        perror("oh no2");
        exit(1);
    }
    uint64_t ret = 0;
    char *ret_bytes = (char*)&ret;
    for (int i = 0; i < len; i++) {
        if (get_bit(b, i+start)) {
            set_bit(ret_bytes, i);
        }
    }
    return ret;
    /*
    for (int i = 0; i < 8; i++) ret_bytes[i] = 0;
    uint64_t start_byte_index = start / 8;
    uint64_t start_bit_offset = start % 8;
    uint64_t end_byte_index = divup(end+1, 8);
    uint64_t i = 0;
    for (uint64_t byte = start_byte_index; byte < end_byte_index; byte++) {
        ret_bytes[i] = b[byte];
        i++;
    }
    if (len == 64) return ret >> start_bit_offset;
    return (ret >> start_bit_offset) & ((1ul << (len)) - 1);
    */
}

typedef struct {
    uint64_t p1;
    uint64_t p2;
} ar_entry;

ar_entry get_ar(int chan, volatile unsigned long *register_space) {
    uint64_t part1_addr = chan == 0 ? 0xaaa : 0xaba;
    uint64_t part2_addr = chan == 0 ? 0xaab : 0xabb;
    ar_entry ar;
    ar.p1 = register_space[part1_addr];
    ar.p2 = register_space[part2_addr];
    return ar;
}

void print_ar_entry(int chan, ar_entry *e) {
    char* a = (char*)e;
    printf("AR chan=%d id=%ld addr=%lx len=%ld size=%ld burst=%ld prot=%lx qos=%lx valid=%ld cache=%lx lock=%lx region=%lx user=%lx fifo_full=%ld fifo_empty_n=%ld raw=%lx %lx\n",
    chan, ar_id(a), ar_addr(a), ar_len(a), ar_size(a), ar_burst(a), ar_prot(a),
    ar_qos(a), ar_valid(a), ar_cache(a), ar_lock(a), ar_region(a), ar_user(a), ar_fifo_full(a), ar_fifo_empty_n(a),
    e->p2, e->p1);
}


int fpga_regs() {

    ar_entry e1;
    e1.p2 = 0x8005556f56df77df;
    e1.p1 = 0x77ff76fc2539e666;
    print_ar_entry(1, &e1);
    return 0;
}

int main() { fpga_regs(); }
