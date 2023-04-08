#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include "dstr.c"
#include "terminal.c"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

/*********************** variables ***********************/

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8

typedef struct erow
{
  /* count of chars in the full row */
  int size;
  /* count of rendered chars */
  int rsize;
  /* all chars of the row */
  char *chars;
  /* rendered chars with replaced tabs */
  char *render;
} erow;

struct config
{
  char *filename;
  /* The position of the cursor in the file 0 based */
  int fx, fy;
  /* The position of the cursor in the rendered file */
  int rx;
  /* The size of the screen 1 based */
  int screen_rows;
  int screen_cols;
  /* Cols and Rows offsets */
  int rowoff;
  int coloff;
  /** The current buffer */
  erow *rows;
  int numrows;
  /* Terminal options */
  struct termios orig_termios;
};

enum align
{
  LEFT,
  CENTER,
  RIGHT
};

struct config C;

/*********************** input ***********************/

void
move_cursor (key_code key)
{
  erow *row = (C.fy >= C.numrows) ? NULL : &C.rows[C.fy];

  switch (key)
    {
    case ARROW_LEFT:
      if (C.fx > 0)
        C.fx--;
      else if (C.fy > 0)
        {
          C.fy--;
          C.fx = C.rows[C.fy].size;
        }
      break;
    case ARROW_DOWN:
      if (C.fy < C.numrows)
        C.fy++;
      break;
    case ARROW_UP:
      if (C.fy > 0)
        C.fy--;
      break;
    case ARROW_RIGHT:
      if (row && C.fx < row->size)
        C.fx++;
      else if (row && C.fx == row->size)
        {
          C.fy++;
          C.fx = 0;
        }
      break;
    }

  row = (C.fy >= C.numrows) ? NULL : &C.rows[C.fy];
  int rowlen = row ? row->size : 0;
  if (C.fx > rowlen)
    C.fx = rowlen;
}

void
process_keypress ()
{
  key_code c = read_key ();
  switch (c)
    {
    case CTRL_KEY ('q'):
      refresh_screen ();
      exit (0);
      break;
    case ARROW_LEFT:
    case ARROW_DOWN:
    case ARROW_UP:
    case ARROW_RIGHT:
      move_cursor (c);
      break;
    }
}

/*********************** output ***********************/

void
draw_welcome_screen (struct dstr *buf)
{
  char welcome[80];
  int welcome_lenght = snprintf (welcome, sizeof (welcome),
                                 "KILO editor -- version %s", KILO_VERSION);
  if (welcome_lenght > C.screen_cols)
    welcome_lenght = C.screen_cols;

  int padding = (C.screen_cols - welcome_lenght) / 2;
  if (padding)
    {
      dstr_append (buf, "~", 1);
      padding--;
    }

  while (padding--)
    dstr_append (buf, " ", 1);

  dstr_append (buf, welcome, welcome_lenght);
}

int
row_fx_to_rx (erow *row, int fx)
{
  int rx = 0;
  for (int j = 0; j < fx; j++)
    {
      if (row->chars[j] == '\t')
        rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
      rx++;
    }
  return rx;
}

void
scroll_editor ()
{
  C.rx = C.fx;
  if (C.fy < C.numrows)
    C.rx = row_fx_to_rx (&C.rows[C.fy], C.fx);

  if (C.fy < C.rowoff)
    C.rowoff = C.fy;

  if (C.fy >= C.rowoff + C.screen_rows)
    C.rowoff = C.fy - C.screen_rows + 1;

  if (C.rx < C.coloff)
    C.coloff = C.rx;

  if (C.rx >= C.coloff + C.screen_cols)
    C.coloff = C.rx - C.screen_cols + 1;
}

/* Replace symbols such as tabs */
void
update_row (erow *row)
{
  int tabs = 0;
  int j;
  int idx = 0;

  for (j = 0; j < row->size; j++)
    if (row->chars[j] == '\t')
      tabs++;

  free (row->render);
  row->render = malloc (row->size + tabs * (KILO_TAB_STOP - 1) + 1);

  for (j = 0; j < row->size; j++)
    {
      if (row->chars[j] == '\t')
        {
          row->render[idx++] = ' ';
          while (idx % KILO_TAB_STOP != 0)
            row->render[idx++] = ' ';
        }
      else
        row->render[idx++] = row->chars[j];
    }
  row->render[idx] = '\0';
  row->rsize = idx;
}

void
append_row (char *s, size_t len)
{
  C.rows = realloc (C.rows, sizeof (erow) * (C.numrows + 1));

  erow *row = &C.rows[C.numrows];
  row->size = len;
  row->chars = malloc (len + 1);
  memcpy (row->chars, s, len);
  row->chars[len] = '\0';

  row->rsize = 0;
  row->render = NULL;
  update_row (row);

  C.numrows++;
}

void
draw_rows (struct dstr *buf)
{
  for (int i = 0; i < C.screen_rows; ++i)
    {
      int filerow = i + C.rowoff;
      if (filerow < C.numrows)
        /* Draws a content of the current file */
        {
          int len = C.rows[filerow].rsize - C.coloff;
          if (len < 0)
            len = 0;
          if (len > C.screen_cols)
            len = C.screen_cols;
          dstr_append (buf, &C.rows[filerow].render[C.coloff], len);
        }
      else
        /* Draw Welcome screen or fill empty terminal rows with ~ */
        {
          if (C.numrows == 0 && i == C.screen_rows / 3)
            draw_welcome_screen (buf);
          else
            dstr_append (buf, "~", 1);
        }
      /* Erase the line right after cursor */
      dstr_append (buf, "\x1b[K", 3);
      dstr_append (buf, "\r\n", 2);
    }
}

void
draw_statusbar_zone (struct dstr *buffer, char *content, int content_len,
                     int zone_len, enum align al)
{
  content_len
      = (content_len < zone_len) ? content_len : zone_len - content_len;
  int pad = 0;
  switch (al)
    {
    case LEFT:
      dstr_append (buffer, content, content_len);
      pad = (zone_len - content_len);
      for (int i = 0; i < pad; i++)
        dstr_append (buffer, " ", 1);
      break;
    case CENTER:
      pad = (zone_len - content_len) / 2;
      for (int i = 0; i < pad; i++)
        dstr_append (buffer, " ", 1);
      dstr_append (buffer, content, content_len);
      for (int i = 0; i < pad; i++)
        dstr_append (buffer, " ", 1);
      break;
    case RIGHT:
      pad = zone_len - content_len;
      for (int i = 0; i < pad; i++)
        dstr_append (buffer, " ", 1);
      dstr_append (buffer, content, content_len);
      break;
    }
}

void
draw_statusbar (struct dstr *buffer)
{
  dstr_append (buffer, INVERT_COLORS, 4);

  int zone_len = C.screen_cols / 3;

  /* Draw file name */
  if (C.filename != NULL)
    {
      int filename_len = strlen (C.filename);
      draw_statusbar_zone (buffer, C.filename, filename_len, zone_len, LEFT);
    }
  else
    {
      draw_statusbar_zone (buffer, " ", 1, zone_len, LEFT);
    }
  /* Draw hint */
  char *hint = "Press Ctrl+Q to exit";
  int hint_len = strlen (hint);
  draw_statusbar_zone (buffer, hint, hint_len, zone_len, CENTER);
  /* Draw cursor position */
  char pos[32];
  snprintf (pos, sizeof (pos), "%d:%d/%d", C.rx + 1, C.fy + 1, C.numrows);
  int pos_len = strlen (pos);
  draw_statusbar_zone (buffer, pos, pos_len, zone_len, RIGHT);

  dstr_append (buffer, "\x1b[m", 3);
}

void
render_buffer (struct dstr *buffer)
{
  write (STDIN_FILENO, buffer->chars, buffer->length);
}

void
refresh_screen ()
{

  /* Save the current window size */
  if (get_window_size (&C.screen_rows, &C.screen_cols) == -1)
    die ("get_window_size");

  /* Reserve space for status bar */
  C.screen_rows -= 1;

  scroll_editor ();

  struct dstr buf = DSTR_EMPTY;

  /* Hide the cursor */
  dstr_append (&buf, "\x1b[?25l", 6);
  /* Put the cursor to the upper left corner */
  dstr_append (&buf, "\x1b[H", 3);
  /* Write the ~ on every not filled row */
  draw_rows (&buf);
  /* Draw statusbar at the end of the buffer */
  draw_statusbar (&buf);
  /* Put the cursor back to the upper left corner */
  dstr_append (&buf, "\x1b[H", 3);
  /* Put cursor to the saved position */
  char b[32];
  snprintf (b, sizeof (b), "\x1b[%d;%dH", (C.fy - C.rowoff) + 1,
            (C.rx - C.coloff) + 1);
  dstr_append (&buf, b, strlen (b));
  /* Show the cursor */
  dstr_append (&buf, "\x1b[?25h", 6);

  /* Flash buffer to the stdout and free memory */
  render_buffer (&buf);
  dstr_free (&buf);
}

/******************** file i/o **********************/

void
open_file (char *filename)
{
  FILE *fp = fopen (filename, "r");
  C.filename = filename;
  if (!fp)
    die ("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline (&line, &linecap, fp)) != -1)
    {
      // Remove "end of line" symbols
      while (linelen > 0
             && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
        linelen--;

      append_row (line, linelen);
    }
  free (line);
  fclose (fp);
}

/*********************** init ***********************/

/* Restores original terminal settings */
void
restore_termios ()
{
  if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &C.orig_termios) == -1)
    die ("tcsetattr");
}

void
init_editor (void)
{
  C.fx = 0;
  C.fy = 0;
  C.rx = 0;
  C.rowoff = 0;
  C.coloff = 0;
  C.numrows = 0;
  C.rows = NULL;
  C.filename = NULL;

  /* Saves the original terminal options */
  if (tcgetattr (STDIN_FILENO, &C.orig_termios) == -1)
    die ("tcgetattr");

  /* Set hook to restore the original terminal options */
  atexit (restore_termios);
}

int
main (int argc, char *argv[])
{
  enable_raw_mode ();
  init_editor ();

  if (argc >= 2)
    open_file (argv[1]);

  while (1)
    {
      refresh_screen ();
      process_keypress ();
    }
  return 0;
}
