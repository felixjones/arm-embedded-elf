/*

  elf.h

  Embedded ARM ELF32 loader

*/

#ifndef __ELF_H__
#define __ELF_H__

#include <stddef.h> /* size_t */

/**
 * elf_dl*open flag parameters
 */
#define ELF_RTLD_DEFAULT    ( 0x0 )
#define ELF_RTLD_SKIP_CHECK ( 0x1 )

/**
 * Type used for custom allocators if desired
 * behaves just like realloc, but realloc to zero will free memory
 * @param  void * Cookie pointer provided by elf_allocf caller
 * @param  void * Original memory to reallocate/free or NULL for malloc
 * @param  size_t Allocation size or zero for deallocation
 * @return        Pointer to newly allocated memory or NULL
 */
typedef void * ( * elf_allocf )( void *, void *, size_t );

#if defined( __cplusplus )
extern "C" {
#endif

/**
 * ELF initialization (default realloc/free)
 * @param  buf  Pointer to ELF file in memory
 * @param  flag ELF_RTLD_* bit flags (defined above)
 * @return      Handle to loaded ELF context
 */
void * elf_dlmemopen( const void * buf, int flag );

/**
 * ELF initialization (custom allocator, see elf_allocf)
 * @param  buf   Pointer to ELF file in memory
 * @param  flag  ELF_RTLD_* bit flags (defined above)
 * @param  alloc Realloc with a uptr cookie
 * @param  uptr  Cookie user pointer to be sent to elf_allocf
 * @return       Handle to loaded ELF context
 */
void * elf_dlmemopen_alloc( const void * buf, int flag, elf_allocf alloc, void * uptr );

/**
 * Unlinks and destroys ELF context
 * @param handle Valid, open ELF context
 */
void elf_dlclose( void * handle );

/**
 * Get ELF error message
 * also clears the internal error state of the ELF context
 * @param  handle Valid, open ELF context
 * @return        Error message as a Cstring, invalidated by subsequent elf_ calls
 */
const char * elf_dlerror( void * handle );

/**
 * Add symbol to ELF link map
 * required symbols referenced in the ELF should use this
 * @param handle Valid, open ELF context
 * @param name   Symbol name that will be used by linker
 * @param sym    Pointer to symbol data that will be used by linker
 */
void elf_mapsym( void * handle, const char * name, void * sym );

/**
 * Return memory requirements of linked ELF
 * returned size should be used to allocate space to link ELF into
 * @param  handle Valid, open ELF context
 * @return        Memory byte requirement length
 */
size_t elf_lbounds( void * handle );

/**
 * Link ELF into given memory buffer
 * all resolution is done in this step
 * missing symbols must be already added by elf_mapsym
 * @param handle Valid, open ELF context
 * @param buf    Allocated memory of size given by elf_lbounds
 */
void elf_link( void * handle, void * buf );

/**
 * Find ELF symbol
 * used to locate ELF symbols such as function pointers
 * must not be called before elf_link
 * @param  handle Valid, open ELF context
 * @param  symbol Cstring name that will be searched for within ELF
 * @return        Symbol data
 */
void * elf_dlsym( void * handle, const char * symbol );

#if defined( __cplusplus )
}
#endif

#endif // define __ELF_H__
