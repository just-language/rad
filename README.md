# RAD (Rapid Application Development)

The RAD `C++20` asynchronous I/O and networking library provides comprehensive facilities to simplify the development of async I/O and networking applications by leveraging modern C++ async features like coroutines and lambdas.

The library includes JSON parsers, URL parser, HTTP 1.1 parsers, HTTP 2 parsers, and DNS parser to help applications implement the most common internet protocols.

For databases, the library provides modern `C++20` wrappers for SQLite and ODBC.

Additionally, many coroutine types and wrappers are implemented to make using `C++20` coroutines easy and intuitive.

Since this library's design is greatly inspired by the famous `asio`, it will be very familiar for those with an `asio` background.

The library namespace is `rad` where all types reside.

## Executors

To enable applications to submit async operations, the library provides executor concepts, interfaces, and implementations.

The `Executor`, `TimerExecutor`, `ProxyExecutor`, and `ProxyTimerExecutor` concepts are defined in `<rad/async/executor.h>` with the interfaces `any_executor` and `timer_executor`.

The `IoExecutor`, `IoTimerExecutor`, `ProxyIoExecutor`, and `ProxyIoTimerExecutor` concepts are defined in `<rad/async/io_executor.h>` with interface `io_executor`.

The library implements three executors: `io_loop`, `thread_pool`, and `strand`.

The `io_loop` is defined in `<rad/async/io_loop.h>` and implements all the executor interfaces (async operations, timer and I/O). It is backed by `IOCP` on Windows, `kqueue` on BSD systems, and `epoll` and `io_uring` on Linux. The `io_uring` backend will be used only if enabled at compile time (default), enabled at runtime (default), and a recent kernel version that supports `io_uring` is found at runtime.

The `thread_pool` executor is defined in `<rad/threading/thread_pool.h>` and implements `any_executor` and `timer_executor`.

The `strand<T>` executor is a proxy executor defined in `<rad/async/strand.h>` that provides serialized execution of handlers and async operations. It has the same functionality as `asio::strand`.

There is another executor called the UI executor `Application` implemented in the rad-ui library.

Since all async objects (sockets, files, pipes, timers, etc.) hold references to their associated executor, the executor must remain valid as long as any associated async object is used or any async operation is in progress.

The same applies to async objects - they must remain valid as long as any pending async operation on the object is in progress.

Executors and async objects cannot be moved while an operation holds a reference to them.

The only exception is `timer`, which upon destruction or move will cancel any pending async wait operation if it is still cancelable, or detach it if it can no longer be canceled.

## Coroutines

Many coroutine types are provided in `<rad/coro/*>` to make it easy to use `C++20` coroutines.

The `task<T>` is the main coroutine return type and supports custom coroutine frame allocation. It is defined in `<rad/coro/task.h>`.

Coroutines that return `task<T>` are lazy coroutines because they will not start until the returned `task<T>` is awaited.

To run multiple coroutines in parallel and wait for them all, use `when_all` functions and `operator &&` overloads. They are defined in `<rad/coro/when_all.h>`.

To spawn an awaitable (like `task<T>`) to be started on an executor, use `spawn` functions. To async wait for the spawned awaitables, use the `spawner<Executor>` class to spawn coroutines and await the spawner. Spawn functions are defined in `<rad/coro/spawn.h>`.

To async wait on a `spawner<Executor>` or `strand<Executor>` while handling exceptions, use the `wait_on` functions in `<rad/coro/spawn.h>` or `scoped_wait` functions in `<rad/coro/scoped_wait.h>`.

To execute functions and coroutines on other executors or switch coroutines to other executors, use `execute` functions in `<rad/coro/execute.h>`.

To run an awaitable with timeout, use `execute_timeout` defined in `<rad/coro/execute_timeout.h>`.

There are also `async_event` and `async_phaser` types that provide async event notifications using handlers or coroutines. They are defined in `<rad/async/async_event.h>` and `<rad/async/async_phaser.h>`.

## Async Operations on Objects

The async objects provide `async_*` functions (`async_write`, `async_read`, `async_wait`, etc.) to perform async operations using handlers and awaitables.

If the async function arguments are `Args ... args`, then the last argument determines the kind of async mode used.

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

The library provides an async timer `timer` defined in `<rad/async/timer.h>` that provides methods to issue async wait operations using handlers and awaitable types.

## Networking

The IP address types, parsers, and serializers are defined in `<rad/net/types.h>`.

The TCP and UDP networking functionality is provided in `<rad/net/tcp.h>` and `<rad/net/udp.h>` respectively, which provide async sockets, resolvers, and acceptors (for TCP).

The types `net::tcp` and `net::udp` encapsulate types and flags for TCP and UDP sockets.

The UNIX domain endpoint `local::endpoint` is defined in `<rad/local/endpoint.h>`, and the UNIX TCP and UDP functionality is provided in `<rad/local/tcp.h>` and `<rad/local/udp.h>` through `local::tcp` and `local::udp`.

To use sockets with other protocols, implement your protocol and use it with `net::async_socket_base` defined in `<rad/net/async_socket.h>`.

To rebind a socket constructed with an `io_loop` executor to a `strand<io_loop>` executor, use the `rebind_executor` free function. The inner `io_loop` executor of the `strand` must be the same as the `io_loop` executor the socket was constructed with; otherwise, the behavior is undefined.

The async DNS resolver is implemented using the operating system's address resolution API like `getaddrinfo`, which is not async, so an internal thread pool is used to emulate async resolution.

Windows 8 and later versions are the only operating systems that provide native async DNS resolution.

To use a truly async DNS resolver without going through a thread pool, use the async UDP and TCP DNS resolver `net::ares::resolver` defined in `<rad/net/dns/ares_resolver.h>` or the async DNS Over HTTPS (DOH) resolver `net::doh::resolver` defined in `<rad/net/dns/doh_resolver.h>`.

The HTTP2 client can also be used to make many parallel DNS requests, which is much faster than the DOH resolver. This is demonstrated in the HTTP2 client tests.

## SSL

The library provides SSL support using various SSL backends. The currently implemented backends are OpenSSL, WolfSSL, and MbedTLS.

The SSL backend interfaces are defined in `<rad/net/ssl/sslctx.h>`, and the implementations are defined in:
- `<rad/net/ssl/openssl_ctx.h>` for `net::ssl::openssl::context`
- `<rad/net/ssl/wolfssl_ctx.h>` for `net::ssl::wolfssl::context`
- `<rad/net/ssl/mbedtls_ctx.h>` for `net::ssl::mbedtls::context`

For the MbedTLS backend, many functionalities are added, such as loading system certificates, using key passwords, verify callbacks, and null-terminating PEM keys and certificate buffers if they aren't already.

If another backend is needed, users can implement the SSL interfaces for their preferred backend.

The SSL `net::ssl::stream<T>` defined in `<rad/net/ssl/stream.h>` provides secure streaming of data using an SSL implementation and an underlying stream-oriented transport layer like `net::tcp::socket`.

The async SSL implementation in this library is more memory efficient than `asio`'s implementation because the library implements SSL BIOs and I/O callbacks to save buffers used by BIO pairs. Instead of adding two more buffers compared to sync SSL, the library uses only one additional buffer.

This one buffer can be further eliminated if the SSL library guarantees that the internal SSL buffers remain valid until they are transferred by the transport layer.

## HTTP

The library provides HTTP 1.1 message parsing and serialization in `<rad/net/http/http_parser.h>` and an HTTP 1.1 async client `net::http::http_client` using coroutines with SSL support in `<rad/net/http/http_client.h>`.

For HTTP 2, there is an HPACK implementation in `<rad/net/http2/hpack.h>`, HTTP 2 frames parser and serializer in `<rad/net/http2/http2_parser.h>`, and HTTP 2 async client `net::http2::client` in `<rad/net/http2/http2_client.h>`.

## JSON

The library includes JSON SAX and DOM parsers and JSON serializers since JSON is a widely used format for exchanging human-readable data.

To use the JSON functionality, include `<rad/json/json.h>`. The SAX parser is `json::basic_parser<Handler>`, and the DOM parsers are `json::parser` for parsing a complete JSON buffer and `json::stream_parser` for parsing complete JSON from multiple buffers.

There is also the JSON incremental serializer `json::serializer`.

The design resembles that of `boost::json`, but this library's implementation is more lightweight though slightly less performant.

## URL

The library provides a URL parser and serializer according to WHATWG specifications `net::url` defined in `<rad/net/url/url.h>` and various percent encoding functions in `<rad/net/url/percent_encoding.h>`.

The class `net::url` handles percent encoding and decoding and supports UTF-8 inputs.

## Unicode and Strings

Unicode encoding and decoding is implemented in `<rad/utf.h>`, which provides support for:
- UTF-8 `utf8_codecvt`
- UTF-16 LE and BE `utf16_codecvt`, `utf16_le_codecvt`, and `utf16_be_codecvt`
- UTF-32 LE and BE `utf32_codecvt`, `utf32_le_codecvt`, and `utf32_be_codecvt`

Support for converting between different Unicode encodings is provided with `utf_converter<From, To>` and many free functions that use `utf_converter`, such as `string_to_u16string` and `u16string_to_string`.

## Crypto

While attempting to build a more async-efficient SSL engine, I implemented the necessary cryptography for TLS protocols. However, I realized this was too substantial a task for an individual and requires extensive cryptography knowledge.

I stopped development but kept the implemented crypto functionality because it may be useful for applications requiring AES or GCM without needing larger libraries like CryptoPP or OpenSSL.

The ciphers will not be as fast as fine-grained libraries like OpenSSL and may be vulnerable to side-channel attacks if the fast crypto feature is not enabled and hardware acceleration is disabled.

The AES ciphers `crypto::aes128`, `crypto::aes192`, and `crypto::aes256` are defined in `<rad/crypto/aes.h>`.

The supported modes are:
- `crypto::gcm_mode` in `<rad/crypto/modes/gcm.h>`
- `crypto::ctr_mode` in `<rad/crypto/modes/ctr.h>`
- `crypto::cbc_mode` in `<rad/crypto/modes/cbc.h>`
- `crypto::ecb_mode` in `<rad/crypto/modes/ecb.h>`

## SQLite

Modern `C++20` SQLite wrappers `sqlite::database` and `sqlite::query` are provided in `<rad/databases/sqlite.h>` with features like range-for-loop select, RAII transactions, and safe string query parameter binding to prevent SQL injection attacks.

## ODBC

Similar wrappers for ODBC are provided in `<rad/databases/odbc.h>` for Windows. Support for Linux is available using unixODBC (disabled by default).

## Channels

Rust-like async channels `sync::oneshot::channel` and `sync::mpsc::channel` are provided in `<rad/channels/oneshot_channel.h>` and `<rad/channels/mpsc_channel.h>` respectively.

## Files, Pipes and Serial Ports

Windows async named pipes `pipe::async_pipe` are defined in `<rad/ipc/async_pipe.h>` for Windows only.

Anonymous async pipes (UNIX pipes) `io::unnamed_pipe` are defined in `<rad/io/unnamed_pipe.h>` for UNIX and Windows. On UNIX, they are implemented using the `pipe` syscall, and on Windows using named pipes with random names.

Async serial port `io::serial_port` is defined in `<rad/io/serial_port.h>` for Windows and UNIX systems.

A blocking file implementation `io::files::file` is defined in `<rad/io/files.h>` to make file operations portable and easy, including reading and writing binary data.

## Windows Extensions

Many Windows extensions are provided in `<rad/windows/*>`.

Registry key wrappers `winreg::key` and `winreg::key_view` (for predefined keys) are defined in `<rad/windows/winreg.h>`. Predefined keys like `winreg::classes_root` and `winreg::local_machine` are supported.

Windows clock `windows_clock` in `<rad/windows/windows_clock.h>` is an STL-compatible clock that ticks since 1/1/1601 with 100-nanosecond units, making it suitable for use with Windows `FILETIME`.

Windows service controller `svc::service_controller` enables management of Windows services. The service interface `svc::service_worker` and `svc::service` make it easy for applications to implement Windows services. These are all defined in `<rad/windows/service.h>`.

## How to Build and Use

The library uses CMake. The following options are defined:

- `WITH_SQLITE` (ON or OFF) enables the SQLite backend and links against the SQLite library. Default is ON.
- `WITH_OPENSSL` (ON or OFF) enables the OpenSSL backend and links against the OpenSSL library. Default is ON.
- `WITH_WOLFSSL` (ON or OFF) enables the WolfSSL backend and links against the WolfSSL library. Default is ON.
- `WITH_MBEDTLS` (ON or OFF) enables the MbedTLS backend and links against the MbedTLS library. Default is ON.
- `WITH_FAST_CRYPTO` (ON or OFF) enables the use of AES-NI and GCM CPU instructions if available. Default is ON.
- `WITH_LINUX_URING` (ON or OFF) enables the use of io_uring on Linux if available and links against liburing. Default is ON.
- `WITH_UNIX_ODBC` (ON or OFF) enables the use of unixODBC on Linux and links against unixODBC. Default is OFF.
- `BUILD_TESTS` (ON or OFF) builds tests. Default is ON.
- `STATIC_RUNTIME` (ON or OFF) links against static C++ runtime. Default is OFF.

To easily install dependencies, use vcpkg with the manifest file vcpkg.json provided by the library.

For example:

```
cmake /path/to/rad/ -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-linux
```

To use the library with CMake:

```
find_package(rad CONFIG REQUIRED)
target_link_libraries(main PRIVATE rad::rad)
# For other library modules
target_link_libraries(main PRIVATE rad::sqlite rad::openssl rad::wolfssl rad::mbedtls)
```
## License

Distributed under the MIT License.