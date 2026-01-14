#ifndef HELPERS_H
#define HELPERS_H

#include "icssh.h"

// sorts by time (oldest to newest)
int compare_bgentry(void* a, void* b);

// frees allocated memory
void delete_bgentry(void* entry);

// adapts print_bgentry to linked list
void print_bgentry_wrapper(void* entry, void* fp, char* sep);

// validate redirection
int validate_redirections(job_info* job);

#endif
