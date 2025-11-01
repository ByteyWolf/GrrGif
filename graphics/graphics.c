/* We use our own graphics library to prevent bloat.
 * This picks between the Windows and Linux implementations
 * of it. */

#ifdef _WIN32
#include "graphics_win32.c"
#else
#include "graphics_linux.c"
#endif
