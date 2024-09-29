#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>


char *lsh_read_line() {
    char *line = NULL;
    size_t bufsize = 0;

    // getline 从标准输入读取整行，包括换行符，并根据需要自动分配内存。
    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            exit(EXIT_SUCCESS);
        } else {
            perror("readline");
            exit(EXIT_FAILURE);
        }
    }

    return line;
}

/**
 * strtok 用于将字符串拆分成一系列的标记（tokens），基于指定的分隔符。
 * 返回指向下一个标记的指针，如果没有更多的标记则返回 NULL。
 * 
 * 工作原理：
 *   - strtok 会在第一次调用时接收要分割的字符串，并查找第一个标记。
 *   - 它使用静态变量保存字符串的当前位置，因此后续调用时将 str 参数设置为 NULL，以继续从上次中断的位置开始。
 *   - 每次调用会将找到的分隔符替换为字符串结束符 \0，从而将字符串分割开。
 * 
 * 注意： strtok 不是线程安全的，因为它使用了内部的静态变量。如果需要线程安全的版本，可以使用 strtok_r。
 */

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a" // // Space, Tab, Carriage Return, Newline, Bell
char **lsh_split_line(char *line) {
    int bufsize = LSH_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "lsh: allocation error.\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, LSH_TOK_DELIM);
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += LSH_TOK_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "lsh: allocation error.\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, LSH_TOK_DELIM);
    }
    tokens[position] = NULL;

    return tokens;
}

/**
 * 内置命令：由 Shell 自身直接实现的命令。如 cd（更改目录）、exit（退出 Shell）、export（设置环境变量）、alias（创建别名）等。
 * 非内置命令：系统中的可执行文件，由 Shell 通过创建子进程来执行。如 ls（列出目录内容）、grep（文本搜索）、cat（连接文件并打印）等。
 */

/// 执行非内置外部命令
int lsh_launch(char **args) {
    pid_t pid, wpid;
    int status;

    pid = fork(); // Create child process
    if (pid == 0) {
        // Child process
        if (execvp(args[0], args) == -1) {
            perror("lsh");
        }
        exit(EXIT_FAILURE); 
    } else if (pid < 0) {
        // Create child process failed
        perror("lsh");
    } else {
        do {
            wpid = waitpid(pid, &status, WUNTRACED); // 等待子进程结束
        } while (!WIFEXITED(status) && !WIFSIGNALED(status)); // 如果子进程正常退出或者子进程被信号中断，返回 true
    }

    return 1;
}

/// Function Declarations for builtin shell commands
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_greet(char **args);

char *builtin_str[] = {
    "cd",
    "help",
    "exit",
    "greet"
};

/**
 * 声明了一个函数指针数组，数组名为 builtin_func。
 * 数组中每个元素都是指向函数的指针，该函数接受一个 char ** 的参数，返回一个 int。
 */
int (*builtin_func[]) (char **) = {
    &lsh_cd,
    &lsh_help,
    &lsh_exit,
    &lsh_greet
};

int lsh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char*);
}

int lsh_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "lsh: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("lsh");
        }
    }
    return 1;
}

int lsh_help(char **args) {
    printf("The following are built in:\n");

    for (int i = 0; i < lsh_num_builtins(); ++i) {
        printf(" %s\n", builtin_str[i]);
    }

    printf("Use the man command for information on other programs.\n");
    return 1;
}

int lsh_exit(char **args) {
    return 0;
}

int lsh_greet(char **args) {
    printf("Hello, welcome to lsh!\n");
    return 1;
}

int lsh_execute(char **args) {
    int i;

    if (args[0] == NULL) {
        // An empty command was entered.
        return 1;
    }

    for (i = 0; i < lsh_num_builtins(); ++i) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return lsh_launch(args);
}

void lsh_loop() {
    char *line;
    char **args;
    int status;

    do {
        printf(">");
        line = lsh_read_line();
        args = lsh_split_line(line);
        status = lsh_execute(args);

        free(line);
        free(args);
    } while(status);
}

int main(int argc, char **argv) {
    lsh_loop();

    return EXIT_SUCCESS;
}


/**
 * 示例
 * help：调用内置命令 help，触发 lsh_help() 函数。
 * cd ../：调用内置命令 cd，更改当前目录为上级目录，触发 lsh_cd() 函数。
 * cd：不带参数调用 cd，应打印错误信息，测试 lsh_cd() 对缺少参数的处理。
 * exit：调用内置命令 exit，触发 lsh_exit() 函数，退出 shell。
 * ls：执行外部命令 ls，列出当前目录内容，触发 lsh_launch() 函数。
 * pwd：执行外部命令 pwd，显示当前工作目录，触发 lsh_launch() 函数。
 * echo Hello：执行外部命令 echo，打印字符串 Hello，触发 lsh_launch() 函数。
 * 空命令（直接按回车）：测试输入空命令的情况，触发 lsh_execute() 中对空命令的处理。
 * nonexistentcommand：输入一个不存在的命令，测试程序对错误命令的处理，触发 lsh_launch() 中的错误处理。
 */