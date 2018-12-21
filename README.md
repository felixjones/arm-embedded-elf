# arm-embedded-elf
Basic ELF shared object library loader for ARM embedded systems

# Usage #

Compile elf/elf.c and include elf/elf.h in your project.

An ELF file can be loaded from memory with:
```c
void * const handle = elf_dlmemopen( elf_file_in_memory, ELF_RTLD_DEFAULT );
error = elf_dlerror( handle );
if ( error ) {
  // Load error
} else {
  // Load success
}

// ELF must be closed even if load error occurs
elf_dlclose( handle );
```

Unresolved symbols must be added before linking:
```c
void my_function() {}
int my_integer = 32;

// Add function
elf_mapsym( handle, "my_function", ( void * )my_function );
error = elf_dlerror( handle );
if ( error ) {
  // ...
}

// Add int
elf_mapsym( handle, "my_integer", ( void * )&my_integer );
error = elf_dlerror( handle );
if ( error ) {
  // ...
}
```

Linking must be done into already allocated memory:
```c
void * memory = malloc( elf_lbounds( handle ) );

elf_link( handle, memory );
error = elf_dlerror( handle );
if ( error ) {
  // ...
}

// After ELF is used
elf_dlclose( handle );
free( memory );
```

After linking, symbols can be retrieved and used:
```c
typedef void ( * func_t )( void );

func_t func = ( func_t )elf_dlsym( handle, "elf_function" );

if ( !func ) {
  // Failed to find symbol
  error = elf_dlerror( handle );
  // ...
} else {
  // Use function
  ( *func )();
}
```

# Known issues #

## Limited implementation ##

This is a very basic implementation for ARM architecture.
Not all the ARM resolution types are implemented (only 3).
A lot of the ELF spec is not implemented, specifically classic init/fini support (modern arrays only).

## Unlinked Thumb functions need -mlong-calls ##

For some reason, without mlong-calls, calling Thumb functions from a Thumb or ARM compiled ELF results in attempting to run the Thumb function in an ARM context.
ELF shared objects that are likely to use Thumb functions must be compiled with -mlong-calls.

This does not affect ARM functions, so another work-around is to wrap Thumb functions with ARM and use that with elf_mapsym instead.  

## Memory only ##

File stream sources are not currently supported, but adding this shouldn't be too difficult.

With fmemopen the default system can likely be changed to expect file streams.

## No lazy loading ##

Link resolution happens for all symbols immediately, even if they are never used.