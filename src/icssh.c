#include "icssh.h"
#include "linkedlist.h"
#include <readline/readline.h>
#include "helpers.h"
#include <signal.h>

volatile sig_atomic_t color = 31;
volatile sig_atomic_t sigchld_received = 0; // flag set by SIGCHLD handler
list_t* bg_list = NULL; // linked list of bg jobs
char username[256]; // cached username for sig handler

// SIGCHLD handler, sets flag when child process terminates
void sigchld_handler(int sig) {
	(void)sig;
	sigchld_received = 1;
}

// SIGUSR2 handler, prints colored greeting
void sigusr2_handler(int sig) {
	(void)sig;	

	// build the msg string
	char msg[512];
	char color_str[3];
	int idx = 0;
	int i;

	// convert color to str (31 - 36)
	color_str[0] = '0' + (color / 10);
	color_str[1] = '0' + (color % 10);
	color_str[2] = '\0';

	// build msg "\x1B[0;"
	msg[idx++] = '\x1B';
	msg[idx++] = '[';
	msg[idx++] = '0';
	msg[idx++] = ';';

	// add color digits
	msg[idx++] = color_str[0];
	msg[idx++] = color_str[1];

	// add "mHi "
	msg[idx++] = 'm';
	msg[idx++] = 'H';
	msg[idx++] = 'i';
	msg[idx++] = ' ';

	// add username
	for (i = 0; username[i] != '\0' && idx < 500; i++) {
		msg[idx++] = username[i];
	}

	// add "!\x1B[0m\n"
	msg[idx++] = '!';
	msg[idx++] = '\x1B';
	msg[idx++] = '[';
	msg[idx++] = '0';
	msg[idx++] = 'm';
	msg[idx++] = '\n';
	msg[idx] = '\0';

	// write to STDERR using async-signal-safe write()
	write(STDERR_FILENO, msg, idx);

	// increment color and wrap around if ya need
	color++;
	if (color > 36) { color = 31; }
}

// reap all terminated bg jobs
void reap_bg_jobs() {
	int status;
	pid_t pid;

	// use WNOHANG to avoid blocking and reap all term children
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		// find/remove this job from the bg list
		node_t* current = bg_list->head;
		node_t* prev = NULL;

		while (current != NULL) {
			bgentry_t* entry = (bgentry_t*)current->data;

			if (entry->pid == pid) {
				printf(BG_TERM, pid, entry->job->line);

				// remove the above term bg job from the list
				if (prev == NULL) {
					bg_list->head = current->next;
				} else {
					prev->next = current->next;
				}
				bg_list->length--;

				delete_bgentry(entry);
				free(current);
				break;
			}

			prev = current;
			current = current->next;
		}
	}
}

int main(int argc, char* argv[]) {
    int max_bgprocs = -1;
	int exec_result;
	int exit_status;
	int last_exit_status = 0;
	pid_t pid;
	pid_t pids[10];
	pid_t wait_result;
	char* line;
#ifdef GS  // This compilation flag is for grading purposes. DO NOT REMOVE
    rl_outstream = fopen("/dev/null", "w");
#endif

    // check command line arg
    if(argc > 1) {
        int check = atoi(argv[1]);
        if(check != 0)
            max_bgprocs = check;
        else {
            printf("Invalid command line argument value\n");
            exit(EXIT_FAILURE);
        }
    }

    // cache username
    char* user = getenv("USER");
    if (user != NULL) {
	    strncpy(username, user, sizeof(username) - 1);
	    username[sizeof(username) - 1] = '\0';
    } else {
	    strncpy(username, "user", sizeof(username) - 1);
	    username[sizeof(username) - 1] = '\0';
    }

	// Setup segmentation fault handler
	if (signal(SIGSEGV, sigsegv_handler) == SIG_ERR) {
		perror("Failed to set signal handler");
		exit(EXIT_FAILURE);
	}

	// setup SIGCHLD handler for bg process termination
	if (signal(SIGCHLD, sigchld_handler) == SIG_ERR) {
		perror("Failed to set SIGCHLD handler");
		exit(EXIT_FAILURE);
	}

	// SIGUSR2 handler
	if (signal(SIGUSR2, sigusr2_handler) == SIG_ERR) {
		perror("Failed to set SIGUSR2 handler");
		exit(EXIT_FAILURE);
	}

	// init bg job list
	bg_list = CreateList(compare_bgentry, print_bgentry_wrapper, delete_bgentry);

    	// print the prompt & wait for the user to enter commands string
	while ((line = readline(SHELL_PROMPT)) != NULL) {
		// check if any bg jobs have terminated
		if (sigchld_received) {
			reap_bg_jobs();
			sigchld_received = 0;
		}

        	// MAGIC HAPPENS! Command string is parsed into a job struct
        	// Will print out error message if command string is invalid
		    job_info* job = validate_input(line);
        	if (job == NULL) { // Command was empty string or invalid
			free(line);
			continue;
		}

        	//Prints out the job linked list struture for debugging
        	#ifdef DEBUG   // If DEBUG flag removed in makefile, this will not longer print
            		debug_print_job(job);
        	#endif

		// valid redirections?
		if (!validate_redirections(job)) {
			fprintf(stderr, RD_ERR);
			free(line);
			free_job(job);
			continue;
		}

		// valid pipes and redirections?
		if (job->nproc > 1 && (job->in_file != NULL || job->out_file != NULL || (job->procs && job->procs->err_file != NULL))) {
			fprintf(stderr, PIPE_ERR);
			free(line);
			free_job(job);
			continue;
		}

		// example built-in: exit
		if (strcmp(job->procs->cmd, "exit") == 0) {
			// kill all bg jobs
			node_t* current = bg_list->head;
			while (current != NULL) {
				bgentry_t* entry = (bgentry_t*)current->data;
				
				// kill proc
				kill(entry->pid, SIGKILL);

				// print term message
				printf(BG_TERM, entry->pid, entry->job->line);

				current = current->next;
			}

			// reap all killed bg jobs
			current = bg_list->head;
			while (current != NULL) {
				bgentry_t* entry = (bgentry_t*)current->data;
				int status;
				waitpid(entry->pid, &status, 0);
				current = current->next;
			}

			// free bg_list
			while (bg_list->head != NULL) {
				node_t* temp = bg_list->head;
				bg_list->head = temp->next;
				delete_bgentry((bgentry_t*)temp->data);
				free(temp);
			}
			free(bg_list);	
			
			// Terminating the shell
			free(line);
			free_job(job);
           		validate_input(NULL);   // calling validate_input with NULL will free the memory it has allocated
            		return 0;
		}

		// built-in cd
		if (strcmp(job->procs->cmd, "cd") == 0) {
			char* target_dir;

			// if no arg provided, use HOME env variable
			if (job->procs->argc < 2 || job->procs->argv[1] == NULL) {
				target_dir = getenv("HOME");
				if (target_dir == NULL) {
					fprintf(stderr, DIR_ERR);
					free(line);
					free_job(job);
					continue;
				}
			} else {
				target_dir = job->procs->argv[1];
			}

			// attempt to changed directory
			if (chdir(target_dir) == 0) {
				// success - print absolute pathname
				char cwd[1024];
				if (getcwd(cwd, sizeof(cwd)) != NULL) {
					printf("%s\n", cwd);
				}
			} else {
				// failure
				fprintf(stderr, DIR_ERR);
			}

			free(line);
			free_job(job);
			continue;
		}

		// built-in estatus
		if (strcmp(job->procs->cmd, "estatus") == 0) {
			printf("%d\n", last_exit_status);
			free(line);
			free_job(job);
			continue;
		}

		// built-in bg_list
		if (strcmp(job->procs->cmd, "bglist") == 0) {
			// print from oldest to newest
			PrintLinkedList(bg_list, stderr, "");
			free(line);
			free_job(job);
			continue;
		}

		// built-in fg
		if (strcmp(job->procs->cmd, "fg") == 0) {
			pid_t target_pid = -1;
			bgentry_t* target_entry = NULL;
			node_t* target_node = NULL;
			node_t* prev = NULL;

			// any bg jobs?
			if (bg_list->length == 0) {
				fprintf(stderr, PID_ERR);
				free(line);
				free_job(job);
				continue;
			}

			// which job to bring to fg?
			if (job->procs->argc < 2) {
				// no arg, then get last in the list
				node_t* current = bg_list->head;
				while (current != NULL) {
					if (current->next == NULL) {
						// found the last job (most recent)
						target_node = current;
						target_entry = (bgentry_t*)current->data;
						target_pid = target_entry->pid;
						break;
					}
					prev = current;
					current = current->next;
				}	
			} else {
				// arg provided woohooo, find it
				target_pid = atoi(job->procs->argv[1]);
				node_t* current = bg_list->head;

				while(current != NULL) {
					bgentry_t* entry = (bgentry_t*)current->data;
					if (entry->pid == target_pid) {
						target_node = current;
						target_entry = entry;
						break;
					}
					prev = current;
					current = current->next;
				}

				if (target_entry == NULL) {
					// pid not found anywhere
					fprintf(stderr, PID_ERR);
					free(line);
					free_job(job);
					continue;
				}
			}

			// print the cl
			printf("%s\n", target_entry->job->line);

			// wait for proc to finish
			int fg_status;
			pid_t wait_result = waitpid(target_pid, &fg_status, 0);
			if (wait_result < 0) {
				perror("waitpid");
			} else {
				// update exit status
				if (WIFEXITED(fg_status)) {
					last_exit_status = WEXITSTATUS(fg_status);
				} else if (WIFSIGNALED(fg_status)) {
					last_exit_status = 128 + WTERMSIG(fg_status);
				} else {
					last_exit_status = fg_status;
				}
			}

			// remove it from the bg_list
			if (prev == NULL) {
				bg_list->head = target_node->next;
			} else {
				prev->next = target_node->next;
			}
			bg_list->length--;

			// free the bgentry
			delete_bgentry(target_entry);
			free(target_node);

			free(line);
			free_job(job);
			continue;
		}

	if (job->nproc == 1) {
		// example of good error handling!
        // create the child proccess
		if ((pid = fork()) < 0) {
			perror("fork error");
			exit(EXIT_FAILURE);
		}
		if (pid == 0) {  //If zero, then it's the child process
            //get the first command in the job list to execute
		    proc_info* proc = job->procs;

		    // I/O Redirection
		    
		    // input redirection: '<'
		    if (job->in_file != NULL) {
			    int fd_in = open(job->in_file, O_RDONLY);
			    if (fd_in < 0) {
				    fprintf(stderr, RD_ERR);
				    free_job(job);
				    free(line);
				    validate_input(NULL);
				    exit(EXIT_FAILURE);
			    }
			    dup2(fd_in, STDIN_FILENO);
			    close(fd_in);
		    }

		    // output redirection: '>'
		    if (job->out_file != NULL) {
			    int fd_out = open(job->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			    if (fd_out < 0) {
				    fprintf(stderr, RD_ERR);
				    free_job(job);
				    free(line);
				    validate_input(NULL);
				    exit(EXIT_FAILURE);
			    }
			    dup2(fd_out, STDOUT_FILENO);
			    close(fd_out);
		    }

		    // err redirection: '2>'
		    if (proc->err_file != NULL) {
			    int fd_err = open(proc->err_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			    if (fd_err < 0) {
				    fprintf(stderr, RD_ERR);
				    free_job(job);
				    free(line);
				    validate_input(NULL);
				    exit(EXIT_FAILURE);
			    }
			    dup2(fd_err, STDERR_FILENO);
			    close(fd_err);
		    }

			exec_result = execvp(proc->cmd, proc->argv);
			if (exec_result < 0) {  //Error checking
				printf(EXEC_ERR, proc->cmd);
				
				// Cleaning up to make Valgrind happy 
				// (not technically necessary because resources will be released when reaped by parent)
				free_job(job);  
				free(line);
        			validate_input(NULL);  // calling validate_input with NULL will free the memory it has allocated
				exit(EXIT_FAILURE);
			}
		}	
	} else {
		// processes with pipes
		int num_pipes = job->nproc - 1;
		int pipefds[num_pipes][2];
		
		// create all pipes
		for (int i = 0; i < num_pipes; i++) {
			if (pipe(pipefds[i]) < 0) {
				perror("pipe error");
				exit(EXIT_FAILURE);
			}
		}

		// fork children per proc
		proc_info* proc = job->procs;
		for (int i = 0; i < job->nproc; i++) {
			pids[i] = fork();

			if (pids[i] < 0) {
				perror("fork error");
				exit(EXIT_FAILURE);
			}
			
			// child proc
			if (pids[i] == 0) {
				
				// if not first proc, previous pipe read needed?
				if (i > 0) {
					dup2(pipefds[i-1][0], STDIN_FILENO);
				}

				// if not last proc, write to next pipe
				if (i < num_pipes) {
					dup2(pipefds[i][1], STDOUT_FILENO);
				}

				// close all pipe file descriptors in child
				for (int j = 0; j < num_pipes; j++) {
					close(pipefds[j][0]);
					close(pipefds[j][1]);
				}

				exec_result = execvp(proc->cmd, proc->argv);
				if (exec_result < 0) {
					printf(EXEC_ERR, proc->cmd);
					free_job(job);
					free(line);
					validate_input(NULL);
					exit(EXIT_FAILURE);
				}
			}

			//move to next proc
			if (proc->next_proc != NULL) {
				proc = proc->next_proc;
			}
		}

		// parent, close all pipe file descripts
		for (int i = 0; i < num_pipes; i++) {
			close(pipefds[i][0]);
			close(pipefds[i][1]);
		}

		// for piped cmds, use the last proc's ID
		pid = pids[job->nproc - 1];
					
	}	
		// parent proc
		if (job->bg) {
			
			// are we at capacity?
			if (max_bgprocs > 0 && bg_list->length >= max_bgprocs) {
				// kill the child we just started
				if (job->nproc == 1) {
					kill(pid, SIGKILL);
					waitpid(pid, NULL, 0);
				} else {
				// kill all the children in piped job
					int i;
					for (i = 0; i < job->nproc; i++) {
						kill(pids[i], SIGKILL);
						waitpid(pids[i], NULL, 0);
					}
				}
				
				fprintf(stderr, BG_ERR);
				free_job(job);
				free(line);
				continue;
			}

			// create bg entry for the job
			bgentry_t* bg_entry = malloc(sizeof(bgentry_t));
			bg_entry->job = job;
			bg_entry->pid = pid;
			bg_entry->seconds = time(NULL);

			// insert in order by time
			InsertInOrder(bg_list, bg_entry);

			free(line);
			// not freeing jobs for the bg sake, stored in bgentry
		} else {

			// fg job, wait for everything to finish
			if (job->nproc == 1) {

				// As the parent, wait for the foreground job to finish
				wait_result = waitpid(pid, &exit_status, 0);
				if (wait_result < 0) {
					printf(WAIT_ERR);
					exit(EXIT_FAILURE);
				}

				// store exit status for 'estatus'
				if (WIFEXITED(exit_status)) {
					last_exit_status = WEXITSTATUS(exit_status);
					#ifdef DEBUG // delete later
					fprintf(stderr, "Child exited normally with status: %d\n", last_exit_status); // delete later
					#endif // delete later
				} else if (WIFSIGNALED(exit_status)) { // delete later
					last_exit_status = 128 + WTERMSIG(exit_status); // delete later
					#ifdef DEBUG // delete later
					fprintf(stderr, "Child terminated by signal: %d, storing: %d\n", // delete later
							WTERMSIG(exit_status), last_exit_status); // delete later
					#endif // delete later
				} else {
					// raw status
					last_exit_status = exit_status;
					#ifdef DEBUG // delete later
					fprintf(stderr, "Child terminated abnormally, raw status: %d\n", exit_status); // delete later
					#endif // delete later
				}
			} else {
				// multiple procs so wait for all but use the last proc's exit status
				int i;
				for (i = 0; i < job->nproc; i++) {
					wait_result = waitpid(pids[i], &exit_status, 0);
					if (wait_result < 0) {
						printf(WAIT_ERR);
						exit(EXIT_FAILURE);
					}

					// only store the exit status from the last proc
					if (i == job->nproc - 1) {
						if (WIFEXITED(exit_status)) {
							last_exit_status = WEXITSTATUS(exit_status);
							#ifdef DEBUG
							fprintf(stderr, "Last piped child exited with status: %d\n", last_exit_status);
							#endif
						} else if (WIFSIGNALED(exit_status)) {
							last_exit_status = 128 + WTERMSIG(exit_status);
						} else {
							last_exit_status = exit_status;
						}
					}
				}
			}
			free_job(job);  // if a foreground job, we no longer need the data
			free(line);
		}
	}
    	// calling validate_input with NULL will free the memory it has allocated
    	validate_input(NULL);

#ifndef GS
	fclose(rl_outstream);
#endif
	return 0;
}
