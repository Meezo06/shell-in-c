#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <ctype.h>

typedef unsigned char u8;
typedef unsigned short u16;

const char* const comms[] = {
	"exit", "echo", "type", "pwd", "cd"
};

const u8 ARGS_LEN = 50;
const u16 PATHS_LEN = 512;

void echo(const char* const args[]);
char* type(const char* const comm, bool show);
bool execute(const char* const args[]);
void cd(char* const dir, char* cwd);
void trans_line(char* args[], char line[]);

int main(int argc, char *argv[]) {
	// Flush after every printf
	setbuf(stdout, NULL);
	char *cwd = malloc(PATHS_LEN);
	getcwd(cwd, PATHS_LEN);


	while (true) {
		printf("$ ");
		char line[4096];
		char* args[ARGS_LEN];
		fgets(line, 4096, stdin);
		line[strcspn(line, "\n")] = '\0';
		trans_line(args, line);	
		if (strcmp(args[0], "exit") == 0 || args[0] == NULL) break;
		else if (strcmp(args[0], "echo") == 0) echo((const char * const *)args);
		else if (strcmp(args[0], "type") == 0) type(args[1], true);
		else if (strcmp(args[0], "pwd") == 0) puts(cwd);
		else if (strcmp(args[0], "cd") == 0) cd(args[1], cwd);
		else {
			bool status = execute((const char * const*)args);
			if (!status) printf("%s: command not found\n", args[0]);
		}
	}

	return 0;
}

void echo(const char* const args[]) {
	if (args[0] == NULL) return;
	for(u8 i = 1; args[i] != NULL; i++) printf("%s ", args[i]);
	putc('\n', stdout);
}

char* type(const char const *comm, bool show) {
	u8 comms_len = sizeof(comms) / sizeof(comms[0]);
	for (u16 i = 0; i < comms_len; i++) {
		if (strcmp(comms[i], comm) == 0) {
			printf("%s is a shell builtin\n", comm);
			return NULL;
		}
	}
	char *PATH = strdup(getenv("PATH"));
	const char *del = ":";
	char *full_path = malloc(PATHS_LEN);
	if (full_path == NULL) return NULL;
	for (char *dir = strtok(PATH, del); dir != NULL; dir = strtok(NULL, del)) {
		DIR *curr_dir = opendir(dir);
		if (curr_dir == NULL) continue;
		struct dirent *file;
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
	if (show) printf("%s: not found\n", comm);
	free(PATH);
	return NULL;
}

void cd(char* const dir, char* cwd) {
	const char* home = getenv("HOME");
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
		printf("cd: %s: No such file or directory\n", dir);
		return;
	}
	memcpy(cwd, dir_full_path, PATHS_LEN);
	closedir(dir_f);
}

bool execute(const char* const args[]) {
    char *path = type(args[0], false); // type is a custom function that returns the path of the command
	if (path == NULL) return false;

	pid_t pid = fork();

	if (pid == -1) {
		printf("Error executing\n");
		return false;
	}
	else if (pid == 0) {
		execv(path, (char * const*) args);
		perror("Execution Failed\n");
		exit(EXIT_FAILURE);
	}
	else {
		int status;
		waitpid(pid, &status, 0); 
	}
	return true;
}


void trans_line(char* args[], char line[]) {
	char* command = strtok(line, " ");
	args[0] = command;
	if (command == NULL) return;
	u8 i = 1;
	char* tmp = line + strlen(command);
	*tmp = ' ';
	char* start = tmp;
	char* end = start;
	while (*end != '\0') {
		while (isspace(*end)) *end++ = '\0';
		if (*end == '\0') break;
		start = end;
		while (!isspace(*end)) {
			if (*end == '\\') {
				if (end - start == 0) {
				       	start++;
					end += 2;
					continue;
				}
				*end++ = '\0';
				strcat(start, end);
				continue;
			}
			if ((*end == '\'' || *end == '"') && end - start == 0) {
				char delim = *end;
				start++;
				while (*++end != delim) if (*end == '\0') {
					printf("Format error\n");
					return;
				}
				*end++ = '\0';
				if (!isspace(*end)) strcat(start, end--);
			}
			else if (*end == '\'' || *end == '"') {
				char delim = *end;
				*end = '\0';
				char* tmp_s = end + 1;
				while (*++end != delim) if (*end == '\0') {
					printf("Format error\n");
					args[0] = NULL;
					return;
				}
				*end++ = '\0';
				strcat(start, tmp_s);
				if (!isspace(*end)) {
					strcat(start, end);
					end -= 2;
					*(end + strlen(end) + 1) = '\0';
				}
			}
			else {
				if (*end++ == '\0') break;
			}
		}
		args[i++] = start;
	}
	args[i] = NULL;
}
