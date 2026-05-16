#include "utest.h"

#include <atomic>
#include <thread>

#include <eepp/network/http.hpp>
#include <eepp/network/tcplistener.hpp>
#include <eepp/network/tcpsocket.hpp>

using namespace EE;
using namespace EE::Network;

UTEST( Http, responseHeaderLineLargerThanReceiveBuffer ) {
	TcpListener listener;
	ASSERT_EQ( listener.listen( Socket::AnyPort, IpAddress::LocalHost ), Socket::Done );

	std::atomic<bool> serverOk{ false };
	std::thread server( [&listener, &serverOk] {
		TcpSocket client;
		if ( listener.accept( client ) != Socket::Done )
			return;

		// Read and discard the HTTP request headers.
		std::string request;
		char buffer[1024];
		std::size_t received = 0;

		while ( request.find( "\r\n\r\n" ) == std::string::npos ) {
			Socket::Status st = client.receive( buffer, sizeof( buffer ), received );
			if ( st != Socket::Done )
				return;
			request.append( buffer, received );
		}

		const std::string response = "HTTP/1.1 200 OK\r\n"
									 "X-Long: " +
									 std::string( 17000, 'a' ) +
									 "\r\nContent-Length: 5\r\n"
									 "Connection: close\r\n"
									 "\r\n"
									 "hello";

		serverOk = client.send( response.data(), response.size() ) == Socket::Done;

		client.disconnect();
	} );

	Http http( "127.0.0.1", listener.getLocalPort() );
	Http::Response response = http.sendRequest( Http::Request( "/" ), Seconds( 5 ) );

	server.join();
	listener.close();

	EXPECT_TRUE( serverOk );
	EXPECT_EQ( response.getStatus(), Http::Response::Ok );
	EXPECT_TRUE( response.getBody() == "hello" );
}
