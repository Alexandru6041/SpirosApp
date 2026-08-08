#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <socket_helper.h>

/// @brief Reading the exact number of bytes inside a TCP connection.
/// @param sock > the socket we're receiving the data from
/// @param buf > the buffer we write the data into
/// @param n > the number of bytes we have to receive
/// @return returning -1 in case of error and 0 if OK.
int read_exact(int sock, void *buf, size_t n) {
    unsigned char *p = (unsigned char *) buf;
    size_t received = 0;

    while(received < n) {
        ssize_t r = recv(sock, p + received, n - received, 0);

        if(r <= 0)
            return -1;

        received += (size_t) r;
    }

    return 0;
}

/// @brief Assembling the data-buffer for sending via TCP connection.
/// @param buf > The buffer assemble the data into.
/// @param pos > The position pointer which will move along with the length of the word.
/// @param s   > The string we construct the data from.
int put_str(unsigned char *buf, int *pos, size_t bufsize, const char *s) {
    size_t len = strlen(s) + 1;
    if(*pos + len > bufsize) 
        return -1;
    
    memcpy(buf + *pos, s, len);
    *pos = *pos + (int)len;
    return 0;
}

/// @brief Reading the big endian 32-bit integer coming from the network.
/// @attention Inside the network the rule states that the most significant byte is last, whereas in the ARM/x86 architectures, the most significant byte is the first.
/// @param p > The pointer we'll be using for traversing the byte.
/// @return Returning the big-endian(network-order) ints converted into host-order(little-endian) ints readable by x86/ARM architectures.
int read_big_endian32(const unsigned char *p) {
    unsigned int v;
    memcpy(&v, p, 4);
    return (int)ntohl(v);
}

/// @brief Writing the big endian 32-bit integer for the network.
/// @attention Inside the network the rule states that the most significant byte is last, whereas in the ARM/x86 architectures, the most significant byte is the first.
/// @param buf > the buffer we assemble the outgoing data into.
/// @param pos > the position pointer which will walk through the buffer.
/// @param value > the value we are writting.
int write_big_endian32(unsigned char *buf, int *pos, size_t bufsize, unsigned int value) {
    if(*pos + 4 > (int) bufsize)
        return -1;
    
    unsigned int net = htonl(value);
    memcpy(buf + *pos, &net, 4);
    *pos += 4;

    return 0;
}

/// @brief Reading the big endian 16-bit integer data from the network for table rows & columns.
/// @attention Inside the network the rule states that the most significant byte is last, whereas in the ARM/x86 arhitectures, the most significant byte is the first.
/// @param p The pointer we'll be using for traversing the byte.
/// @return Returning the big-endian(network-order) ints converted into host-order(little-endian) ints readable by x86/ARM architectures.
unsigned short read_big_endian16(const unsigned char *p) {
    unsigned short v;
    memcpy(&v, p, 2);
    return ntohs(v);
}