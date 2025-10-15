
## HTTP 2 client

The HTTP 2 client cannot be used concurrently from multiple threads. If the executor runs on multiple threads, it must be wrapped in a `strand<Executor>` and this strand executor must be used instead.

#### Constructing a Client

```cpp
#include <rad/net/http2/http2_client.h>
// include one of the ssl implementations
#include <rad/net/ssl/openssl_ctx.h>
// include the io timer executor
#include <rad/async/io_loop.h>

using namespace rad;
namespace ssl = net::ssl;
namespace http2 = net::http2;

task<> make_requests(io_loop& loop, ssl::context_base& ctx,
    std::string host, std::vector<std::string> urls) {
    // construct a closed client
    http2::client client{ loop, ctx };
    
    // construct a client with ssl stream
    // the ssl handshake must have been done
    // and protocol h2 has been selected with ALPN
    ssl::stream<tcp::socket> stream = ...;
    http2::client client{ loop, host,  std::move(stream)};

    // construct a client with TCP stream
    // the TCP socket must have been connected
    tcp::socket stream = ...;
    http2::client client{ loop, ctx, host,  std::move(stream)};
}
```

#### Completing the HTTP 2 Handshake

When the client is constructed with an SSL or TCP stream, complete the handshake:

```cpp
co_await client.handshake();
```

If the client is closed, use `connect` instead:

```cpp
// all host resolution is done with system resolver
// connect to HTTPS host at port 443
co_await client.connect("https://example.com");
// connect to HTTP host at port 80
co_await client.connect("http://example.com");
// connect to HTTPS host at port 555
co_await client.connect("https://example.com:555");

// connect to host example.com at port 789 and
// specify whether to use SSL or not
const bool is_ssl = true; // or false
co_await client.connect("example.com", 789, is_ssl);

// connect to 23.192.228.80 at port 443 and
// specify whether to use SSL or not
// the host is still needed for :authority header
co_await client.connect("example.com", 
    net::ipv4_endpoint{ { 23, 192, 228, 80 }, 443 }, is_ssl);

// connect to one of the IP endpoints and
// specify whether to use SSL or not
// the host is still needed for :authority header
// the endpoints will be tried one by one in order
// until a connection succeeds
std::array<net::endpoint, 2> targets = {
    net::endpoint{ "23.192.228.80", 443 },
    net::endpoint{ "23.192.228.84", 555 }
};
co_await client.connect("example.com", targets, is_ssl);
```
#### Making Requests and Receiving Responses

```cpp
// The request method prepares a request and returns
// a task to send and wait for the response

http2::headers hdrs;
hdrs.insert("Some-Header", "Some-Value");
const_buffer req_body = buffer(nullptr);
http2::response res;
// the response body will be stored in this buffer
std::string res_body;
task<> req = client.request("https://example.com/path/",
    http2::verb::get, std::move(hdrs), req_body,
    res, dynamic_buffer(res_body));
// start the request and wait for the response
co_await req;
std::cout << "Response headers: " << res.serialize();
std::cout << "Response body: " << res_body;

// to store the response body in the response struct:
co_await client.request("https://example.com/path/",
    http2::verb::get, std::move(hdrs), req_body, res);

std::cout << "Response headers: " << res.serialize();
std::cout << "Response body: " << res.body;

// to get a response from the task:
auto res = co_await client.request("https://example.com/path/",
    http2::verb::get, std::move(hdrs), req_body,
    dynamic_buffer(res_body));
std::cout << "Response headers: " << res.serialize();
std::cout << "Response body: " << res_body;

// to get a response and body in one struct from the task:
auto res = co_await client.request("https://example.com/path/",
    http2::verb::get, std::move(hdrs), req_body);

std::cout << "Response headers: " << res.serialize();
std::cout << "Response body: " << res.body;
```

#### Sending Multiple Requests in Parallel

```cpp
#include <rad/coro/when_all.h>

std::vector<task<>> requests;
// prepare the requests
for (auto i : range(100)) {
    requests.emplace_back(client.request(...));
}
// send the requests and wait for them all
co_await when_all(std::move(requests));

// continue make requests again
for (auto i : range(100)) {
    requests.emplace_back(client.request(...));
}
// send the requests and wait for them all
co_await when_all(std::move(requests));
```
It is also possible to spawn the requests:

```cpp
#include <rad/coro/spawn.h>
#include <rad/coro/when_all.h>

for (auto i : range(100)) {
    spawn(loop, client.request(...), 
        [](const http2::response& res) {},
        [](std::exception_ptr error) {});
}

// while the spawned requests are in flight issue more requests!
std::vector<task<>> requests;
// prepare the requests
for (auto i : range(100)) {
    requests.emplace_back(client.request(...));
}
// send the requests and wait for them all
co_await when_all(std::move(requests));
```
#### Connection Management

To send a PING and wait for its ACK:

```cpp
co_await client.ping();
```

To gracefully close the connection by sending a GOAWAY frame:

```cpp
co_await client.async_close(http2::error::no_error);
```

For ungraceful close:

```cpp
client.stop();
```

#### Configuring Timeouts

```cpp
using namespace std::chrono;
http2::endpoint_timeout timeouts;
timeouts.handshake_timeout = seconds(10);
timeouts.idle_timeout = seconds(20);
// to send PINGs in the background for idle connections
timeouts.keep_alive_pings = true;

client.timeouts(timeouts);

// to disable timeouts
client.disable_timeouts();
```

#### Additional Settings

```cpp
// set max acceptable response headers size
client.max_headers_size(32 * 1024);
// set max acceptable response body size
client.max_body_size(1024 * 1024);
// if non final OK response is received, treat this as error
client.accept_non_ok_responses(false);
// set max successive CONTINUATION frames
// limiting successive CONTINUATION frames is
// necessary to protect against CONTINUATION frames flood.
client.max_continuation_frames(10);
// don't follow 3XX redirection responses
client.follow_redirection(false);
// limit the max successive 3XX redirection responses
client.max_redirections(30);
```

#### Connection Driving

The connection is driven when a request is awaited, so users must keep sending requests to prevent the connection from becoming idle and to receive and act on HTTP 2 incoming SETTINGS, PING, and data as they arrive.

Alternatively, users can await the `drive` method to keep the connection driven and send ACK for incoming SETTINGS and PING, and send PING frames to detect if the connection is broken.

```cpp
// spawn the client driver to react on
// incoming SETTINGS and PING even if no requests
// are currently in flight
spawn(loop, client.drive());
// send requests or do other non HTTP 2 stuff...
```

Note that even if `drive` is awaited, the server may decide to close the connection anyway if the client doesn't send a request within a specified time window, or if it wants to close the connection for any reason.

The client tries to keep the connection to the server open as long as possible to save the cost of TCP connection establishment, SSL handshake, and HTTP 2 handshake.