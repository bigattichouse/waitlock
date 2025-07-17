#ifndef WAITLOCK_CHECKSUM_H
#define WAITLOCK_CHECKSUM_H

#include "../waitlock.h"

/* CRC32 checksum functions for lock file integrity */
uint32_t calculate_crc32(const void *data, size_t len);
uint32_t calculate_lock_checksum(const struct lock_info *info);
bool validate_lock_checksum(const struct lock_info *info);

#endif /* WAITLOCK_CHECKSUM_H */