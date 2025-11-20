/* This code is an updated version of the sample code from "Computer Networks: A Systems
 * Approach," 5th Edition by Larry L. Peterson and Bruce S. Davis. Some code comes from
 * man pages, mostly getaddrinfo(3). */
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <iostream>

#define SERVER_PORT "5432"
#define MAX_LINE 256

using std::string;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;

/*
 * Lookup a host IP address and connect to it using service. Arguments match the first two
 * arguments to getaddrinfo(3).
 *
 * Arguments (both a std::string of ASCII text):
 *   host: The hostname or IP address of the machine running the server application.
 *   service: The port number of the server application on the machine identified by host.
 *
 * Returns a connected socket descriptor or -1 on error. Caller is responsible for closing
 * the returned socket.
 */
int lookup_and_connect( const string &host, const string &service );

int main( int argc, char *argv[] ) {
	string host;
	string buf;
	int s;

	if ( argc == 2 ) {
		host = argv[1];
	}
	else {
		cerr << "usage: " << argv[0] << " host" << endl;
		exit( 1 );
	}

	/* Lookup IP and connect to server */
	if ( ( s = lookup_and_connect( host, SERVER_PORT ) ) < 0 ) {
		exit( 1 );
	}

	/* Main loop: get and send lines of text */
	while ( cin >> buf ) {
		if ( send( s, buf.c_str(), buf.length()+1, 0 ) == -1 ) {
			perror( "stream-talk-client: send" );
			close( s );
			exit( 1 );
		}
	}

	close( s );

	return 0;
}

int lookup_and_connect( const string &host, const string &service ) {
	struct addrinfo hints;
	struct addrinfo *rp, *result;
	int s;

	/* Translate host name into peer's IP address */
	memset( &hints, 0, sizeof( hints ) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	if ( ( s = getaddrinfo( host.c_str(), service.c_str(), &hints, &result ) ) != 0 ) {
		cerr << "stream-talk-client: getaddrinfo: " << gai_strerror( s ) << endl;
		return -1;
	}

	/* Iterate through the address list and try to connect */
	for ( rp = result; rp != NULL; rp = rp->ai_next ) {
		if ( ( s = socket( rp->ai_family, rp->ai_socktype, rp->ai_protocol ) ) == -1 ) {
			continue;
		}

		if ( connect( s, rp->ai_addr, rp->ai_addrlen ) != -1 ) {
			break;
		}

		close( s );
	}
	if ( rp == NULL ) {
		perror( "stream-talk-client: connect" );
		return -1;
	}
	freeaddrinfo( result );

	return s;
}
