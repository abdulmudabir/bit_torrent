
// standard libraries
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// libraries for networking stuff
#include <netinet/in.h>				// for struct sockaddr_in
#include <netinet/ip.h>				// ip header library (must come before ip_icmp.h)
#include <netinet/ip_icmp.h> 		// icmp header
#include <arpa/inet.h>				// internet address library
#include <netdb.h>					// for hostent struct
#include <sys/socket.h>				// for socket operations
#include <sys/types.h>
#include <signal.h>

#include "bt_lib.h"
#include "bt_setup.h"

int main (int argc, char * argv[]){

    bt_args_t bt_args;	// structure to capture command-line arguments
    int i;	// loop iterator

    parse_args(&bt_args, argc, argv);

    if (bt_args.verbose) {	// if verbose mode is requested
		printf("Args:\n");
        printf("verbose: %d\n", bt_args.verbose);
        printf("save_file: %s\n", bt_args.save_file);	// display name of file to save to
        printf("log_file: %s\n", bt_args.log_file);		// display name of file to log information to
        printf("torrent_file: %s\n", bt_args.torrent_file);	// metainfo or torrent file being used by bt client

		// print information of all peers
        for (i = 0; i < MAX_CONNECTIONS; i++) {
			if(bt_args.peers[i] != NULL)
                print_peer(bt_args.peers[i]);
        }
    }

    //read and parse the torrent file here
    parseTorrentFile(&bt_args);

    if (bt_args.verbose) {
        // print out the torrent file arguments here
    }

    // main client loop
    //printf("Starting Main Loop\n");
    /*
	while(1){

        //try to accept incoming connection from new peer
             
        
        // poll current peers for incoming traffic
        //     write pieces to files
        //     update peers' choke or unchoke status
        //     responses to have/havenots/interested etc.
        
        // for peers that are not choked
        //     request pieces from outcoming traffic

        // check livelness of peers and replace dead (or useless) peers
        // with new potentially useful peers
        
        // update peers, 

    }
	*/
	
    return 0;
}
