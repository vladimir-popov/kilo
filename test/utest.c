#include "dstr.c"
#include <assert.h>
#include <stdio.h>
#include <struct.h>

void
assertEq (char *actual, char *expected)
{
  int la = strlen (actual);
  int le = strlen (expected);
  assert (la == le);

  for (int i = 0; i < la; i++)
    {
      assert (actual[i] == expected[i]);
    }
}

void
test_init ()
{
  struct dstr actual = dstr_init ("Hello world!");
  assert (actual.length == 12);
  assertEq (actual.chars, "Hello world!");
}

void
test_append ()
{
  struct dstr str = dstr_init ("Hello");
  dstr_append (&str, " world!?", 7);
  assertEq (str.chars, "Hello world!");
}

int
main (void)
{
  test_init ();
  test_append ();
  printf ("All tests have been passed");
}
