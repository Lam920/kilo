/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct termios orig_termios;

/*** terminal ***/

/*Error handling*/
void die(const char *s){
    perror(s);
    exit(1);
}

void disableRawMode(){
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1){
        die("tcsetattr");
    }
}

void enableRawMode(){
    /* Save original terminal mode */
    if (tcgetattr(STDIN_FILENO, &orig_termios)) {
        die("tcgetattr");
    }
    /* Set terminal to normal mode when kilo exit */
    atexit(disableRawMode);
  
    struct termios raw = orig_termios;
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

char editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

void editorProcessKeypress() {
    char c = editorReadKey();

    switch (c)
    {
    case CTRL_KEY('q'):
        exit(0);
        break;  
    default:
        break;
    }
}
/*** init ***/

int main(){
    enableRawMode();

    while (1) {
        editorProcessKeypress();
    }
    return 0;
}