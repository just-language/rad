cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
project (rad VERSION 0.0.3 LANGUAGES C CXX)
set(LIBRARY_NAME "rad")

option(WITH_ASYNC_DEBUG "Enable async debugging and logging" OFF)
option(WITH_SINGLE_THD_EXECUTORS "Disable multithreading support in the library executors" OFF)
option(WITH_TRACKABLE "Enable trackable support for debugging in release builds" OFF)
option(WITH_SQLITE		"Build sqlite databases library"	ON)
option(WITH_OPENSSL		"Enable SSL support with OpenSSL"	ON)
option(WITH_WOLFSSL		"Enable SSL support with WolfSSL"	ON)
option(WITH_MBEDTLS		"Enable SSL support with MbedTLS"	ON)
option(WITH_FAST_CRYPTO "Enable crypto intrinsics to use cpu instructions if available" ON)
option(WITH_UNIX_ODBC "Enable unixodbc support on unix systems. The unixODBC driver must be installed" OFF)
option(WITH_LINUX_URING "Enable linux io_uring support. The liburing library must be installed" ON)

option(BUILD_TESTS "Build tests" ON)
option(STATIC_RUNTIME "Link to static c/c++ runtime library" OFF)

if(UNIX)
	if (BSD)
		set(BSD_OS TRUE)
		set(WITH_LINUX_URING OFF)
	elseif (ANDROID)
		set(ANDROID_OS TRUE)
		set(LINUX_OS TRUE)
		set(WITH_LINUX_URING OFF)
	else()
		set(LINUX_OS TRUE)
	endif()
else()
	if (WIN32)
		set(WINDOWS_OS TRUE)
		set(WITH_UNIX_ODBC OFF)
		set(WITH_LINUX_URING OFF)
	else()
		message(FATAL_ERROR "Unsupported platform. supported are: windows, linux, bsd and android" )
	endif()
endif()

set(HTTP_SOURCES
	src/net/http/http_parser.cpp
	src/net/http/http_client.cpp
	src/net/http/http_error.cpp
	src/net/http/http_headers_fields.cpp
	src/net/http/message_serialization.cpp
	src/net/http/http_operations.cpp
)

set(HTTP2_SOURCES
	src/net/http2/hpack.cpp
	src/net/http2/http2_client.cpp
	src/net/http2/http2_parser.cpp
)

set(DNS_SOURCES
	src/net/dns/dns_parser.cpp
	src/net/dns/ares_resolver.cpp
	src/net/dns/doh_resolver.cpp
)

set(CRYPTO_SOURCES
	src/crypto/base64.cpp
	src/crypto/aes.cpp
	src/crypto/gcm.cpp
	src/crypto/error.cpp
)

set(JSON_SOURCES
	src/json/json_value.cpp
	src/json/json_error.cpp
	src/json/json_parser.cpp
	src/json/json_serializer.cpp
)

set(WINDOWS_SOURCES
	src/windows/os_info.cpp
	src/windows/sysinfo.cpp
	src/windows/env.cpp
	
	src/threading/windows/condition_variable.cpp
	src/threading/windows/thread.cpp
	
	src/net/windows/async_resolver.cpp
	src/net/windows/async_socket.cpp
	src/io/windows/iocp.cpp
	src/io/windows/iocp_io_loop.cpp
	
	src/io/windows/files.cpp
	src/io/windows/async_file_impl.cpp
	src/io/windows/console.cpp
	src/io/windows/serial_port.cpp
	src/io/windows/unnamed_pipe.cpp
	
	src/ipc/windows/async_pipe.cpp
	
	src/windows/handle.cpp
	src/windows/process.cpp
	src/windows/system_error.cpp
	src/windows/user_env.cpp
	src/windows/winreg.cpp
	src/windows/service.cpp
	src/windows/wts.cpp
	
	src/crypto/wincrypt.cpp
	src/crypto/windpapi.cpp
)

set(POSIX_SOURCES
	src/posix/sysinfo.cpp
	src/threading/posix/thread.cpp
	src/net/posix/async_resolver.cpp
	src/net/posix/async_socket.cpp
	src/net/posix/resolver.cpp
	src/net/posix/socket.cpp
	src/io/posix/async_file_impl.cpp
	src/io/posix/files.cpp
	src/io/posix/iovecs_buffers.cpp
	src/io/posix/unnamed_pipe.cpp
	src/io/posix/serial_port.cpp
	src/io/posix/reactor_io_loop.cpp
)

set(LINUX_SOURCES
	src/io/linux/epoll.cpp
)

set(BSD_SOURCES
	src/io/bsd/kqueue.cpp
)

if (WITH_LINUX_URING)
	set(LINUX_SOURCES 
		${LINUX_SOURCES}
		src/io/linux/io_uring_loop.cpp
	)
endif()

set(OPENSSL_SOURCES
	src/net/ssl/openssl_impl.cpp
)

set(WOLFSSL_SOURCES
	src/net/ssl/wolfssl_impl.cpp
)

set(MBEDTLS_SOURCES
	src/net/ssl/mbedtls_impl.cpp
)

set(SQLITE_SOURCES
	src/databases/sqlite.cpp
)

set(SOURCES
	${SOURCES}
	src/utf.cpp
	src/cli.cpp
	src/no_ec_ref.cpp
	src/ring_buffer_consumer_producer.cpp
	src/strand.cpp
	src/channels/channels_error.cpp
	src/threading/condition_variable.cpp
	src/threading/thread_pool.cpp
	src/net/types.cpp	
	src/io/io_loop.cpp
	
	# crypto
	${CRYPTO_SOURCES}
	# http
	${HTTP_SOURCES}
	#http 2
	${HTTP2_SOURCES}
	# dns
	${DNS_SOURCES}
	# idna library
	src/net/idna/punycode.cpp
	src/net/idna/idna.cpp
	# url library
	src/net/url/percent_encoding.cpp
	src/net/url/url.cpp
	src/net/url/url_ip_parsing.cpp
	# json
	${JSON_SOURCES}
)
	
if (WINDOWS_OS)
	set(SOURCES ${SOURCES} ${WINDOWS_SOURCES} "src/databases/odbc.cpp")
elseif (WITH_UNIX_ODBC)
	set(SOURCES ${SOURCES} "src/databases/odbc.cpp")
	find_package(unixodbc REQUIRED)
endif()

if (UNIX)
	set(SOURCES ${SOURCES} ${POSIX_SOURCES})
endif()

if (LINUX_OS)
	set(SOURCES ${SOURCES} ${LINUX_SOURCES})
endif()

if (BSD_OS)
	set(SOURCES ${SOURCES} ${BSD_SOURCES})
endif()

include(cmake/CompilerFlags.cmake)
set(COMPILE_DEFS ${COMPILE_DEFS} RAD_LIB_IS_BEING_BUILT)

add_library(${LIBRARY_NAME} ${SOURCES})
target_compile_definitions(${LIBRARY_NAME} PRIVATE ${COMPILE_DEFS})
if (WIN32)
	target_compile_definitions(${LIBRARY_NAME} PUBLIC NOMINMAX)
endif()

if (BUILD_SHARED_LIBS)
	target_compile_definitions(${LIBRARY_NAME} PUBLIC RAD_IS_BUILT_AS_SHARED_LIB)
endif()

if (WITH_ASYNC_DEBUG)
	target_compile_definitions(${LIBRARY_NAME} PUBLIC RAD_ENABLE_ASYNC_DEBUG)
endif()

if (WITH_TRACKABLE)
	target_compile_definitions(${LIBRARY_NAME} PUBLIC RAD_ENABLE_TRACKABLE)
endif()

if (WITH_SINGLE_THD_EXECUTORS)
	target_compile_definitions(${LIBRARY_NAME} PUBLIC RAD_DISABLE_EXECUTOR_THREADS)
endif()

if (WITH_FAST_CRYPTO)
	target_compile_definitions(${LIBRARY_NAME} PUBLIC RAD_FAST_CRYPTO)
endif()

if (WITH_UNIX_ODBC)
	target_link_libraries(${LIBRARY_NAME} PUBLIC UNIX::odbc)
endif()

if (WITH_LINUX_URING)
	find_package(PkgConfig)
	pkg_check_modules(liburing REQUIRED IMPORTED_TARGET GLOBAL liburing>=2.0)
	target_link_libraries(${LIBRARY_NAME} PUBLIC PkgConfig::liburing)
	target_compile_definitions(${LIBRARY_NAME} PUBLIC RAD_HAS_IO_URING)
endif()

if(BUILD_SHARED_LIBS)
	if (WIN32)
		target_link_libraries(${LIBRARY_NAME} PUBLIC ws2_32 crypt32 bcrypt odbc32 wtsapi32 netapi32 userenv)
	else()
		target_link_libraries(${LIBRARY_NAME} PUBLIC pthread dl)
	endif()
endif()

if (WINDOWS_OS AND STATIC_RUNTIME)
	set_property(TARGET ${LIBRARY_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(GNUInstallDirs)
target_include_directories(${LIBRARY_NAME} PUBLIC
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
	"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

if (WITH_SQLITE)
	add_library(sqlite ${SQLITE_SOURCES})
	set_target_properties(sqlite PROPERTIES OUTPUT_NAME "rad-sqlite")
	find_package(unofficial-sqlite3 CONFIG REQUIRED)
	target_link_libraries(sqlite PUBLIC ${LIBRARY_NAME} PUBLIC unofficial::sqlite3::sqlite3)
	target_compile_definitions(sqlite PRIVATE ${COMPILE_DEFS})
	target_compile_definitions(sqlite PUBLIC RAD_HAVE_SQLITE)
	if (WINDOWS_OS AND STATIC_RUNTIME)
		set_property(TARGET sqlite PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
	target_include_directories(sqlite PUBLIC
		"$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
		"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
	)
	install(TARGETS sqlite
		EXPORT ${LIBRARY_NAME}-targets
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
		RUNTIME DESTINATION bin
	)
endif()
if (WITH_OPENSSL)
	find_package(OpenSSL REQUIRED)
	add_library(openssl ${OPENSSL_SOURCES})
	set_target_properties(openssl PROPERTIES OUTPUT_NAME "rad-openssl")
	target_link_libraries(openssl PUBLIC ${LIBRARY_NAME} PUBLIC OpenSSL::SSL PUBLIC OpenSSL::Crypto)
	target_compile_definitions(openssl PRIVATE ${COMPILE_DEFS})
	target_compile_definitions(openssl PUBLIC RAD_HAVE_OPENSSL RAD_HAVE_SSL)
	if (WINDOWS_OS AND STATIC_RUNTIME)
		set_property(TARGET openssl PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
	target_include_directories(openssl PUBLIC
		"$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
		"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
	)
	install(TARGETS openssl
		EXPORT ${LIBRARY_NAME}-targets
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
		RUNTIME DESTINATION bin
	)
endif()
if (WITH_WOLFSSL)
	find_package(wolfssl CONFIG REQUIRED)
	add_library(wolfssl ${WOLFSSL_SOURCES})
	set_target_properties(wolfssl PROPERTIES OUTPUT_NAME "rad-wolfssl")
	target_link_libraries(wolfssl PUBLIC ${LIBRARY_NAME} PUBLIC wolfssl::wolfssl)
	target_compile_definitions(wolfssl PRIVATE ${COMPILE_DEFS})
	target_compile_definitions(wolfssl PUBLIC RAD_HAVE_WOLFSSL RAD_HAVE_SSL)
	if (WINDOWS_OS AND STATIC_RUNTIME)
		set_property(TARGET wolfssl PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
	target_include_directories(wolfssl PUBLIC
		"$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
		"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
	)
	install(TARGETS wolfssl
		EXPORT ${LIBRARY_NAME}-targets
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
		RUNTIME DESTINATION bin
	)
endif()
if (WITH_MBEDTLS)
	find_package(MbedTLS CONFIG REQUIRED)
	add_library(mbedtls ${MBEDTLS_SOURCES})
	set_target_properties(mbedtls PROPERTIES OUTPUT_NAME "rad-mbedtls")
	target_link_libraries(mbedtls PUBLIC ${LIBRARY_NAME} PUBLIC MbedTLS::mbedtls)
	if (WINDOWS_OS)
		target_link_libraries(mbedtls PUBLIC crypt32.lib)
	endif()
	target_compile_definitions(mbedtls PRIVATE ${COMPILE_DEFS})
	target_compile_definitions(mbedtls PUBLIC RAD_HAVE_MBEDTLS RAD_HAVE_SSL)
	if (WINDOWS_OS AND STATIC_RUNTIME)
		set_property(TARGET mbedtls PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
	target_include_directories(mbedtls PUBLIC
		"$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
		"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
	)
	install(TARGETS mbedtls
		EXPORT ${LIBRARY_NAME}-targets
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
		RUNTIME DESTINATION bin
	)
endif()

# Install the library
install(TARGETS rad
	EXPORT rad-targets
	LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install the headers
install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/rad"
	CONFIGURATIONS Release
	DESTINATION "include"
	FILES_MATCHING PATTERN "*.h"
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
	"${CMAKE_CURRENT_LIST_DIR}/cmake/rad-config.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/rad-config.cmake"
	INSTALL_DESTINATION "share/rad"
)

write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/rad-config-version.cmake"
	VERSION 1.0
	COMPATIBILITY SameMinorVersion
)

install(
  FILES
	"${CMAKE_CURRENT_BINARY_DIR}/rad-config.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/rad-config-version.cmake"
  DESTINATION "share/rad"
)

# Install the export set
install(
    EXPORT rad-targets
    DESTINATION share/rad
    NAMESPACE rad::
)


set(TESTS_SOURCES 
	tests/main.cpp
	tests/trackable_tests.cpp
	tests/channels_tests.cpp
	tests/cli_tests.cpp
	tests/coro_tests.cpp
	tests/crypto_tests.cpp
	tests/base64_tests.cpp
	tests/executors_compile_tests.cpp
	tests/hpack_tests.cpp
	tests/json_tests.cpp
	tests/matrix-tests.cpp
	tests/net-tests.cpp
	tests/ring_buffer_tests.cpp
	tests/serial_port_tests.cpp
	tests/socket-compile-tests1.cpp
	tests/socket-compile-tests2.cpp
	tests/socket-tests.cpp
	tests/stack_lists_tests.cpp
	tests/strand-tests.cpp
	tests/string_tests.cpp
	tests/url_tests.cpp
	tests/punycode_tests.cpp
	tests/percent_encoding_tests.cpp
	tests/utf-tests.cpp
	tests/views_tests.cpp
	tests/unnamed_pipes_tests.cpp
	tests/http_parser_tests.cpp
)

if (WIN32 OR WITH_UNIX_ODBC)
	set(TESTS_SOURCES ${TESTS_SOURCES} tests/odbc-tests.cpp)
endif()

set(TESTS_LINK_LIBS rad)

if (WIN32)
	set(TESTS_SOURCES 
		${TESTS_SOURCES} 
		tests/pipe-test.cpp
		tests/windows_service.cpp
	)
	set(TESTS_LINK_LIBS ${TESTS_LINK_LIBS} ws2_32 crypt32 odbc32)
elseif(WITH_UNIX_ODBC)
	set(TESTS_LINK_LIBS ${TESTS_LINK_LIBS} UNIX::odbc)
endif()

if (WITH_SQLITE)
	set(TESTS_SOURCES ${TESTS_SOURCES} tests/sqlite-tests.cpp)
	set(TESTS_LINK_LIBS ${TESTS_LINK_LIBS} sqlite)
endif()

if (WITH_OPENSSL OR WITH_WOLFSSL OR WITH_MBEDTLS)
	set(TESTS_SOURCES ${TESTS_SOURCES} tests/dns_resolvers_tests.cpp tests/http_client_tests.cpp tests/http2_client_tests.cpp)
	if (WITH_OPENSSL)
		set(TESTS_LINK_LIBS ${TESTS_LINK_LIBS} openssl OpenSSL::SSL OpenSSL::Crypto)
	endif()
	if (WITH_WOLFSSL)
		set(TESTS_SOURCES ${TESTS_SOURCES} tests/wolfssl_client_test.cpp)
		set(TESTS_LINK_LIBS ${TESTS_LINK_LIBS} wolfssl)
	endif()
	if (WITH_MBEDTLS)
		set(TESTS_LINK_LIBS ${TESTS_LINK_LIBS} mbedtls)
	endif()
endif()

if (BUILD_TESTS)
	add_executable(rad-tests ${TESTS_SOURCES})
	if (MSVC)
		set_source_files_properties(tests/json_tests.cpp tests/punycode_tests.cpp 
		tests/percent_encoding_tests.cpp 
		tests/percent_encoding_tests.cpp 
		tests/http_parser_tests.cpp
		tests/url_tests.cpp	PROPERTIES COMPILE_OPTIONS "/utf-8")
	endif()
	target_link_libraries(rad-tests ${TESTS_LINK_LIBS})
	if (WINDOWS_OS AND STATIC_RUNTIME)
		set_property(TARGET rad-tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
	add_custom_command(
		TARGET rad-tests
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
				"${CMAKE_CURRENT_SOURCE_DIR}/tests/urltestdata.json"
				"$<TARGET_FILE_DIR:rad-tests>/urltestdata.json"
		COMMENT "Copying urltestdata.json executable to tests folder"
	)
endif()