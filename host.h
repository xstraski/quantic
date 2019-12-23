// host.h

#ifndef HOST_H
#define HOST_H

/*
========================================================================================================================
HOST INTERFACE

Host is a program that is responsible for running the server and its subsystems.
Host can run whether a dedicated server, or a listen server, also known as "localhost".
Being dedicated it executes only a server side, communicating with connected clients via network protocol.
Being listen it executes both client and server sides, which exchange messages between each other by
so called "loopback" intermediate buffer, and/or communicating with external clients via network protocol.

If -dedicated command line argument is specified, the host runs as a dedicated server, otherwise
it runs as a listen server. This argument check is optionally implemented, depending on a target platform,
because the program might not be supported to run a dedicated server in a particular platforms
(f.e. game consoles, mobile devices).

If SERVERONLY is compile-time defined, only server part of code gets built, and the executable
will be dedicated server only.

Returns QCODE_NORMAL to the OS when quits with no error(s).
Returns QCODE_ERROR/QCODE_RECURISVE to the OS when encounters an unrecoverable critical (and recursive) error.
========================================================================================================================
*/

// H_NAME is a short name of the product, used for internal needs, required to have no spaces and/or special symbols
// H_NAME_LONG is a product complete title, used and shown only in user-visible area
#define H_NAME      "Quantic"
#define H_NAME_LONG "Quantic: Work in Progress"

// host program version
#define H_VERSION_MAJOR 0
#define H_VERSION_MINOR 1

// host program development start data, used for build number calculations, must be represented
// as a string in format "Mmm ?D YYYY", f.e. "May 18 2011" or "Oct  9 2018"
#define H_DEVDATE "16 Nov 2019"

// host program memory restrictions, telling how much memory is required at least to run the program,
// and how much memory the program can gain at all
#define H_MIN_MEMORY (4 * 1024 * 1024 * 1024)
#define H_MAX_MEMORY (16 * 1024 * 1024 * 1024)

// host program filesystem paths
// H_BASEDIR is a name of the directory located in workpath containing main resources used by the client/server
// H_USERDIR is a name of the directory located somewhere in system-specific locating for user-specific program data
#define H_BASEDIR "q0"
#define H_USERDIR "quantic"

// host program execution parameters, gets filled by sys layer and passed to Host_Init
typedef struct {
	void * membase;                            // base address of the memory chunk prepared for dynamic allocations
	size_t memsize;                            // size of the memory chunk pointed above

	char * rootpath;                           // root path that contains all needed directories, 0 to be system layer's currentpath
} hostparams_t;

/*
=================================================================

Host program execution interface

=================================================================
*/
void Host_Init(hostparams_t *params);
void Host_Shutdown(qboolean_t aftererror);
void Host_Frame(void);

extern qboolean_t host_dedicated;

// timings
extern unsigned   host_frames;
extern qw_t       host_frameclocks;
extern float      host_framesecs, host_framerate;

#endif // #ifndef HOST_H
