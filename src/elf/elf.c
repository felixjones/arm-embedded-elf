/*

  elf.c

  Embedded ARM ELF32 loader

*/

/*

  ELF32

  Definitions based on "sys/elf.h"

*/

#include <stdint.h> /* uint8_t uint16_t uint32_t int32_t uintptr_t */

#define EI_CLASS   ( 4 )
#define EI_DATA    ( 5 )
#define EI_VERSION ( 6 )
#define EI_NIDENT  ( 16 )

#define ELFCLASS32  ( 1 )
#define ELFDATA2LSB ( 1 )
#define ET_DYN      ( 3 )

#define PT_LOAD    ( 1 )
#define PT_DYNAMIC ( 2 )

#define DT_NULL         ( 0 )
#define DT_NEEDED       ( 1 )
#define DT_PLTRELSZ     ( 2 )
#define DT_PLTGOT       ( 3 )
#define DT_HASH         ( 4 )
#define DT_STRTAB       ( 5 )
#define DT_SYMTAB       ( 6 )
#define DT_RELA         ( 7 )
#define DT_RELASZ       ( 8 )
#define DT_RELAENT      ( 9 )
#define DT_STRSZ        ( 10 )
#define DT_SYMENT       ( 11 )
#define DT_INIT         ( 12 )
#define DT_FINI         ( 13 )
#define DT_SONAME       ( 14 )
#define DT_RPATH        ( 15 )
#define DT_SYMBOLIC     ( 16 )
#define DT_REL          ( 17 )
#define DT_RELSZ        ( 18 )
#define DT_RELENT       ( 19 )
#define DT_PLTREL       ( 20 )
#define DT_DEBUG        ( 21 )
#define DT_TEXTREL      ( 22 )
#define DT_JMPREL       ( 23 )
#define DT_INIT_ARRAY   ( 0x19 )
#define DT_INIT_ARRAYSZ ( 0x1b )
#define DT_FINI_ARRAY   ( 0x1a )
#define DT_FINI_ARRAYSZ ( 0x1c )
#define DT_LOPROC       ( 0x70000000 )
#define DT_HIPROC       ( 0x7fffffff )

#define SHN_UNDEF     ( 0 )
#define SHN_LORESERVE ( 0xff00 )
#define SHN_ABS       ( 0xfff1 )

#define STB_GLOBAL ( 1 )
#define STB_WEAK   ( 2 )

typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word, Elf32_Off, Elf32_Addr;
typedef int32_t Elf32_Sword;

typedef struct {
  uint8_t    e_ident[EI_NIDENT];
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off  e_phoff;
  Elf32_Off  e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;
} Elf32_Ehdr;

typedef struct {
  Elf32_Word  p_type;
  Elf32_Off   p_offset;
  Elf32_Addr  p_vaddr;
  Elf32_Addr  p_paddr;
  Elf32_Word  p_filesz;
  Elf32_Word  p_memsz;
  Elf32_Word  p_flags;
  Elf32_Word  p_align;
} Elf32_Phdr;

typedef struct {
  Elf32_Word st_name;
  Elf32_Addr st_value;
  Elf32_Word st_size;
  uint8_t    st_info;
  uint8_t    st_other;
  Elf32_Half st_shndx;
} Elf32_Sym;

typedef struct {
  Elf32_Sword d_tag;

  union {
    Elf32_Word d_val;
    Elf32_Addr d_ptr;
  } d_un;
} Elf32_Dyn;

typedef struct {
  Elf32_Addr r_offset;
  Elf32_Word r_info;
} Elf32_Rel;

#define ELF32_PH_GET( header, index )       ( ( Elf32_Phdr * )( ( uintptr_t )( header ) + ( header )->e_phoff + ( header )->e_phentsize * ( index ) ) )
#define ELF32_PH_CONTENT( header, section ) ( ( uintptr_t )( header ) + ( section )->p_offset )

#define ELF32_ST_BIND( info ) ( ( info ) >> 4 )

#define ELF32_R_SYM( info )  ( ( info ) >> 8 )
#define ELF32_R_TYPE( info ) ( ( uint8_t )( info ) )

/*

  ARM ELF

  4.6.1.2 Relocation types "ELF for the ARM Architecture"

*/

#define R_ARM_ABS32     ( 2 )
#define R_ARM_JUMP_SLOT ( 22 )
#define R_ARM_RELATIVE  ( 23 )

/*

  Elf

*/

#include "elf/elf.h"

#include <stdlib.h> /* realloc */
#include <string.h> /* memset memcpy */

/**
 * When set, this flag indicates an error Cstring is available
 * elf_dlerror retrieves the Cstring and resets this flag
 */
#define _ELF_ERROR ( 0x1 << 15 )

/**
 * Generic void function call type
 * used by init and fini functions
 */
typedef void ( * elf_voidf )( void );

/**
 * Used for storing symbols in the link map
 * link map is stored in a lazy binary tree
 */
typedef struct Elf_symbolNode {
  int                     hash;
  void *                  symbol;
  struct Elf_symbolNode * lt;
  struct Elf_symbolNode * gt;
} Elf_symbolNode;

/**
 * Internal ELF context structure
 * instance is returned from elf_dl*open
 */
typedef struct {
  elf_allocf       alloc;
  void *           uptr;
  int              flags;
  const char *     error;
  Elf32_Ehdr *     header;
  Elf_symbolNode * globalSymbols;
  elf_voidf *      finiArray;
  Elf32_Word       finiLength;
} Elf_handle;

/**
 * Handy macro for casting pointer to usable Elf_handle
 */
#define _ELF_H( X ) ( ( Elf_handle * )( X ) )

/**
 * Error string messages
 * these are not descriptive to save space and be displayable on short column
 */
static const char * const _elf_error_magic_id                 = "Magic ID";
static const char * const _elf_error_class                    = "Class";
static const char * const _elf_error_endian                   = "Endian";
static const char * const _elf_error_version                  = "Version";
static const char * const _elf_error_type                     = "Type";
static const char * const _elf_error_dynamic_section          = "Dynamic section";
static const char * const _elf_error_dependency               = "Dependency";
static const char * const _elf_error_d_tag                    = "D_tag";
static const char * const _elf_error_missing_entries          = "Missing entries";
static const char * const _elf_error_unresolved_symbol        = "Unresolved symbol";
static const char * const _elf_error_unimplemented_st_shndx   = "Unimplemented st_shndx";
static const char * const _elf_error_zero_sized_rel           = "Zero sized rel";
static const char * const _elf_error_unimplemented_relocation = "Unimplemented relocation";

/**
 * Handy short cut for calling custom elf_allocf as malloc
 * @param  handle ELF context structure
 * @param  size   Length of memory to be allocated
 * @return        Pointer to allocated memory, or NULL if failed
 */
inline static void * _elf_malloc( Elf_handle * handle, size_t size ) {
  return handle->alloc( handle->uptr, NULL, size );
}

/**
 * Handy short cut for calling custom elf_allocf as free
 * @param handle ELF context structure
 * @param ptr    Pointer to memory previously allocated by _elf_malloc
 */
inline static void _elf_free( Elf_handle * handle, void * ptr ) {
  handle->alloc( handle->uptr, ptr, 0 );
}

/**
 * Fast, terrible hash function for Cstrings
 * based on Java's hash; known to be terrible
 * @param  str Cstring to be hashed
 * @return     Hash value
 */
static int _elf_hash( const char * str ) {
  int hash = 7;

  while ( *str ) {
    hash = hash * 31 + *str;
    str++;
  }

  return hash;
}

/**
 * Implementation of an elf_allocf that uses STD malloc
 * @param  cookie  Provided by ELF context structure
 * @param  ptr     Original memory to reallocate/free or NULL for malloc
 * @param  newsize Allocation size or zero for deallocation
 * @return         Pointer to newly allocated memory or NULL
 */
static void * _elf_stdalloc( void * cookie, void * ptr, size_t newsize ) {
  if ( !newsize ) {
    free( ptr );
    return NULL;
  }

  return realloc( ptr, newsize );
}

/**
 * Only does basic checks to validate an ELF header
 * this is skipped if ELF_RTLD_SKIP_CHECK is set
 * @param handle ELF context structure
 */
static void _elf_check( Elf_handle * handle ) {
  const uint32_t ident4 = *( uint32_t * )( &handle->header->e_ident[0] );

  if ( ident4 != 0x464C457F ) {
    handle->flags |= _ELF_ERROR;
    handle->error = _elf_error_magic_id;
    return;
  }

  if ( handle->header->e_ident[EI_CLASS] != ELFCLASS32 ) {
    handle->flags |= _ELF_ERROR;
    handle->error = _elf_error_class;
    return;
  }

  if ( handle->header->e_ident[EI_DATA] != ELFDATA2LSB ) {
    handle->flags |= _ELF_ERROR;
    handle->error = _elf_error_endian;
    return;
  }

  if ( handle->header->e_ident[EI_VERSION] != 1 ) {
    handle->flags |= _ELF_ERROR;
    handle->error = _elf_error_version;
    return;
  }

  if ( handle->header->e_type != ET_DYN ) {
    handle->flags |= _ELF_ERROR;
    handle->error = _elf_error_type;
    return;
  }
}

/**
 * Deallocates symbol tree
 * used to kill the link map
 * unfortunately this is recursive
 * @param handle ELF context structure
 * @param node   Chunk of tree to start freeing
 */
static void _elf_tree_free( Elf_handle * handle, Elf_symbolNode * node ) {
  if ( node->lt ) {
    _elf_tree_free( handle, node->lt );
  }

  if ( node->gt ) {
    _elf_tree_free( handle, node->gt );
  }

  _elf_free( handle, node );
}

/**
 * Locate symbol within symbol tree
 * used to search link map
 * @param  handle ELF context structure
 * @param  hash   Hash of a symbol Cstring to find in tree
 * @return        Symbol value
 */
static void * _elf_tree_find( Elf_handle * handle, int hash ) {
  Elf_symbolNode ** map = &handle->globalSymbols;

  while ( *map ) {
    if ( ( *map )->hash == hash ) {
      return ( *map )->symbol;
    }

    if ( hash < ( *map )->hash ) {
      map = &( *map )->lt;
      continue;
    }

    if ( hash > ( *map )->hash ) {
      map = &( *map )->gt;
      continue;
    }
  }

  return NULL;
}

/**
 * Insert/add/replace symbol into tree
 * no tree balancing is done, this is lazy
 * @param handle ELF context structure
 * @param hash   Hash of a symbol Cstring
 * @param sym    Symbol value itself
 */
static void _elf_tree_add( Elf_handle * handle, const int hash, void * sym ) {
  Elf_symbolNode ** map = &handle->globalSymbols;

  while ( *map ) {
    if ( hash == ( *map )->hash ) {
      ( *map )->symbol = sym;
      return;
    }

    if ( hash < ( *map )->hash ) {
      map = &( *map )->lt;
      continue;
    }

    if ( hash > ( *map )->hash ) {
      map = &( *map )->gt;
      continue;
    }
  }

  *map = ( Elf_symbolNode * )_elf_malloc( handle, sizeof( **map ) );
  ( *map )->hash = hash;
  ( *map )->symbol = sym;
  ( *map )->lt = ( *map )->gt = NULL;
}

/**
 * Relocates symbols within a given relocation table
 * @param handle  ELF context structure
 * @param buf     Executable memory ELF is linking into
 * @param reltab  Source relocation table
 * @param entsize Span between each relocation in table
 * @param limit   Size of the relocation table
 * @param symtab  Symbol table to be resolved
 * @param syment  Span between each symbol entry
 */
static void _elf_relocate( Elf_handle * handle, void * buf, uintptr_t reltab, Elf32_Word entsize, Elf32_Word limit, uintptr_t symtab, Elf32_Word syment ) {
  const uintptr_t tableEnd = reltab + limit;

  /* Loop through relocation table and relocate the symbols */
  while ( reltab < tableEnd ) {
    const Elf32_Rel * const rel = ( Elf32_Rel * )reltab;
    const Elf32_Sym * const symbol = ( Elf32_Sym * )( symtab + ( ELF32_R_SYM( rel->r_info ) * syment ) );
    uint32_t * const ref = ( uint32_t * )( ( uintptr_t )buf + rel->r_offset );

    switch ( ELF32_R_TYPE( rel->r_info ) ) {
    case R_ARM_ABS32:
      *ref += symbol->st_value;
      break;
    case R_ARM_JUMP_SLOT:
      *ref = symbol->st_value;
      break;
    case R_ARM_RELATIVE:
      *ref += ( uintptr_t )buf;
      break;
    default:
      handle->flags |= _ELF_ERROR;
      handle->error = _elf_error_unimplemented_relocation;
      return;
    }

    reltab += entsize;
  }
}

/*

  ELF implementations

*/

/**
 * ELF initialization (default realloc/free)
 * @param  buf  Pointer to ELF file in memory
 * @param  flag ELF_RTLD_* bit flags (defined above)
 * @return      Handle to loaded ELF context
 */
void * elf_dlmemopen( const void * buf, int flag ) {
  Elf_handle * const handle = ( Elf_handle * )_elf_stdalloc( NULL, NULL, sizeof( *handle ) );

  handle->alloc = _elf_stdalloc;
  handle->uptr = NULL;
  handle->flags = flag;
  handle->header = ( Elf32_Ehdr * )buf;
  handle->globalSymbols = NULL;
  handle->finiArray = NULL;
  handle->finiLength = 0;

  if ( ( handle->flags & ELF_RTLD_SKIP_CHECK ) == 0 ) {
    _elf_check( handle );
  }

  return handle;
}

/**
 * ELF initialization (custom allocator, see elf_allocf)
 * @param  buf   Pointer to ELF file in memory
 * @param  flag  ELF_RTLD_* bit flags (defined above)
 * @param  alloc Realloc with a uptr cookie
 * @param  uptr  Cookie user pointer to be sent to elf_allocf
 * @return       Handle to loaded ELF context
 */
void * elf_dlmemopen_alloc( const void * buf, int flag, elf_allocf alloc, void * uptr ) {
  Elf_handle * const handle = ( Elf_handle * )alloc( uptr, NULL, sizeof( *handle ) );

  handle->alloc = alloc;
  handle->uptr = uptr;
  handle->flags = flag;
  handle->header = ( Elf32_Ehdr * )buf;
  handle->globalSymbols = NULL;
  handle->finiArray = NULL;
  handle->finiLength = 0;

  if ( ( handle->flags & ELF_RTLD_SKIP_CHECK ) == 0 ) {
    _elf_check( handle );
  }

  return handle;
}

/**
 * Unlinks and destroys ELF context
 * @param handle Valid, open ELF context
 */
void elf_dlclose( void * handle ) {
  /* Call ELF destructors */
  for ( Elf32_Word ii = 0; ii < _ELF_H( handle )->finiLength; ii++ ) {
    ( *_ELF_H( handle )->finiArray[ii] )();
  }

  /* Release link map */
  if ( _ELF_H( handle )->globalSymbols ) {
    _elf_tree_free( _ELF_H( handle ), _ELF_H( handle )->globalSymbols );
  }

  const elf_allocf alloc = _ELF_H( handle )->alloc;
  void * const uptr = _ELF_H( handle )->uptr;

  alloc( uptr, handle, 0 );
}

/**
 * Get ELF error message
 * also clears the internal error state of the ELF context
 * @param  handle Valid, open ELF context
 * @return        Error message as a Cstring, invalidated by subsequent elf_ calls
 */
const char * elf_dlerror( void * handle ) {
  if ( _ELF_H( handle )->flags & _ELF_ERROR ) {
    _ELF_H( handle )->flags &= ~_ELF_ERROR; /* Remove error flag */
    return _ELF_H( handle )->error;
  }

  return NULL;
}

/**
 * Add symbol to ELF link map
 * required symbols referenced in the ELF should use this
 * @param handle Valid, open ELF context
 * @param name   Symbol name that will be used by linker
 * @param sym    Pointer to symbol data that will be used by linker
 */
void elf_mapsym( void * handle, const char * name, void * sym ) {
  const int hash = _elf_hash( name );

  _elf_tree_add( _ELF_H( handle ), hash, sym );
}

/**
 * Return memory requirements of linked ELF
 * returned size should be used to allocate space to link ELF into
 * @param  handle Valid, open ELF context
 * @return        Memory byte requirement length
 */
size_t elf_lbounds( void * handle ) {
  size_t high = 0;

  /* Size needed is the size of the program binary in ELF */
  for ( Elf32_Half ii = 0; ii < _ELF_H( handle )->header->e_phnum; ii++ ) {
    Elf32_Phdr * const program = ELF32_PH_GET( _ELF_H( handle )->header, ii );

    if ( program->p_type == PT_LOAD ) {
      uint32_t segMax = program->p_vaddr + program->p_memsz;

      segMax = ( ( segMax - 1 ) / program->p_align + 1 ) * program->p_align;

      if ( segMax > high ) {
        high = segMax;
      }
    }
  }

  return high;
}

/**
 * Link ELF into given memory buffer
 * all resolution is done in this step
 * missing symbols must be already added by elf_mapsym
 * @param handle Valid, open ELF context
 * @param buf    Allocated memory of size given by elf_lbounds
 */
void elf_link( void * handle, void * buf ) {
  const Elf32_Ehdr * const header = _ELF_H( handle )->header;
  const Elf32_Phdr * dynamicSection = NULL;

  /* Copy ELF program into memory and prepares static variables */
  for ( Elf32_Half ii = 0; ii < header->e_phnum; ii++ ) {
    Elf32_Phdr * const h = ELF32_PH_GET( header, ii );

    if ( h->p_type == PT_LOAD ) {
      const uintptr_t dest = ( uintptr_t )buf + h->p_vaddr;

      memset( ( void * )( dest + h->p_filesz ), 0, h->p_memsz - h->p_filesz );
      memcpy( ( void * )dest, ( void * )ELF32_PH_CONTENT( header, h ), h->p_filesz );
    }
  }

  /* Find dynamic section */
  for ( Elf32_Half ii = 0; ii < header->e_phnum; ii++ ) {
    Elf32_Phdr * const section = ELF32_PH_GET( header, ii );

    if ( section->p_type == PT_DYNAMIC ) {
      dynamicSection = section;
      break;
    }
  }

  /* Dynamic section is kind of important for dynamic libraries */
  if ( !dynamicSection ) {
    _ELF_H( handle )->flags |= _ELF_ERROR;
    _ELF_H( handle )->error = _elf_error_dynamic_section;
    return;
  }

  Elf32_Word pltrelsz = 0, strsz = 0, syment = 0, relsz = 0, relent = 0, initLength = 0;
  uintptr_t reltab = 0, jmpReltab = 0, symtab = 0;
  const Elf32_Word * hash = NULL;
  const char * strtab = NULL;
  const elf_voidf * initArray = NULL;

  /* Pull out the table information from the dynamic section */
  /* this will be used for dynamic relocation */
  for ( Elf32_Dyn * dynamics = ( Elf32_Dyn * )ELF32_PH_CONTENT( header, dynamicSection ); dynamics->d_tag != DT_NULL; dynamics++) {
    switch ( dynamics->d_tag ) {
    case DT_NEEDED: /* Dependencies are not supported, so return */
      _ELF_H( handle )->flags |= _ELF_ERROR;
      _ELF_H( handle )->error = _elf_error_dependency;
      return;
    case DT_PLTRELSZ:
      pltrelsz = dynamics->d_un.d_val;
      break;
    case DT_HASH:
      hash = ( Elf32_Word * )( ( uintptr_t )header + dynamics->d_un.d_ptr );
      break;
    case DT_STRTAB:
      strtab = ( const char * )( ( uintptr_t )header + dynamics->d_un.d_ptr );
      break;
    case DT_SYMTAB:
      symtab = ( uintptr_t )buf + dynamics->d_un.d_ptr;
      break;
    case DT_STRSZ:
      strsz = dynamics->d_un.d_val;
      break;
    case DT_SYMENT:
      syment = dynamics->d_un.d_val;
      break;
    case DT_REL:
      reltab = ( uintptr_t )header + dynamics->d_un.d_ptr;
      break;
    case DT_RELSZ:
      relsz = dynamics->d_un.d_val;
      break;
    case DT_RELENT:
      relent = dynamics->d_un.d_val;
      break;
    case DT_JMPREL:
      jmpReltab = ( uintptr_t )header + dynamics->d_un.d_ptr;
      break;
    case DT_INIT_ARRAY:
      initArray = ( elf_voidf * )( dynamics->d_un.d_ptr + ( uintptr_t )buf );
      break;
    case DT_INIT_ARRAYSZ:
      initLength = dynamics->d_un.d_val / sizeof( Elf32_Addr );
      break;
    case DT_FINI_ARRAY:
      _ELF_H( handle )->finiArray = ( elf_voidf * )( dynamics->d_un.d_ptr + ( uintptr_t )buf );
      break;
    case DT_FINI_ARRAYSZ:
      _ELF_H( handle )->finiLength = dynamics->d_un.d_val / sizeof( Elf32_Addr );
      break;
    case DT_PLTGOT: /* Ignore these sections */
    case DT_INIT:
    case DT_FINI:
    case DT_PLTREL:
    case DT_TEXTREL:
    case 0x6FFFFFFA:
      break;
    default:
      _ELF_H( handle )->flags |= _ELF_ERROR;
      _ELF_H( handle )->error = _elf_error_d_tag;
      return;
    }
  }

  if ( !hash || !strtab || !symtab || !syment || !strsz ) {
    _ELF_H( handle )->flags |= _ELF_ERROR;
    _ELF_H( handle )->error = _elf_error_missing_entries;
    return;
  }

  /* Actual symbol resolution */
  /* any global symbols added by elf_mapsym are resolved here */
  for ( Elf32_Word ii = 1; ii < hash[1]; ii++ ) {
    Elf32_Sym * const symbol = ( Elf32_Sym * )( symtab + ( ii * syment ) );

    if ( symbol->st_shndx == SHN_UNDEF ) {
      void * const resolved = _elf_tree_find( _ELF_H( handle ), _elf_hash( strtab + symbol->st_name ) );

      if ( !resolved && !( ELF32_ST_BIND( symbol->st_info ) & STB_WEAK ) ) {
        _ELF_H( handle )->flags |= _ELF_ERROR;
        _ELF_H( handle )->error = _elf_error_unresolved_symbol;
        return;
      }

      symbol->st_shndx = SHN_ABS;
      symbol->st_value = ( Elf32_Addr )resolved;
    } else if (symbol->st_shndx < SHN_LORESERVE) {
      symbol->st_shndx = SHN_ABS;
      symbol->st_value = ( Elf32_Addr )( symbol->st_value + ( uintptr_t )buf );
    } else if ( symbol->st_shndx != SHN_ABS ) {
      _ELF_H( handle )->flags |= _ELF_ERROR;
      _ELF_H( handle )->error = _elf_error_unimplemented_st_shndx;
      return;
    }

    if ( ELF32_ST_BIND( symbol->st_info ) & STB_GLOBAL ) {
      elf_mapsym( handle, strtab + symbol->st_name, ( void * )symbol->st_value );
    }
  }

  /* Actual symbol relocation */
  if ( reltab ) {
    if ( !relsz || !relent ) {
      _ELF_H( handle )->flags |= _ELF_ERROR;
      _ELF_H( handle )->error = _elf_error_zero_sized_rel;
      return;
    }

    _elf_relocate( _ELF_H( handle ), buf, reltab, relent, relsz, symtab, syment );

    if ( _ELF_H( handle )->flags & _ELF_ERROR ) {
      return;
    }
  }

  /* Actual jump table relocation */
  if ( jmpReltab ) {
    _elf_relocate( _ELF_H( handle ), buf, jmpReltab, sizeof( Elf32_Rel ), pltrelsz, symtab, syment );

    if ( _ELF_H( handle )->flags & _ELF_ERROR ) {
      return;
    }
  }

  /* Once the ELF is linked, it is safe to call the library constructors */
  for ( Elf32_Word ii = 0; ii < initLength; ii++ ) {
    ( *initArray[ii] )();
  }
}

/**
 * Find ELF symbol
 * used to locate ELF symbols such as function pointers
 * must not be called before elf_link
 * @param  handle Valid, open ELF context
 * @param  symbol Cstring name that will be searched for within ELF
 * @return        Symbol data
 */
void * elf_dlsym( void * handle, const char * symbol ) {
  return _elf_tree_find( _ELF_H( handle ), _elf_hash( symbol ) );
}
