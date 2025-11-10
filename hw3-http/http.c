#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include "http.h"

//---------------------------------------------------------------------------------
// TODO:  Documentation
//
// Note that this module includes a number of helper functions to support this
// assignment.  YOU DO NOT NEED TO MODIFY ANY OF THIS CODE.  What you need to do
// is to appropriately document the socket_connect(), get_http_header_len(), and
// get_http_content_len() functions.
//
// NOTE:  I am not looking for a line-by-line set of comments.  I am looking for
//        a comment block at the top of each function that clearly highlights you
//        understanding about how the function works and that you researched the
//        function calls that I used.  You may (and likely should) add additional
//        comments within the function body itself highlighting key aspects of
//        what is going on.
//
// There is also an optional extra credit activity at the end of this function. If
// you partake, you need to rewrite the body of this function with a more optimal
// implementation. See the directions for this if you want to take on the extra
// credit.
//--------------------------------------------------------------------------------

char *strcasestr(const char *s, const char *find)
{
    char c, sc;
    size_t len;

    if ((c = *find++) != 0) {
        c = tolower((unsigned char)c);
        len = strlen(find);
        do {
            do {
                if ((sc = *s++) == 0)
                    return (NULL);
            } while ((char)tolower((unsigned char)sc) != c);
        } while (strncasecmp(s, find, len) != 0);
        s--;
    }
    return ((char *)s);
}

char *strnstr(const char *s, const char *find, size_t slen)
{
    char c, sc;
    size_t len;

    if ((c = *find++) != '\0') {
        len = strlen(find);
        do {
            do {
                if ((sc = *s++) == '\0' || slen-- < 1)
                    return (NULL);
            } while (sc != c);
            if (len > slen)
                return (NULL);
        } while (strncmp(s, find, len) != 0);
        s--;
    }
    return ((char *)s);
}


/**
 * socket_connect() - Establishes a TCP connection to a remote HTTP server
 * @host The hostname or domain name of the server (e.g., "httpbin.org" or "cci-p141.cci.drexel.edu" )
 * @port The port number to connect to (typically 80 for HTTP)
 *
 * This function performs the complete sequence of operations needed to establish
 * a TCP connection to a remote server:
 *
 * 1. DNS Resolution: Uses gethostbyname() to resolve the hostname to an IP address.
 *    gethostbyname() queries DNS servers and returns a hostent structure containing
 *    the IP address(es) associated with the hostname.
 *
 * 2. Address Structure Setup: Constructs a sockaddr_in struct with:
 *    - IP address: Copied from the hostent struct using bcopy()
 *    - Port: Converted to network byte order (big-endian) using htons()
 *    - Address family: Set to AF_INET for IPv4
 *
 * 3. Socket Creation: Calls socket() with:
 *    - PF_INET: IPv4 protocol family
 *    - SOCK_STREAM: TCP
 *    - 0: Let system choose appropriate protocol (TCP for SOCK_STREAM)
 *
 * 4. Connection Establishment: Uses connect() to initiate the TCP three-way
 *    handshake (SYN, SYN-ACK, ACK) with the server. This blocks until the
 *    connection is established or fails.
 *
 * Return: Socket file descriptor (positive int) on success, or negative on failure:
 *         -2 if DNS resolution fails, -1 if socket creation or connection fails
 */
int socket_connect(const char *host, uint16_t port){
    struct hostent *hp;
    struct sockaddr_in addr;
    int sock;

    if((hp = gethostbyname(host)) == NULL){
        herror("gethostbyname");
        return -2;
    }


    bcopy(hp->h_addr_list[0], &addr.sin_addr, hp->h_length);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    sock = socket(PF_INET, SOCK_STREAM, 0);

    if(sock == -1){
        perror("socket");
        return -1;
    }

    if(connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1){
        perror("connect");
        close(sock);
        return -1;
    }

    return sock;
}


/**
 * get_http_header_len() - Calculates the length of the HTTP header in bytes
 * @http_buff Buffer containing the HTTP response (header + body)
 * @http_buff_len Total length of data in the buffer
 *
 * HTTP headers are terminated by a blank line, specifically the sequence "\r\n\r\n"
 * (carriage return, line feed, carriage return, line feed). This function locates
 * that terminator to determine where the header ends and the body begins.
 *
 * 1. Uses strnstr() to search for HTTP_HEADER_END ("\r\n\r\n") within the buffer.
 *    strnstr() is safer than strstr() because it limits the search to http_buff_len
 *    bytes, preventing reading beyond the valid data.
 *
 * 2. If found, end_ptr points to the start of "\r\n\r\n" in the buffer.
 *
 * 3. Calculates header length using pointer arithmetic:
 *    - (end_ptr - http_buff) gives the number of bytes from buffer start to "\r\n\r\n"
 *    - Adding strlen(HTTP_HEADER_END) includes the terminator itself in the count
 *    - This gives the complete header length including the blank line separator
 *
 * Ex: If http_buff contains "HTTP/1.1 200 OK\r\n...headers...\r\n\r\nbody..."
 *     and end_ptr points to the first '\r' of "\r\n\r\n", then
 *     header_len = (position of "\r\n\r\n") + 4 (length of "\r\n\r\n")
 *
 * Return: Header length in bytes on success, -1 if header terminator not found
 */
int get_http_header_len(char *http_buff, int http_buff_len){
    char *end_ptr;
    int header_len = 0;
    end_ptr = strnstr(http_buff,HTTP_HEADER_END,http_buff_len);

    if (end_ptr == NULL) {
        fprintf(stderr, "Could not find the end of the HTTP header\n");
        return -1;
    }

    header_len = (end_ptr - http_buff) + strlen(HTTP_HEADER_END);

    return header_len;
}


/**
 * get_http_content_len() - Extracts the Content-Length value from HTTP headers
 * @http_buff Buffer containing the HTTP response
 * @http_header_len Length of the header portion
 *
 * The Content-Length header specifies the size of the HTTP response body in bytes.
 * This is critical for Keep-Alive connections because it needs to know exactly how
 * many bytes to receive before the body ends so it can send another request.
 *
 * HTTP headers have the format: "Header-Name: value\r\n"
 * This function parses the header section line by line to find "Content-Length: <value>"
 *
 * 1. Initialize pointers:
 *    - next_header_line: Points to the current line being processed
 *    - end_header_buff: Marks the end of the header section
 *
 * 2. Loop through each header line:
 *    a. Clear the header_line buffer with bzero() to rid previous data
 *    b. Use sscanf() with format "%[^\r\n]s" to read characters until \r or \n
 *       This extracts one complete header line
 *    c. Use strcasestr() to check if this line contains "Content-Length"
 *       case-insensitive
 *
 * 3. If Content-Length header found:
 *    a. Use strchr() to locate the ':' delimiter in the header line
 *    b. Advance pointer by 1 to skip the ':' and point to the value
 *    c. Use atoi() to convert the string value to an integer
 *    d. Return the content length
 *
 * 4. Advance to next line:
 *    - Add strlen(header_line) to skip the current line content
 *    - Add strlen(HTTP_HEADER_EOL) (2 bytes for "\r\n") to skip the line ending
 *
 * Example: For "Content-Length: 512\r\n", strchr finds ':', header_value points
 *          to " 512", and atoi() converts " 512" to integer 512
 *
 * Return: Content length in bytes, or 0 if Content-Length header not found
 *         (0 is valid per HTTP spec, indicating no body)
 */
int get_http_content_len(char *http_buff, int http_header_len){
    char header_line[MAX_HEADER_LINE];

    char *next_header_line = http_buff;
    char *end_header_buff = http_buff + http_header_len;

    while (next_header_line < end_header_buff){
        bzero(header_line,sizeof(header_line));
        sscanf(next_header_line,"%[^\r\n]s", header_line);

        char *isCLHeader = strcasestr(header_line,CL_HEADER);
        if(isCLHeader != NULL){
            char *header_value_start = strchr(header_line, HTTP_HEADER_DELIM);
            if (header_value_start != NULL){
                char *header_value = header_value_start + 1;
                int content_len = atoi(header_value);
                return content_len;
            }
        }
        next_header_line += strlen(header_line) + strlen(HTTP_HEADER_EOL);
    }
    fprintf(stderr,"Did not find content length\n");
    return 0;
}

//This function just prints the header, it might be helpful for your debugging
//You dont need to document this or do anything with it, its self explanitory. :-)
void print_header(char *http_buff, int http_header_len){
    fprintf(stdout, "%.*s\n",http_header_len,http_buff);
}

//--------------------------------------------------------------------------------------
//EXTRA CREDIT - 10 pts - READ BELOW
//
// Implement a function that processes the header in one pass to figure out BOTH the
// header length and the content length.  I provided an implementation below just to
// highlight what I DONT WANT, in that we are making 2 passes over the buffer to determine
// the header and content length.
//
// To get extra credit, you must process the buffer ONCE getting both the header and content
// length.  Note that you are also free to change the function signature, or use the one I have
// that is passing both of the values back via pointers.  If you change the interface dont forget
// to change the signature in the http.h header file :-).  You also need to update client-ka.c to
// use this function to get full extra credit.
//--------------------------------------------------------------------------------------
int process_http_header(char *http_buff, int http_buff_len, int *header_len, int *content_len){
    int h_len, c_len = 0;
    h_len = get_http_header_len(http_buff, http_buff_len);
    if (h_len < 0) {
        *header_len = 0;
        *content_len = 0;
        return -1;
    }
    c_len = get_http_content_len(http_buff, http_buff_len);
    if (c_len < 0) {
        *header_len = 0;
        *content_len = 0;
        return -1;
    }

    *header_len = h_len;
    *content_len = c_len;
    return 0; //success
}
