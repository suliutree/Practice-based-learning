#include <stdio.h>
#include <unistd.h>    // POSIX标准的UNIX系统调用头文件，提供对标准输入输出的访问，如read、write函数。
#include <termios.h>   // 终端I/O接口，提供对终端属性（如模式设置）的操作函数和结构体。
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>     // 引入错误码定义，用于错误处理，如errno变量。
#include <sys/ioctl.h> // 引入设备控制接口，用于获取终端窗口大小等信息。
#include <string.h>

/*** define ***/

#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN
};

/*** data ***/

struct editorConfig {
    int cx, cy;
    int screenrows;
    int screencols;
    struct termios orig_termios; // 定义一个termios结构体变量orig_termios，用于保存原始的终端属性，以便在程序退出时恢复。
};

struct editorConfig E;


/*** terminal ***/

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);  // 向标准输出发送ANSI转义序列ESC[2J，清除整个屏幕。
    write(STDOUT_FILENO, "\x1b[H", 3);   // 向标准输出发送ANSI转义序列ESC[H，将光标移动到屏幕左上角。

    perror(s);
    exit(1);
}

void disableRawMode() {

    // 使用tcsetattr函数将标准输入（文件描述符STDIN_FILENO）的终端属性设置为orig_termios，TCSAFLUSH选项表示在更改属性前刷新输入缓冲区。
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr");
    }
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode); // 注册disableRawMode函数，使其在程序退出时自动调用，确保终端属性被恢复。

    struct termios raw = E.orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // 使用tcsetattr函数将修改后的raw属性应用到标准输入，TCSAFLUSH表示在设置前刷新输入缓冲区。
}

int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }

    if (c == '\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] > '0' && seq[1] <= 9) {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                }
            }
        }

        return '\x1b';
    } else {
        return c;
    }
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) // 如果写入失败（返回字节数不等于4），返回-1
        return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;           // 验证响应是否以ESC[开头，如果不是，返回-1。
    if (scanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;    // 使用scanf从buf的第三个字符开始，解析两个整数（行和列），如果解析失败，返回-1。

    return 0;
}

int getWindowSize (int *rows, int *cols) {
    struct winsize ws;

    // 使用ioctl系统调用，向终端设备发送TIOCGWINSZ请求，获取窗口大小，并存储在ws中。
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        // 向终端发送ANSI转义序列ESC[999C（向右移动999列）和ESC[999B（向下移动999行），将光标移动到最右下角。
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** append buffer ***/

struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0};

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}

/*** input ***/

void editorMoveCursor(int key) {
    switch (key) {
        case ARROW_LEFT:
            if (E.cx != 0)
                E.cx--;
            break;
        case ARROW_RIGHT:
            if (E.cx >= 0 && E.cx < E.screencols - 1)
                E.cx++;
            break;
        case ARROW_UP:
            if (E.cy != 0)
                E.cy--;
            break;
        case ARROW_DOWN:
            if (E.cy >= 0 && E.cy < E.screenrows - 1)
                E.cy++;
            break;
    }
}

void editorProcessKeypress() {
    int c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times = E.screenrows;
                while (times--)
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
}

/*** output ***/

void editorDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; ++y) {
        if (y == E.screenrows / 3) {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION);
            if (welcomelen > E.screencols) welcomelen = E.screencols;
            int padding = (E.screencols - welcomelen) / 2;
            if (padding) {
                abAppend(ab, "~", 1);
                padding--;
            }
            while (padding--) abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcomelen);
        } else {
            abAppend(ab, "~", 1);
        }

        abAppend(ab, "\x1b[K", 3);
        if (y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6); // hide cursor
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6); // show cursor

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** init ***/

void initEditor() {
    E.cx = 0;
    E.cy = 0;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
}

int main() {    
    enableRawMode();
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}