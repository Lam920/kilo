- read() returns the number of bytes that it read, return 0 when reach EOF
Ctrl-D: tell read() that it's reach EOF
+ TCSAFLUSH:  argument specifies when to apply the change, it waits for all 
pending output to be written to the terminal, and also discards any input that hasn’t been read.
+ ECHO: print char we type into terminal
+ ICANON: read input line by line instead of char by char (press Enter to read a line)

- Software flow control:
+ Ctrl-S: Stops data from being transmitted to the terminal util you press 
+ Ctrl-Q

- After setting terminal to raw mode:
+ Ctrl- Key = 1 - 26: Actually Ctrl will strip bit 5, 6 of Key press ==> Value of Ctrl + Key
Example: A = 65(10) = 1000001 ==> Strip bit 5 , 6 == > 00001 ==> Ctrl + A = 1(10) 
+ 0x1b = 27 = Esc


w a s d keys. (If you’re unfamiliar with using these keys as arrow keys: w is your up arrow, s is your down arrow, a is left, d is right.)

Reads time out (after 0.1 seconds), 

References:
+ Escape sequence: https://en.wikipedia.org/wiki/VT100, https://vt100.net/docs/vt100-ug/chapter3.html#CUP
