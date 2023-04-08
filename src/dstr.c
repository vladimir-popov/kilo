#include <stdlib.h>
#include <string.h>

/******************** Dynamic string *********************/
struct dstr
{
  int length;
  char *chars;
};

#define DSTR_EMPTY                                                            \
  {                                                                           \
    0, NULL                                                                   \
  }

void
dstr_append (struct dstr *this, const char *prefix, int len)
{
  char *new = realloc (this->chars, this->length + len);

  if (new == NULL)
    return;

  memcpy (&new[this->length], prefix, len);
  this->chars = new;
  this->length += len;
}

struct dstr
dstr_init (const char *template)
{
  int len = strlen (template);
  char *str = malloc (len);
  strcpy (str, template);
  struct dstr new = { len, str };
  return new;
}

void
dstr_free (const struct dstr *str)
{
  free (str->chars);
}
