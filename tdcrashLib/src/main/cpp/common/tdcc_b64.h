//
// Created by bugliee on 2022/1/18.
//

#ifndef TDCRASH_TDCC_B64_H
#define TDCRASH_TDCC_B64_H 1

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t tdcc_b64_encode_max_len(size_t in_len);
char *tdcc_b64_encode(const uint8_t *in, size_t in_len, size_t *out_len);

size_t tdcc_b64_decode_max_len(size_t in_len);
uint8_t *tdcc_b64_decode(const char *in, size_t in_len, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif //TDCRASH_TDCC_B64_H
