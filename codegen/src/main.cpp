import rstd;
import liproto.codegen;
using namespace rstd::prelude;
using rstd::literals::operator""_str;
using ::alloc::vec::Vec;

auto fail(ref<str> message) -> int {
    auto stream = rstd::io::stderr();
    (void)rstd::io::write_all(stream, message.as_bytes());
    return 1;
}
auto main() -> int {
    Vec<u8>    input;
    auto       stream = rstd::io::stdin();
    rstd::byte buffer[4096];
    for (;;) {
        auto result = rstd::as<rstd::io::Read>(stream).read(
            mut_ref<u8[]>::from_raw_parts(buffer, usize(4096)));
        if (result.is_err()) return fail("liproto: stdin read failed\n"_str);
        if (*result == usize()) break;
        if (*result > usize(64 * 1024 * 1024) - input.len())
            return fail("liproto: request exceeds byte limit\n"_str);
        input.extend_from_slice(slice<u8>::from_raw_parts(buffer, *result));
    }
    auto request = liproto::codegen::decode_request(input.as_slice());
    if (request.is_err()) return fail("liproto: invalid CodeGeneratorRequest\n"_str);
    auto                                 schema = liproto::codegen::build_schema(*request);
    Vec<liproto::codegen::GeneratedFile> files;
    if (schema.is_ok()) files = liproto::codegen::generate(*schema);
    auto response = liproto::codegen::encode_response(
        files.as_slice(), schema.is_err() ? schema.unwrap_err().as_str() : ""_str);
    if (response.is_err()) return fail("liproto: response encoding failed\n"_str);
    auto output = rstd::io::stdout();
    if (rstd::io::write_all(output, response->as_slice()).is_err())
        return fail("liproto: stdout write failed\n"_str);
    return 0;
}
