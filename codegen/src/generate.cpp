import rstd;
import rstd.argparse;
import liproto.codegen;
using namespace rstd::prelude;
using rstd::literals::operator""_str;
using ::alloc::string::String;
using rstd::ffi::OsString;

auto fail(ref<str> message) -> int {
    rstd::io::eprintln("liproto-gen: {}", message);
    return 1;
}
auto main() -> int {
    using namespace rstd::argparse;
    auto command = Command::make("liproto-gen"_str);
    command.about("Generate one C++ module from a FileDescriptorSet"_str);
    auto input_key = command.add_arg(Arg<OsString>::value("descriptor-set"_str, os_string_parser())
                                         .long_name("descriptor-set"_str)
                                         .required());
    auto root_key  = command.add_arg(
        Arg<String>::value("root"_str, string_parser()).long_name("root"_str).required());
    auto output_key = command.add_arg(
        Arg<OsString>::value("output"_str, os_string_parser()).long_name("output"_str).required());
    auto parser = rstd::move(command).build();
    if (parser.is_err()) return fail("invalid CLI definition"_str);
    auto parsed = parser->parse_env();
    if (parsed.is_err())
        return fail("expected --descriptor-set FILE --root NAME.proto --output FILE.cppm"_str);
    if (parsed->is_Display()) {
        rstd::io::println("liproto-gen --descriptor-set FILE --root NAME.proto "
                          "--output FILE.cppm");
        return 0;
    }
    auto& matches = parsed->as_Parsed().value;
    auto  input   = matches.get_one(input_key);
    auto  root    = matches.get_one(root_key);
    auto  output  = matches.get_one(output_key);
    if (input.is_err() || root.is_err() || output.is_err() || input->is_none() || root->is_none() ||
        output->is_none())
        return fail("missing arguments"_str);
    auto input_path  = rstd::ref<rstd::path::Path>((***input).as_os_str());
    auto output_path = rstd::ref<rstd::path::Path>((***output).as_os_str());
    auto bytes       = rstd::fs::read(input_path);
    if (bytes.is_err()) return fail("cannot read descriptor set"_str);
    auto request = liproto::codegen::decode_descriptor_set(bytes->as_slice(), (***root).as_str());
    if (request.is_err()) return fail("invalid descriptor set"_str);
    auto schema = liproto::codegen::build_schema(*request);
    if (schema.is_err()) return fail(schema.unwrap_err().as_str());
    auto files = liproto::codegen::generate(*schema);
    if (files.len() != usize(1)) return fail("expected exactly one generated file"_str);
    auto written =
        rstd::fs::write_atomic_if_changed(output_path, files[usize()].content.as_str().as_bytes());
    if (written.is_err()) return fail("cannot write generated module"_str);
    return 0;
}
