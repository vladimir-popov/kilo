#include "keymap.c"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*********************** defines ***********************/

/* Control Symbol Introducer */
#define ESC '\x1b'
#define ESCS "\x1b["

/* Cursor Control */
#define MOVE_RIGHT(N) ESCS #N "C"
#define MOVE_DOWN(N) ESCS #N "B"

/* DSR – Device Status Report */
#define GET_POSISION ESCS "6n"

/* SGR - Select Graphic Rendition */
#define INVERT_COLORS ESCS "7m"

#define CTRL_KEY(k) ((k)&0x1f)

/*********************** terminal ***********************/

void refresh_screen (void);

/* Prints error message and terminate the program */
void
die (const char *msg)
{
  refresh_screen ();
  perror (msg);
  exit (1);
}

/* Changes terminal options to enable raw mode. */
void
enable_raw_mode ()
{
  struct termios raw;
  if (tcgetattr (STDIN_FILENO, &raw) == -1)
    die ("tcgetattr");

  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON | IXOFF);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  /* The VMIN value sets the minimum number of bytes of input needed before
   * read() can return. */
  raw.c_cc[VMIN] = 0;

  /* The VTIME value sets the maximum amount of time to wait before read()
   * returns. It is in tenths of a second. If read() times out, it will
   * return 0. */
  raw.c_cc[VTIME] = 1;

  tcsetattr (STDIN_FILENO, TCSAFLUSH, &raw);
}

key_code
read_key ()
{
  int nread;
  char c;
  while ((nread = read (STDIN_FILENO, &c, 1)) != 1)
    {
      if (nread == -1 && errno != EAGAIN)
        die ("read");
    }

  if (c == ESC)
    {
      char seq[3];
      if (read (STDIN_FILENO, &seq[0], 1) != 1)
        return c;
      if (read (STDIN_FILENO, &seq[1], 1) != 1)
        return c;
      if (seq[0] == '[')
        {
          switch (seq[1])
            {
            case 'A':
              return ARROW_UP;
            case 'B':
              return ARROW_DOWN;
            case 'C':
              return ARROW_RIGHT;
            case 'D':
              return ARROW_LEFT;
            case '3':
              return DELETE_KEY;
            }
        }
    }
  return c;
}

int
get_cursor_position (int *rows, int *cols)
{
  if (write (STDOUT_FILENO, GET_POSISION, 4) != 4)
    return -1;

  char buf[32];
  unsigned int i = 0;

  while (i < sizeof (buf) - 1)
    {
      if (read (STDIN_FILENO, &buf[i], 1) != 1)
        break;
      if (buf[i] == 'R')
        break;
      i++;
    }
  buf[i] = '\0';
  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf (&buf[2], "%d;%d", rows, cols) != 2)
    return -1;
  return 0;
}

int
get_window_size (int *rows, int *cols)
{
  struct winsize ws;
  if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
      if (write (STDOUT_FILENO, MOVE_DOWN (999) MOVE_RIGHT (999), 12) != 12)
        return -1;

      return get_cursor_position (rows, cols);
    }
  else
    {
      *cols = ws.ws_col;
      *rows = ws.ws_row;
      return 0;
    }
}
