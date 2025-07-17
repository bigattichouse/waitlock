#ifndef WAITLOCK_LOCK_H
#define WAITLOCK_LOCK_H

#include "../waitlock.h"

/* Lock management functions */
char* find_lock_directory(void);
int acquire_lock(const char *descriptor, int max_holders, double timeout);
void release_lock(void);
int check_lock(const char *descriptor);
int list_locks(output_format_t format, bool show_all, bool stale_only);
int portable_lock(int fd, int operation);

/* Text fallback format functions */
int write_text_lock_file(const char *path, const struct lock_info *info);
int read_text_lock_file(const char *path, struct lock_info *info);
int read_lock_file_any_format(const char *path, struct lock_info *info);

#endif /* WAITLOCK_LOCK_H */