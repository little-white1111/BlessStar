#ifndef BS_APP_SDK_CONFIG_CONSUMER_H
#define BS_APP_SDK_CONFIG_CONSUMER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bs_consumer_t bs_consumer_t;

bs_consumer_t* bs_consumer_create(const char* shm_path, int efd);
int  bs_consumer_wait(bs_consumer_t* consumer, int timeout_ms);
const void* bs_consumer_get_data(bs_consumer_t* consumer, size_t* out_len);
uint64_t bs_consumer_get_version(bs_consumer_t* consumer);
typedef void (*bs_consumer_on_change_fn)(const void* data, size_t len, uint64_t version, void* userdata);
void bs_consumer_set_on_change(bs_consumer_t* consumer, bs_consumer_on_change_fn fn, void* userdata);
void bs_consumer_destroy(bs_consumer_t* consumer);

#ifdef __cplusplus
}
#endif

#endif // BS_APP_SDK_CONFIG_CONSUMER_H
