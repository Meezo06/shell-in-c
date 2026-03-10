#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

typedef unsigned char u8;
typedef unsigned short u16;

// Built-in commands
const char* const comms[] = {
	"exit", "echo", "type", "pwd", "cd", "history", NULL
};

char* cwd;
const u16 PATHS_LEN = 512;
const u8 PROC_LEN = 10;
const u8 ARGS_LEN = 20;
const char doub_quo_esc[] = {'"', '\\'}; // double quotes escapeable

void run(char* args[][ARGS_LEN]);
bool execute(char* args[], bool);
void echo(char* args[]);
char* type(char* comm, bool show);
void cd(char* dir);
void trans_line(char* args[][ARGS_LEN], char line[]);
void str_shift(char* dest, char* src);
bool is_escapeable(char c);
char **character_name_completion(const char *, int, int);
char *character_name_generator(const char *, int);
void history(void);

int main(int argc, char *argv[]) {
	// Flush after every printf
	setbuf(stdout, NULL);

	// Stroing Directory
	cwd = malloc(PATHS_LEN);
	getcwd(cwd, PATHS_LEN);

	// set tab completion function
	rl_attempted_completion_function = character_name_completion;

	// begin session to use the history "readline/history" functions
	using_history();

	while (true) {
		// set up and get the command input
		char* line = readline("$ ");
		add_history(line);
		char* args[PROC_LEN][ARGS_LEN];

		trans_line(args, line); // get the args

		// match with built-in command or with executables
		if (args[0][0] == NULL) {
			free(line);
			continue;
		}
		if (strcmp(args[0][0], "exit") == 0) break;
		else run(args);
		free(line);
	}

	return 0;
}

inline void run(char* args[][ARGS_LEN]) {

	int fds[2]; // pipe file descriptors
	int tmpio[2]; // temprory I/O descriptors
	u8 proc = 0;
	bool is_last_proc = false;

	pipe(tmpio);
	dup2(0, tmpio[0]);
	dup2(1, tmpio[1]);

	while (args[proc][0] != NULL) {

		if (args[proc + 1][0] != NULL) {
			pipe(fds);
			dup2(fds[1], 1);
			is_last_proc = true;
		}
		else dup2(tmpio[1], 1);

		char* command = args[proc][0];

		if (strcmp("echo", command) == 0) echo(args[proc]);
		else if (strcmp("type", command) == 0) type(args[proc][1], true);
		else if (strcmp("pwd", command) == 0) puts(cwd);
		else if (strcmp("cd", command) == 0) cd(args[proc][1]);
		else if (strcmp("history", command) == 0) history();
		else {
			bool status = execute(args[proc], is_last_proc);
			if (!status) fprintf(stderr, "%s: command not found\n", command);
		}

		dup2(fds[0], 0);
		close(fds[0]);
		close(fds[1]);
		proc++;
	}

	dup2(tmpio[0], 0);
	close(tmpio[0]);
	close(tmpio[1]);
}

inline bool execute(char* args[], bool is_last) {

	char* path = type(args[0], false);
	if (path == NULL) return false;
	pid_t pid = fork();

	if (pid == -1) {
		fprintf(stderr, "Error executing\n");
		free(path);
		return false;
	}
	else if (pid == 0) {
		execv(path, args);
		fprintf(stderr, "Execution Failed\n");
		exit(EXIT_FAILURE);
	}

	if (is_last) {
		int status;
		waitpid(pid, &status, 0);
	}
	
	free(path);
	return true;
}

void history() {

	HIST_ENTRY** list = history_list();
	if (list == NULL) return;
	u16 i = 0;

	while (list[i] != NULL) printf("%d %s\n", i++, list[i]->line);
}

void echo(char* args[]) {
	if (args[0] == NULL) return;
	for(u8 i = 1; args[i] != NULL; i++) printf("%s ", args[i]);
	putc('\n', stdout);
}

// type to get the command executable path
char* type(char* comm, bool show) {
	
	u8 comms_len = sizeof(comms) / sizeof(comms[0]) - 1; // Number of commands

	for (u16 i = 0; i < comms_len; i++) {
		if (strcmp(comms[i], comm) == 0) {
			printf("%s is a shell builtin\n", comm);
			return NULL;
		}
	}

	char *PATH = strdup(getenv("PATH")); // PATH envariable
	const char *del = ":";
	char *full_path = malloc(PATHS_LEN); // To store the executable full path
	if (full_path == NULL) return NULL;

	// Iteration over every directory
	for (char *dir = strtok(PATH, del); dir != NULL; dir = strtok(NULL, del)) {
		DIR *curr_dir = opendir(dir);
		if (curr_dir == NULL) continue;
		struct dirent *file;

		// Iteration over every directory entry
		while ((file = readdir(curr_dir)) != NULL) {
			snprintf(full_path, PATHS_LEN, "%s/%s", dir, file->d_name);
			if (file->d_type == DT_REG
			&& strcmp(file->d_name, comm) == 0
			&& access(full_path, X_OK) == 0)
			{        		
				if (show) {
					printf("%s is %s\n", comm, full_path);
					free(full_path);
				} 
				free(PATH);
				return full_path;
			}
		}
		closedir(curr_dir);
	}

	if (show) fprintf(stderr, "%s: not found\n", comm);
	free(PATH);
	return NULL;
}

// cd to change current directory
void cd(char* dir) {
	const char* home = getenv("HOME"); // Home directory
	char dir_full_path[PATHS_LEN];

	DIR *dir_f;
	if (*dir != '/') {
		if (*dir == '~') {
			strcpy(dir_full_path, home);
			strcat(dir_full_path, dir + 1);
		} 
		else snprintf(dir_full_path, PATHS_LEN, "%s/%s", cwd, dir);
	}
	else strcpy(dir_full_path, dir);

	realpath(dir_full_path, dir_full_path);
	dir_f = opendir(dir_full_path);
	if (dir_f == NULL) {
		fprintf(stderr, "cd: %s: No such file or directory\n", dir);
		return;
	}

	memcpy(cwd, dir_full_path, PATHS_LEN);
	closedir(dir_f);
}

// Arguments handling
void trans_line(char* args[][ARGS_LEN], char line[]) {
	args[0][0] = NULL;
	char* start = line;
	u8 i = 0;
	u8 proc = 0; // processes
	char* end = start;

	while (true) {
		// Null terminate spaces
		while (isspace(*end)) *end++ = '\0'; 
		if (*end == '\0') {
			args[proc++][i] = NULL;
			args[proc][0] = NULL;
			break;
		}

		// Iterating over argument chars
		start = end;
		while (!isspace(*end)) {

			// handling pipes
			if (*end == '|') {
				start = NULL;
				end++;
				break;
			}

			if (*end == '\\') {

				if (end - start == 0) {
					start++;
					end += 2;
					continue;
				}

				*end++ = '\0';
				str_shift(start, end);
				continue;
			}

			if ((*end == '\'' || *end == '"') && end - start == 0) {
				char delim = *end;
				start++;

				while (*++end != delim) {
				       	if (*end == '\0') {
					fprintf(stderr, "Format error\n");
					args[0][0] = NULL;
					return;
					}
					if (*end == '\\' && delim == '"' && is_escapeable(*(end + 1))) {
						*end = '\0';
						str_shift(end, end + 1);
					}
				}
				*end++ = '\0';
				if (!isspace(*end)) str_shift(start, end--);
			}
			else if (*end == '\'' || *end == '"') {
				char delim = *end;
				*end = '\0';
				char* tmp_s = end + 1;

				while (*++end != delim) { 
					if (*end == '\0') {
					fprintf(stderr, "Format error\n");
					args[0][0] = NULL;
					return;
					}
					if (*end == '\\' && delim == '"' && is_escapeable(*(end + 1))) {
						*end = '\0';
						str_shift(end++, end);
					}
				}

				*end++ = '\0';
				str_shift(start, tmp_s);

				if (!isspace(*end)) {
					str_shift(start, end);
					end -= 2;
					*(end + strlen(end) + 1) = '\0';
				}

			}
			else {
				if (*end++ == '\0') break;
			}
		}

		args[proc][i++] = start;

		if (start == NULL) {
			i = 0;
			proc++;
		}
	}

}

// custom strcat() for overlapped strings
void str_shift(char* dest, char* src) {
	u16 offset = strlen(src);
	dest += strlen(dest);
	memmove(dest, src, offset);
	*(dest + offset) = *(dest + offset + 1) = '\0';
}

bool is_escapeable(char c) {
	for (int i = 0; i < 2; i++) if (c == doub_quo_esc[i]) return true;
	return false;
}


// Tab completion list
char **character_name_completion(const char *text, int start, int end) {
	rl_attempted_completion_over = 1; // prevnet default behavior 
	return rl_completion_matches(text, character_name_generator); // find matches
}


// Tab completoin match
char *character_name_generator(const char *text, int state) {
	char *name;
	static char* path_env;
	static char* curr_dir;
	static DIR* dir;
	static u8 list_index, len;

	if (!state) {
		list_index = 0;
		len = strlen(text);
		path_env = strdup(getenv("PATH"));
		curr_dir = strtok(path_env, ":");
		dir = opendir(curr_dir);
	}

	while (true) {
		name = comms[list_index];
		if (name == NULL) break;
		list_index++;
		if (strncmp(name, text, len) == 0) return strdup(name);
	}

	while (true) {
		
		struct dirent* dir_ent;
		while (dir != NULL  && (dir_ent = readdir(dir)) != NULL) {
			char exe_path[PATHS_LEN];
			snprintf(exe_path, PATHS_LEN, "%s/%s", curr_dir, dir_ent->d_name);

			if (dir_ent->d_type == DT_REG
			&& strncmp(text, dir_ent->d_name, len) == 0
			&& access(exe_path, X_OK) == 0) {
				char* exe_name = strdup(dir_ent->d_name);
				return exe_name;
			}

		}

		closedir(dir);
		curr_dir = strtok(NULL, ":");
		if (curr_dir == NULL) break;
		dir = opendir(curr_dir);
	}

	free(path_env);
	return NULL;
}
