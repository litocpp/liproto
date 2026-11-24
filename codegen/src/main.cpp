#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

import liproto.codegen;
using namespace rstd::prelude;
using rstd::literals::operator""_str;
using ::alloc::vec::Vec;

auto fail(const char* message) -> int {
    (void)fputs(message, stderr);
    return 1;
}
auto main() -> int {
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1)
        return fail("liproto: cannot set binary standard streams\n");
#endif
    Vec<u8>    input;
    rstd::byte buffer[4096];
    for (;;) {
        auto count = fread(buffer, 1, sizeof(buffer), stdin);
        if (ferror(stdin)) return fail("liproto: stdin read failed\n");
        if (count == 0) break;
        if (usize(count) > usize(64 * 1024 * 1024) - input.len())
            return fail("liproto: request exceeds byte limit\n");
        input.extend_from_slice(slice<u8>::from_raw_parts(buffer, usize(count)));
    }
    auto request = liproto::codegen::decode_request(input.as_slice());
    if (request.is_err()) return fail("liproto: invalid CodeGeneratorRequest\n");
    auto                                 schema = liproto::codegen::build_schema(*request);
    Vec<liproto::codegen::GeneratedFile> files;
    if (schema.is_ok()) files = liproto::codegen::generate(*schema);
    auto response = liproto::codegen::encode_response(
        files.as_slice(), schema.is_err() ? schema.unwrap_err().as_str() : ""_str);
    if (response.is_err()) return fail("liproto: response encoding failed\n");
    auto bytes = response->as_slice();
    if (fwrite(bytes.as_raw_ptr(), 1, bytes.len().to_primitive(), stdout) !=
            bytes.len().to_primitive() ||
        fflush(stdout) != 0)
        return fail("liproto: stdout write failed\n");
    return 0;
}
