#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

uint64_t divup(uint64_t a, uint64_t b) {
    return (a + b - 1) / b;
}

// start, end inclusive
uint64_t get_bits(char* b, uint64_t end, uint64_t start) {
    if (start > end) {
        perror("oh no1");
        exit(1);
    }
    uint64_t len = end - start + 1;
    if (len > 64) {
        perror("oh no2");
        exit(1);
    }
    uint64_t ret;
    char *ret_bytes = (char*)&ret;
    for (int i = 0; i < 8; i++) ret_bytes[i] = 0;
    uint64_t start_byte_index = start / 8;
    uint64_t start_bit_offset = start % 8;
    uint64_t end_byte_index = divup(end, 8);
    uint64_t i = 0;
    for (uint64_t byte = start_byte_index; byte < end_byte_index; byte++) {
        ret_bytes[i] = b[byte];
        i++;
    }
    if (len == 64) return ret >> start_bit_offset;
    return (ret >> start_bit_offset) & ((1ul << (len)) - 1);
}
// 'b1010_1100_0111 at [9:3] => 'b10_1100_0
int main() {
    uint32_t bytes_[4] = {0x12345678, 0xabcdef0a, 0x1234abcd, 0xdeadbeef};
    char* bytes = (char*)&bytes_[0];
    printf("%x\n", bytes[0]);
    printf("%lx\n", get_bits(bytes, 100, 40));
}
