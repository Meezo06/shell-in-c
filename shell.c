#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>

const char* const comms[] = {
	"exit", "echo", "type", "pwd", "cd"
};

const unsigned short ARGS_LEN = 50;
const unsigned short PATHS_LEN = 512;

void echo(const char* const args[]);
char* type(const char* const comm, bool show);
bool execute(const char* const args[]);
void cd(char* const dir, char* cwd);


int main(int argc, char *argv[]) {
	// Flush after every printf
	setbuf(stdout, NULL);
	char *cwd = malloc(PATHS_LEN);
	getcwd(cwd, PATHS_LEN);


	while (1) {
		printf("$ ");
		char line[4096];
		char* command;
		char* args[ARGS_LEN];
		fgets(line, 4096, stdin);
		line[strcspn(line, "\n")] = '\0';
		command = strtok(line, " ");
		args[0] = command;
		int i = 1;
		while (i < ARGS_LEN && args[i - 1] != NULL) args[i++] = strtok(NULL, " "); 
		if (strcmp(command, "exit") == 0) break;
		else if (strcmp(command, "echo") == 0) echo((const char * const *)args);
		else if (strcmp(command, "type") == 0) type(args[1], true);
		else if (strcmp(command, "pwd") == 0) puts(cwd);
		else if (strcmp(command, "cd") == 0) cd(args[1], cwd);
		else {
			bool status = execute((const char * const*)args);
			if (!status) printf("%s: command not found\n", command);
		}
	}

	return 0;
}

void echo(const char* const args[]) {
	for(int i = 1; args[i] != NULL; i++) printf("%s ", args[i]);
	putc('\n', stdout);
}

char* type(const char const *comm, bool show) {
	unsigned short comms_len = sizeof(comms) / sizeof(comms[0]);
	for (int i = 0; i < comms_len; i++) {
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
