/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

/*** defines ***/
#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY, // <esc>[3~
    HOME_KEY,
    END_KEY,
    PAGE_UP, // <esc>[5~
    PAGE_DOWN // <esc>[6~
};

/*** data ***/

struct editorConfig {
    int cx, cy; //cursor position
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;

// struct termios orig_termios;

/*** terminal: deals with low-level terminal input ***/

/*Error handling*/
void die(const char *s){
    write(STDOUT_FILENO, "\x1b[2J", 4); //Clear the entire screen
    write(STDOUT_FILENO, "\x1b[H", 3); //Put position of cursor to top left corner of the screen

    perror(s);
    exit(1);
}

void disableRawMode(){
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1){
        die("tcsetattr");
    }
}

void enableRawMode(){
    /* Save original terminal mode */
    if (tcgetattr(STDIN_FILENO, &E.orig_termios)) {
        die("tcgetattr");
    }
    /* Set terminal to normal mode when kilo exit */
    atexit(disableRawMode);
  
    struct termios raw = E.orig_termios;
    /* Turn of software flow control for input flag (IXON) and auto translate ('\r' to '\n') (ICRNL)*/
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* Turn off "\n" to "\r\n" in output (Disable POST-PROCESSING for output ) */
    raw.c_oflag &= ~(OPOST);
    /* Set terminal to raw mode and not echo , also turn of signal to prent SIGINT and SIGTSTP */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG );

    raw.c_cc[VMIN] = 0; //set minimum number of bytes of input needed before read() can return
    raw.c_cc[VTIME] = 1; //Max timeout to wait before wait() return: Get 10 char per second

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) {
        die("tcsetattr");
    }
}

/* Read char from keyboard, key by key . Hang if no char is input */
int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    /* Process for Arrow key*/
    if (c == '\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1])
                    {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch (seq[1])
                {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            } 
        }
        else if (seq[0] == 'O') {
            switch (seq[1])
            {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        return '\x1b';
    }
    else {
        return c;
    }
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1; //Send cmd to get cursor position, return back to STDIN read

    //Parse respone to get window size
    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';
    
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    editorReadKey();
    return -1;
}

/* Get window Screen size */
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        // bring cursor to right conner bottom and get position at this point
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    }
    else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** append buffer ***/
// To write to screen 1 time, not used too much write()
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

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

/*** output ***/
/* Draw ~ at the beginning of 24 lines */
void editorDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        /* Display welcome message */
        if (y == E.screenrows / 3) {
            char welcome[80];
            int welcomlen = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION);
            if (welcomlen > E.screencols) welcomlen = E.screencols;
            int padding = (E.screencols - welcomlen) / 2;
            if (padding) {
                abAppend(ab, "~", 1);
                padding--;
            }
            while (padding--) abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcomlen);
        }
        else {
            /* Draw rows*/
            abAppend(ab, "~", 1);
        }
        // abAppend(ab, "\x1b[K", 3);
        // Clear line after cursor position
        abAppend(ab, "\x1b[0K", 4);
        if (y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6); //Hide cursor when clear the screen
    //abAppend(&ab, "\x1b[2J", 4); //Clear the entire screen .Not used, let clear line by line when draw
    abAppend(&ab, "\x1b[H", 3); //Put position of cursor to top left corner of the screen ~ [H: Edit cursor position

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6); //Show cursor when clear the screen

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** input: deals with mapping keys to editor functions at a much higher level. ***/
/* Process Arrow keys */
void editorMoveCursor(int key) {
    switch (key)
    {
    case ARROW_LEFT:
        if (E.cx != 0) {
            E.cx--;
        }
        break;
    case ARROW_RIGHT:
        if (E.cx != E.screencols - 1) {
            E.cx++;
        }
        break;
    case ARROW_UP:
        if (E.cy != 0) {
            E.cy--;
        }
        break;
    case ARROW_DOWN:
        if (E.cy != E.screenrows - 1) {
            E.cy++;
        }
        break;
    default:
        break;
    }
}

void editorProcessKeypress() {
    int c = editorReadKey();
    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4); //Clear the entire screen
        write(STDOUT_FILENO, "\x1b[H", 3); //Put position of cursor to top left corner of the screen
        exit(0);
        break;
    case HOME_KEY:
        E.cx = 0;
        break;
    case END_KEY:
        E.cx = E.screencols - 1;
        break;
    case PAGE_UP:
    case PAGE_DOWN:
        {
            int times = E.screenrows;
            while (times--) {
                editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
        }
        break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        editorMoveCursor(c);
        break;  
    default:
        break;
    }
}
/*** init ***/
void initEditor() {
    E.cx = 0;
    E.cy = 0;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main(){
    enableRawMode();
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}