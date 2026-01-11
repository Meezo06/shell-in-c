#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define BUFF_SIZE 1024

DIR *dir;
char *curr_dir;
char *command = NULL;
char **args = NULL;

void command_map(const char *comm);
void get_comm_and_args(const char *buff);

int main(int argc, char **argv) {

    if (argc != 2) {
        fprintf(stderr, "Wrong format. Usage: ./shell [starting directory]\n");
        return 1;
    }

    dir = opendir(argv[1]);

    // check if directory exists
    if (!dir) {
        closedir(dir);
        fprintf(stderr, "Error: Starting directory is not found\n");
        return 2;
    }

    curr_dir = argv[1];
    char *line = malloc(BUFF_SIZE);
    if (!line) {
        fprintf(stderr, "Shell: malloc failed\n");
        closedir(dir);
        return 3;
    }


    while (1) {
        printf("%s$ ", curr_dir);
        fflush(stdout);

        if (fgets(line, BUFF_SIZE, stdin) == NULL) { /* EOF or error */
            if (feof(stdin)) putchar('\n'); /* nice newline on Ctrl-D */
            break;
        }

        /* strip trailing newline */
        line[strcspn(line, "\n")] = '\0';

        get_comm_and_args(line);

        if (line[0] == '\0' || !command) continue;     /* empty line -> prompt again */
        if (strcmp(command, "exit") == 0) break;
        command_map(command);
        
    }

    free(line);
    free(command);
    free(args);
    free(curr_dir);
    closedir(dir);

    return 0;
}

void command_map(const char *comm) {
    if (!strcmp(comm, "cd")) change_dir();
}

void get_comm_and_args(const char *buff) {
    
    if (buff == NULL) return;

    const char *start, *end;

    start = buff;
    while (*start && isspace((unsigned char) *start)) start++;

    if (*start == '\0') {
        free(command);
        free(args);
       return; 
    }

    end = start;
    while (*end && !isspace((unsigned char) *end)) end++;

    size_t len = end - start + 1;
    realloc(command, len);
    if (!command) return;
    memcpy(command, start, len);
    command[len - 1] = '\0';

    unsigned short i = 0;
    while (*end != '\0') {
        start = end;
        while (*start && isspace((unsigned char) *start)) start++;
        end = start;
        while (*end && !isspace((unsigned char) *end)) end++;
        len = end - start + 1;
        realloc(args[i], len);
        if (!args[i]) return;
        memcpy(args[i], start, len);
        args[i][len - 1] = '\0';
    }

    if (*start == '\0') return;

}