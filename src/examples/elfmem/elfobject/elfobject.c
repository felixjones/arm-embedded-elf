/*

  elfobject.c

*/

#include <stdio.h> /* printf */

void __attribute__ ((constructor)) test_ctor(void) {
  printf( "test_ctor\n" );
}

void __attribute__  ((destructor)) test_dtor(void) {
  printf( "test_dtor\n" );
}

__attribute__ ((visibility("default"))) int test_mul( int a, int b ) {
  return a * b;
}
