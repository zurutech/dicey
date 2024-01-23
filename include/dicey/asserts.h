#if !defined(EGOCMWVDQU_ASSERTS_H)
#define EGOCMWVDQU_ASSERTS_H

// This header is probably unnecessary. Big endian is dead, has been dead for a while, and it is not coming back.
// Still, if some smarty pants has the sudden urge to build this on a weird platform that Unreeal clearly doesn't support,
// this header will be here to stop them.

#if defined (_MSC_VER) && (defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM) || defined(_M_ARM64))
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__) || defined(__arm__) || defined(__aarch64__))
#else
#error "Unsupported platform"
#endif

#endif // EGOCMWVDQU_ASSERTS_H
