#include "helpers.h"
#include "icssh.h"
#include <time.h>

// sorts by time (oldest to newest)
int compare_bgentry(void* a, void* b) {
	if (a == NULL || b == NULL) {
		return 0xBEEFCAFE;
	}

	bgentry_t* entry_a = (bgentry_t*)a;
	bgentry_t* entry_b = (bgentry_t*)b;

	if (entry_a->seconds < entry_b->seconds) {
		return -1;
	} else if (entry_a->seconds > entry_b->seconds) {
		return 1;
	}
	return 0;
}

// frees allocated memory
void delete_bgentry(void* entry) {
       if (entry == NULL) return;

       bgentry_t* bg_entry = (bgentry_t*)entry;
	
	// free job_info struct
	if (bg_entry->job != NULL) {
		free_job(bg_entry->job);
	}

	free(bg_entry);
}

// adapts print_bgentry to linked list
void print_bgentry_wrapper(void* entry, void* fp, char* sep) {
	if (entry == NULL) return;
	print_bgentry((bgentry_t*)entry);
}

// validate redirection
int validate_redirections(job_info* job) {
	if (job == NULL) return 1;

	char* in_file = job->in_file;
	char* out_file = job->out_file;
	char* err_file = job->procs ? job->procs->err_file : NULL;

	// same file used for multiple redirections?
	if (in_file != NULL && out_file != NULL) {
		if (strcmp(in_file, out_file) == 0) {
			return 0;
		}
	}

	if (in_file != NULL && err_file != NULL) {
		if (strcmp(in_file, err_file) == 0) {
			return 0;
		}
	}

	if (out_file != NULL && err_file != NULL) {
		if (strcmp(out_file, err_file) == 0) {
			return 0;
		}
	}

	return 1;
}
