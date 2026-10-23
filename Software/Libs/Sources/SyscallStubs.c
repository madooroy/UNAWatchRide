/*
 * Weak newlib syscall stubs for the service.
 *
 * Some newlib builds (the distro arm-none-eabi-gcc) pull in _write, _read,
 * _close and _lseek through printf-family reentrancy wrappers, even though
 * the app never does file or console I/O through them. ST's toolchain links
 * without these; with others the service fails to link. They are weak, so a
 * toolchain that provides its own wins.
 */
#include <errno.h>

__attribute__((weak)) int _write(int fd, const char* buf, int len) { (void)fd; (void)buf; (void)len; errno = ENOSYS; return -1; }
__attribute__((weak)) int _read(int fd, char* buf, int len)        { (void)fd; (void)buf; (void)len; errno = ENOSYS; return -1; }
__attribute__((weak)) int _close(int fd)                           { (void)fd; errno = ENOSYS; return -1; }
__attribute__((weak)) int _lseek(int fd, int off, int whence)      { (void)fd; (void)off; (void)whence; errno = ENOSYS; return -1; }
