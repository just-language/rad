
# RAD (rapid application development)

The RAD `C++ 20` asynchronous IO and networking library provides many facilities to ease development of async IO and networking applications leveraging modern c++ async features like coroutines and lambdas.

There are also JSON parsers, URL parser, HTTP 1.1 parsers, HTTP 2 parsers and DNS parser to aid applications implement the most famous internet protocols.

For databases, the library provides modern `C++ 20` wrappers for SQLite and ODBC.

Also, many coroutine types and wrappers are implemented to make using `C++ 20` coroutines very easy and intuitive.

Since this library design is greatly inspired by the famous `asio`, it will be very easy to use with `asio` background.

The library namespace is `rad` in which all types reside.
## Executors

To enable applications to submit async operations the library provides executors concepts, interfaces and implementations.

The `Executor`, `TimerExecutor`, `ProxyExecutor` and `ProxyTimerExecutor` concepts are defined in `<rad/async/executor.h>` with the interfaces `any_executor` and `timer_executor`.

The `IoExecutor`, `IoTimerExecutor`, `ProxyIoExecutor` and `ProxyIoTimerExecutor` concepts are defined in `<rad/async/io_executor.h>` with interface `io_executor`.

The library implements three executors: `io_loop`, `thread_pool` and `strand`.

The `io_loop` is defined in `<rad/async/io_loop.h>` and implements all the executors interfaces (async operations, timer and IO). It is backed by `IOCP` on Windows, `kqueue` on BSD systems and `epoll` and `io_uring` on Linux. The `io_uring` backend will be used only if enabled at compile time (default), enabled at runtime (default) and a recent kernel version which supports `io_uring` is found at runtime.

The `thread_pool` executor is defined in `<rad/threading/thread_pool.h>` and implements `any_executor` and `timer_executor`.

The `strand<T>` executor is a proxy executor defined in `<rad/async/strand.h>` to provide serialization execution of handlers and async operations.
It has the same functionality as `asio::strand`.

There is another executor which is the UI executor `Application` implemented in rad-ui library.

Since all async objects (sockets, files, pipes, timers, ...) hold references to their associated executor, the executor must be valid as long as any associated async object is used, or any async operation is in flight.

The same apply to async objects, they must be valid as long as any pending async operation on the object is in flight.

Executors and async objects can't be moved while an operation hold a reference to them.

The only exception is `timer` which on destruction or move will cancel any pending async wait operation on it, if it is still cancelable or detach it if it can no longer be canceled.
## Coroutines

Many coroutines types are provided in `<rad/coro/*>` to make it easy to use `C++ 20` coroutines.

The `task<T>` is the main coroutines return type and supports custom coroutine frame allocation. It is defined in `<rad/coro/task.h>`.

Coroutines which return `task<T>` are lazy coroutines because they will not start until the returned `task<T>` is awaited.

To run multiple coroutines in parallel and wait for them all use `when_all` functions and `operator &&` overloadings. They are defined in `<rad/coro/when_all.h>`.

To spawn an awaitable (like `task<T>`) to be started on an executor use `spawn` functions. To be able to async wait for the spawned awaitables use `spawner<Eexutor>` class to spawn coroutines and await the spawner. Spawn functions are defined in `<rad/coro/spawn.h>`

To async wait on a `spawner<Eexutor>` or `strand<Eexutor>` in face of exceptions use the `wait_on` functions in `<rad/coro/spawn.h>` or `scoped_wait` functions in `<rad/coro/scoped_wait.h>`.

To execute functions and coroutines on another executors or switch coroutines to another executors use `execute` functions in `<rad/coro/execute.h>`.

To run an awaitable with timeout use `execute_timeout` defined in `<rad/coro/execute_timeout.h>`.

There are also `async_event` and `async_phaser` types which provide async event notifications using handlers or coroutines. They are defined in `<rad/async/async_event.h>` and `<rad/async/async_phaser.h>`
## Async operations on objects

The async objects provide `async_*` functions (`async_write`, `async_read`, `async_wait`, ...) to perform async operations using handlers and awaitables.

If the async functions arguments are `Args ... args` then the last arguemnts deduces the kind of the async mode used.

```
// use awaitables and report errors via exceptions
auto result = co_await obj.async_operation(args...);
// use awaitables and report errors via exceptions
// (the same as the above)
auto result = co_await obj.async_operation(args..., rad::no_ec);
std::error_code ec;
// use awaitables and report errors via ec
auto result = co_await obj.async_operation(args..., ec);
// use handlers, the handler signature varies by the operation
obj.async_operation(args..., [](auto&& ... results) {});
// use handlers and custom allocator to allocate memory
// for the operation. the allocated memory will be deallocated
// just before handler invocation so the allocator may be
// reused inside the handler.
obj.async_operation(args..., [](auto&& ... results) {},
std::allocator<uint8_t>{});
```
## Timers

The library provides an async timer `timer` defined in `<rad/async/timer.h>` which provides methods to issue async wait operations using handlers and awaitable types.
## Networking

The IP addresses types, parsers and serializers are defined in `<rad/net/types.h>`.

The TCP and UDP networking functionality is provided respectively in `<rad/net/tcp.h>` and `<rad/net/udp.h>` which provide async sockets, resolvers and acceptors (for TCP).

The types `net::tcp` and `net::udp` encapsulates types and flags for TCP and UDP sockets.

The UNIX domain endpoint `local::endpoint` is defined in `<rad/local/endpoint.h>` and the UNIX TCP and UDP functionality is provided respectively in `<rad/local/tcp.h>` and `<rad/local/udp.h>` through `local::tcp` and `local::udp`.

To use sockets with other protocols implement your protocol and use it with `net::async_socket_base` defined in `<rad/net/async_socket.h>`.

To rebind a socket constructed with `io_loop` executor to `strand<io_loop>` executor use `rebind_executor` free function. The inner `io_loop` executor of the `strand` must be the same as the `io_loop` executor the socket was constructed with, otherwise the behavior is undefined.

The async DNS resolver is implemented using the operating system address resolution API like `getaddrinfo` which is not async so an internal thread pool is used to emulate the async resolution.

The only operating system that provides async DNS resolution is Windows 8 and later versions.

To use a truly async DNS resolver without going through a thread pool use the async UDP and TCP DNS resolver `net::ares::resolver` defined in `<rad/net/dns/ares_resolver.h>` or the async DNS Over HTTPS (DOH) resolver `net::doh::resolver` defined in `<rad/net/dns/doh_resolver.h>`.

The HTTP2 client can also be used to make many parallel DNS requests which is much faster than DOH resolver. This is demonstrated in the http2 client tests.
## SSL

The library provides SSL support using various SSL backends.
The currently implemented backends are OpenSSL, WolfSSL and MbedTLS.

The SSL backend interfaces are defined in `<rad/net/ssl/sslctx.h>` and the implementations are defined in `<rad/net/ssl/openssl_ctx.h>` for `net::ssl::openssl::context`, `<rad/net/ssl/wolfssl_ctx.h>` for `net::ssl::wolfssl::context` and `<rad/net/ssl/mbedtls_ctx.h>` for `net::ssl::mbedtls::context`.

For the MbedTLS backend, many functionalities are added like loading system certificates, using key password and verify callbacks and null terminating PEM keys and certificates buffers if they are not.

If another backend is needed, the user can implement the SSL interfaces for his backend.

The SSL `net::ssl::stream<T>` defined in `<rad/net/ssl/stream.h>` provides secure streaming of data using an SSL implementation and an underlying stream oriented transport layer like `net::tcp::socket`.

The async SSL implementation of this library should be more memory efficient than `asio` implementation since the library implements the SSL BIOs and IO callbacks to save buffers used by BIO pairs, so instead of adding two more buffers compared to sync SSL the library uses only one more buffer.

This one buffer can be further eliminated if the SSL library guarantees that the internal SSL buffers will be valid until they are transferred by the transport layer.
## HTTP

The library provides HTTP 1.1 message parsing and serialization in `<rad/net/http/http_parser.h>` and an HTTP 1.1 async client `net::http::http_client` using coroutines with SSL support in `<rad/net/http/http_client.h>`.

For HTTP 2, there is an HPACK implementation in `<rad/net/http2/hpack.h>`, HTTP 2 frames parser and serializer in `<rad/net/http2/http2_parser.h>` and HTTP 2 async client `net::http2::client` in `<rad/net/http2/http2_client.h>`.
## JSON
The library has JSON SAX and DOM parsers and JSON serializers since JSON is widely used format to exchange human readable data.

To use the JSON functionality include `<rad/json/json.h>`. The SAX parser is `json::parser<Handler>` and the DOM parsers are `json::parser` to parse a complete one JSON buffer, and `json::stream_parser` to parse a complete JSON in multiple buffers.

There is also the JSON incremental serializer `json::serializer`.

The design resembles that of `boost::json` but the library implementation is more lightweight and less in performance.
## URL

The library provides a URL parser and serializer according to WHATWG specifications in `net::url` defined in `<rad/net/url/url.h>` and various percent encoding functions in `<rad/net/url/percent_encoding.h>`

The class `net::url` handles percent encoding and decoding and supports UTF-8 inputs.
## Unicode and strings

Unicode encoding and decoding is implemented in `<rad/utf.h>` which provides support for UTF-8 `utf8_codecvt`, UTF-16 LE and BE `utf16_codecvt`, `utf16_le_codecvt` and `utf16_be_codecvt` and UTF-32 LE and BE `utf32_codecvt`, `utf32_le_codecvt` and `utf32_be_codecvt`.

Support to convert between different unicode encoding is supported with `utf_converter<From, To>` and many free functions that uses `utf_converter` like `string_to_u16string` and `u16string_to_string`.
## Crypto

In order to build a more async efficient SSL engine I tried to implement the necessary cryptography by TLS protocols but I realized it is a too heavy job for an individual to do and it requires a lot of knowledge in the cryptography field.

So I stopped with what I did and kept the implemented crypto because it may be useful for applications requiring AES or GCM without the need to use larger libraries like CryptoPP and OpenSSL.

The ciphers will not be as fast as fine grained libraries like OpenSSL and may be vulnerable to side channel attacks if the fast crypto feature is not enabled and hardware accelaration is disabled.

The AES ciphers `crypto::aes128`, `crypto::aes192` and `crypto::aes256` are defined in `<rad/crypto/aes.h>`.

The supported modes are `crypto::gcm_mode` in `<rad/crypto/modes/gcm.h>`, `crypto::ctr_mode` in `<rad/crypto/modes/ctr.h>`, `crypto::cbc_mode` in `<rad/crypto/modes/cbc.h>` and `crypto::ecb_mode` in `<rad/crypto/modes/ecb.h>`.
## SQLite

A modern `C++ 20` sqlite wrappers `sqlite::database` and `sqlite::query` are provided in `<rad/databases/sqlite.h>` with many features like range for loop select, RAII transactions and safe string query parameters binding to prevent SQL injection attacks.
## ODBC

Similar wrappers for ODBC are provided in `<rad/databases/odbc.h>` for Windows. Support for Linux is available using unixODBC (disabled by default).
## Channels

Rust like async channels `sync::oneshot::channel` and `sync::mpsc::channel` are provided in `<rad/channels/oneshot_channel.h>` and `<rad/channels/mpsc_channel.h>` respectively.
## Files, pipes and serial ports

Windows async named pipes `pipe::async_pipe` is defined in `<rad/ipc/async_pipe.h>` for Windows only.

Anonymous async pipes (UNIX pipes) `io::unnamed_pipe` is defined in `<rad/io/unnamed_pipe.h>` for UNIX and Windows. It is implemented on UNIX using `pipe` syscall, and on Windows using named pipes with random names.

Async serial port `io::serial_port` is defined in `<rad/io/serial_port.h>` for Windows and UNIX systems.

A blocking file implementation `io::files::file` is defined in `<rad/io/files.h>` to make it easy and portable to do file operations and read and write binary data from files.
## Windows extensions

Many Windows extensions are provided in `<rad/windows/*>`.

Registry keys `winreg::key` and `winreg::key_view` (for predefined keys) wrappers are defined in `<rad/windows/winreg.h>`. Predefined keys are supported like `winreg::classes_root` and `winreg::local_machine`.

Windows clock `windows_clock` in `<rad/windows/windows_clock.h>` is STL compatible clock which ticks since (1/1/1601) and its unit is 100 nanosecond. It is suitable to use with Windows `FILETIME`.

Windows service controller `svc::service_controller` enables management of Windows services. The service interface `svc::service_worker` and `svc::service` enable applications to implement Windows services very easily. They are all defined in `<rad/windows/service.h>`.
## How to build

The library uses CMake. The following options are defined:

WITH_SQLITE (ON or OFF) will enable the sqlite backend and link against the sqlite library. The default is ON.

WITH_OPENSSL (ON or OFF) will enable the openssl backend and link against the openssl library. The default is ON.

WITH_WOLFSSL (ON or OFF) will enable the wolfssl backend and link against the wolfssl library. The default is ON.

WITH_MBEDTLS (ON or OFF) will enable the mbedtls backend and link against the mbedtls library. The default is ON.

WITH_FAST_CRYPTO (ON or OFF) will enable the use of AES-NI and GCM cpu instructions if available. The default is ON.

WITH_LINUX_URING (ON or OFF) will enable the use of io_uring on Linux if available and link against liburing. The default is ON.

WITH_UNIX_ODBC (ON or OFF) will enable the use of unixodbc on Linux and link against unixodbc. The default is OFF.

BUILD_TESTS (ON or OFF) will build tests. The default is ON.

STATIC_RUNTIME (ON or OFF) will link against static c++ runtime. The default is OFF.

To easily install the dependencies use vcpkg with the manifest file vcpkg.json provided by the library.

For example:

```
cmake /path/to/rad/ -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-linux
```
## License

Distributed under the MIT License.