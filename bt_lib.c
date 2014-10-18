#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>	//internet address library
#include <netdb.h>  // for struct hostent
#include <sys/socket.h>
#include <sys/types.h>

#include <sys/stat.h>
#include <arpa/inet.h>

#include <openssl/sha.h>	// for using SHA1() function for hashing

#include "bt_lib.h"
#include "bt_setup.h"

#define BUF_LEN 1024

/**
 * calc_id() clubs the IP address and port number of peer into a single string. Then, 
 * fills the peer 'id' with a SHA1 digest computed on the clubbed string
 */
void calc_id(char *ip, unsigned short port, char *id) {
    char data[256];		// data for which SHA1 digest is to be made
    int len;			// length of data
    
    /* format print to store up to 256 bytes (characters) including the terminating '\0' (null) character in 'data'.
     * snprintf() returns the index of the char that it places the '\0' at; basically returns the length of the string copied
     */
    len = snprintf(data, 256, "%s%u", ip, port);    // example data = localhost7000

    /* id is just the SHA1 of the ip and port string
     * SHA1()'s signature:
     * unsigned char *SHA1(const unsigned char *dataString, unsigned long dataLength, unsigned char *SHA1digest);
     */
    SHA1( (unsigned char *) data, len, (unsigned char *) id );

    return;
}

/**
 * init_peer(peer_t *peer, int id, char *ip, unsigned short port) -> int
 *
 * initialize the peer_t structure peer with an id, ip address, and a
 * port. Further, it will set up the sockaddr such that a socket
 * connection can be more easily established.
 *
 * Return: 0 on success, negative values on failure. Will exit on bad
 * ip address.
 *     
 **/
int init_peer(peer_t *peer, char *id, char *ip, unsigned short port) {
        
    struct hostent *hostinfo;	// instantiate hostent struct that contains information like IP address, host name, etc.
	
    // set the host id and port for reference
    memcpy(peer->id, id, ID_SIZE);  // SHA1 hash of peer IP & port is stored as peer struct's 'id'
    peer->port = port;
        
    // get the host by name
    if( (hostinfo = gethostbyname(ip)) == NULL ) {
        perror("gethostbyname failure, no such host?");
        herror("gethostbyname");	// prints error message associated with the current host
        exit(1);
    }
    
    // zero out the peer's sock address structure before filling in its details
    bzero(&(peer->sockaddr), sizeof(peer->sockaddr));
            
    // set the family to AF_INET, i.e., Internet Addressing
    peer->sockaddr.sin_family = AF_INET;
	
    // copy the address from h_addr field of 'hostinfo' to the peer's socket address
    bcopy( (char *) (hostinfo->h_addr), 
                (char *) &(peer->sockaddr.sin_addr.s_addr),
                hostinfo->h_length );
        
    // encode the port to network-byte order to store in sockaddr_in struct
    peer->sockaddr.sin_port = htons(port);
    
    return 0;	// success
}

/**
 * print_peer(peer_t *peer) -> void
 *
 * print out debug info of a peer
 *
 **/
void print_peer(peer_t *peer) {
    int i;	// loop iterator

    if (peer) {
		printf("peer: %s:%u ", 
				inet_ntoa(peer->sockaddr.sin_addr),		// converts a network address to a dots-and-numbers format string
				peer->port);
        printf("id: ");
        
		for (i = 0; i < ID_SIZE; i++) {
            printf("%02x", peer->id[i]);	// convert to 40-byte hex string
        }
		
        printf("\n");
    }
}

void get_hashhex(unsigned char str[]) {

    int i;
    for (i = 0; i < ID_SIZE; i++) {
            printf("%02x", str[i]);    // convert to 40-byte hex string
    }

}

/**
 * init_seeder() documentation TO DO
 **/
void init_seeder(bt_args_t *bt_args) {
    char *parse_bind_str;   // temporary string
    char *token;
    char delim[] = ":"; // delimiter
    char *ip;   // to store IP addr
    char id[20];    // to store SHA1 hash into
    unsigned short port;    // to store port
    int i;  // loop iterator variable

    // copy bt_args.bind_info to parse_bind_str to avoid losing original bt_args.bind_info
    parse_bind_str = malloc(strlen(bt_args->bind_info) + 1);
    memset( parse_bind_str, 0x00, (strlen(bt_args->bind_info) + 1) );   // zero-out parse_bind_str
    strncpy(parse_bind_str, bt_args->bind_info, strlen(bt_args->bind_info));
    // printf("testing, parse_bind_str: '%s'\n", parse_bind_str);

    for ( token = strtok(parse_bind_str, delim), i = 0; token; token = strtok(NULL, delim), i++ ) {
        switch(i) {
            case 0:
                ip = token;
                break;
            case 1:
                port = atoi(token);
                break;
            default:
                break;
        }
    }

    if (i < 2) {    // too few arguments after '-b' flag
        fprintf(stderr, "ERROR: Not enough values in '%s'\n", bt_args->bind_info);
        usage(stderr);
        exit(1);
    }

    if (token) {    // token should be NULL by this time
        fprintf(stderr, "ERROR: Too many values in '%s'\n", bt_args->bind_info);
        usage(stderr);
        exit(1);
    }

    // calculate SHA1 hash of the concatenation of binding machine's IPaddr and port
    calc_id(ip, port, id);    // calculate bt client's ID
    memcpy(bt_args->id, id, 20);

    seeder_listen(ip, port, bt_args);    /* make seeder listen to incoming leecher connections */
}

/**
 * int seeder_listen(char *, unsigned short, bt_args_t *) documentation TO DO
 *
--------------------------sockaddr structures--------------------------------------------
struct sockaddr {
    unsigned short    sa_family;    // address family, AF_xxx
    char              sa_data[14];  // 14 bytes of protocol address
};

// IPv4 AF_INET sockets:

struct sockaddr_in {
    short            sin_family;   // e.g. AF_INET, AF_INET6
    unsigned short   sin_port;     // e.g. htons(3490)
    struct in_addr   sin_addr;     // see struct in_addr, below
    char             sin_zero[8];  // zero this if you want to
};

struct in_addr {
    unsigned long s_addr;          // load with inet_pton()
};
----------------------------------------------------------------------------------------
 * 
 **/
void seeder_listen(char *ip, unsigned short port, bt_args_t *bt_args) {

    struct hostent *hostinfo;   // store network-related information of host machine
    int i;  // loop iterator variable

    if( !(hostinfo = gethostbyname(ip)) ) { // gethostbyname() returns the 'hostent' structure or NULL on failure
        fprintf(stderr,"ERROR: Invalid host name '%s' specified\n", ip);
        usage(stderr);
        exit(1);
    }

    struct sockaddr_in seeder_addr; // structure containing all network-related seeder information

    // populate seeder_addr structure
    seeder_addr.sin_family = hostinfo->h_addrtype;
    seeder_addr.sin_port = htons(port);
    memmove( (char *) &(seeder_addr.sin_addr.s_addr), (char *) hostinfo->h_addr, hostinfo->h_length );

    int seeder_sock;    // seeder's connection-welcoming socket to its leechers
    int new_seeder_sock;    // new seeder allocated socket to exchange data with leechers
    int seeder_listen_status;  // check whether seeder is listening on its socket or no
    char buffer[BUF_LEN];   // a buffer of size 1024 bytes at max to read or write data
    ssize_t bytes_read, total_bytes_read = 0;  // bytes read; total number of bytes read by seeder; ssize_t defined in <unistd.h>

    // create seeder's listening TCP stream socket
    if ( (seeder_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 ) {
        fprintf(stderr, "ERROR: Seeder was unable to set up a listening socket.\n");
        exit(1);
    }

    // bind the seeder's connection-welcoming socket to the specific port number specified in 'peer'
    if ( bind(seeder_sock, (struct sockaddr *) &seeder_addr, sizeof(seeder_addr))  < 0 ) {    // NOTE: operator '->' has precedence over '->' operator
        fprintf(stderr, "ERROR: Seeder encountered error in binding its listening socket.\n");
        exit(1);
    }

    // on successful seeder socket binding, seeder should listen to incoming leecher connections
    if ( (seeder_listen_status = listen(seeder_sock, MAX_CONNECTIONS)) < 0 ) { // set maximum number of incoming leecher connections to 5
        fprintf(stderr, "ERROR: Seeder encountered error while trying to listen to incoming leecher connections.\n");
        exit(1);
    }

    printf("LISTENING on peer: %s", inet_ntoa(seeder_addr.sin_addr));   // print dots-and-numbers version of host
    printf("; id: ");
    for (i = 0; i < ID_SIZE; i++) {
        printf("%02x", bt_args->id[i]);
    }
    printf("\n");   // line feed

    // store connecting leecher's information
    struct sockaddr_in leecher_info;    // to fill in all relevant leecher information
    unsigned int leecher_length = sizeof(leecher_info);
    if ( ( new_seeder_sock = accept(seeder_sock, (struct sockaddr *) &leecher_info, &leecher_length) ) < 0 ) {  // seeder sets up new socket to exchange data with leecher
        fprintf(stderr, "ERROR: Seeder could not set up a new socket to communicate with leecher.\n");
        exit(1);
    }

    memset(buffer, 0x00, BUF_LEN);  // zero-out buffer before using it

    while ( (bytes_read = read(new_seeder_sock, buffer, BUF_LEN)) != 0 ) {  // read data from new seeder socker into buffer;
                                                                            // read until amount to be read is 0 i.e. until no more data is available is to read
        total_bytes_read += bytes_read;

        memset(buffer, 0x00, BUF_LEN);
    }

    // close seeder sockets
    close(new_seeder_sock);
    close(seeder_sock);
}

/**
 * init_leecher() documentation TO DO
 **/
void init_leecher(peer_t *peer) {
    int leecher_sock;   // to create a leecher socket to communicate with seeder
    char buffer[BUF_LEN];    // read/write buffer
    ssize_t bytesWritten = 0;    // track number of bytes read or written

    memset(buffer, 0x00, BUF_LEN); // zero-out buffer

    // create the TCP stream socket
    if ( ( leecher_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) ) < 0 ) {   // non-negative socket() return value indicates failure in creating the socket
        fprintf(stderr, "ERROR: TCP leecher socket creation failed.");
        exit(1);
    }
    
    // establish connection with leecher by calling connect on the seeder's TCP socket, sockfd
    if ( connect( leecher_sock, (struct sockaddr *) &peer->sockaddr, sizeof(peer->sockaddr) ) < 0 ) {
        fprintf(stderr, "ERROR: Connection could not be established to seeder id: ");
        get_hashhex(peer->id);
        exit(1);
    }

    printf("CONNECTION ESTABLISHED to seeder from PEER: %s:%u; peer id: ", inet_ntoa(peer->sockaddr.sin_addr), peer->port);
    get_hashhex(peer->id);
    printf("\n");   // line feed

    // send all communication from leecher to seeder hereforth
    while ( ( bytesWritten = write(leecher_sock, buffer, strlen(buffer)) ) ) {
        printf("testing, you are here!");
    }

    close(leecher_sock);
    printf("CONNECTION CLOSED to seeder by PEER: %s:%u; peer id: ", inet_ntoa(peer->sockaddr.sin_addr), peer->port);
    get_hashhex(peer->id);
    printf("\n");   // line feed
    
}

void fill_handshake_info(peer_t *peer, bt_info_t *bt_info) {

    // null all handshake_info_t structure contents at first
    memset(peer->hs_info.protocol, 0x00, sizeof(peer->hs_info.protocol));
    memset(peer->hs_info.reserved, 0x00, sizeof(peer->hs_info.reserved));
    memset(peer->hs_info.info_hash, 0x00, sizeof(peer->hs_info.info_hash));
    memset(peer->hs_info.peerID, 0x00, sizeof(peer->hs_info.peerID));

    char temp[20];  // temporary char array
    sprintf(temp, "%c", 19);    // first byte is ASII character 19 (decimal)
    strcat(temp, "BitTorrent Protocol");    // concatenate '19' byte with "BitTorrent Protocol" string
    
    // fill those 20-bytes as protocol information in peer's handshake information
    strcpy( peer->hs_info.protocol, temp );   // peer->sockaddr.sin_addr.s_addr
    printf("testing, protocol: '%s'\n", peer->hs_info.protocol);

    // do not need to 'reserved bytes', so leave them as null for now

    // printf("testing, bt_info->name: '%s'\n", bt_args->bt_info->name);
    // calculate SHA1 of "suggested file name" specified in *.torrent file
    // SHA1( (unsigned char *) bt_info->name, strlen(bt_info->name), (unsigned char *) peer->hs_info.info_hash );

}