/*

  elfmem.c

*/

#include <elf/elf.h>

#include <stdio.h> /* printf */
#include <stdlib.h> /* malloc free */

/* elfobject compiled and binary is in memory */
static char elf_file_in_memory[];

typedef int ( * mulf )( int, int );

int main( int argc, char * argv[] ) {
  const char * error;
  void * linkMemory = NULL;

  void * const handle = elf_dlmemopen( elf_file_in_memory, ELF_RTLD_DEFAULT );
  error = elf_dlerror( handle );
  if ( error ) {
    printf( "ELF error \"%s\"\n", error );
    goto _exit;
  }

  /* Add printf function to link map so ELF can use it */
  elf_mapsym( handle, "printf", ( void * )printf );
  error = elf_dlerror( handle );
  if ( error ) {
    printf( "ELF error \"%s\"\n", error );
    goto _exit;
  }

  /* Allocate required memory */
  linkMemory = malloc( elf_lbounds( handle ) );
  if ( !linkMemory ) {
    /* malloc failed */
    goto _exit;
  }

  /* Link loaded ELF! */
  elf_link( handle, linkMemory );
  error = elf_dlerror( handle );
  if ( error ) {
    printf( "ELF error \"%s\"\n", error );
    goto _exit;
  }

  /* We can now retrieve and use symbols */
  mulf mul = ( mulf )elf_dlsym( handle, "test_mul" );
  if ( !mul ) {
    /* Failed to find symbol */
    error = elf_dlerror( handle );
    printf( "ELF error \"%s\"\n", error );
    goto _exit;
  }

  /* Use function in ELF */
  const int result = ( *mul )( 2, 3 );
  printf( "2 * 3 = %d\n", result );

_exit:
  elf_dlclose( handle );
  if ( linkMemory ) {
    free( linkMemory );
  }
  return 0;
}
