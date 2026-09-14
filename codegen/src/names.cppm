module liproto.codegen:names;
import :descriptor;

using namespace rstd::prelude;
using rstd::literals::operator""_str;
using ::alloc::string::String;

namespace liproto::codegen
{
auto identifier(ref<str> value) -> bool {
    if (value.is_empty()) return false;
    for (usize i {}; i < value.len(); ++i) {
        auto c = value.as_bytes()[i].to_primitive();
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') continue;
        if (i != usize() && c >= '0' && c <= '9') continue;
        return false;
    }
    return true;
}

// Escaping z makes the encoding injective; underscores remain structural
// separators.
auto encoded_name(ref<str> value, bool portable = false) -> String {
    constexpr char hex[] = "0123456789abcdef";
    String         output;
    for (auto byte : value.as_bytes()) {
        auto c = byte.to_primitive();
        if ((c >= 'a' && c <= 'y') || (c >= '0' && c <= '9') ||
            (! portable && c >= 'A' && c <= 'Z'))
            output.push_ascii(static_cast<char>(c));
        else {
            output.push_ascii('z');
            output.push_ascii(hex[c >> 4]);
            output.push_ascii(hex[c & 15]);
        }
    }
    return output;
}
auto cpp_namespace(ref<str> package) -> Result<String, String> {
    if (package.is_empty()) return Ok(String::make("proto"_str));
    String result;
    usize  start {};
    for (usize i {}; i <= package.len(); ++i) {
        if (i != package.len() && package.as_bytes()[i] != u8('.')) continue;
        auto part = *package.get(start, i);
        if (! identifier(part)) return Err(String::make("invalid proto package"_str));
        if (start != usize()) result.push_str("::"_str);
        result.push_str("p_"_str);
        result.push_str(encoded_name(part).as_str());
        start = i + usize(1);
    }
    return Ok(rstd::move(result));
}
struct FileNames {
    String output;
    String module;
};
auto file_names(ref<str> path) -> Result<FileNames, String> {
    if (path.len() <= usize(6) || *path.get(path.len() - usize(6), path.len()) != ".proto"_str)
        return Err(String::make("expected relative .proto path"_str));
    auto      stem = *path.get(usize(), path.len() - usize(6));
    FileNames names { String {}, String::make("proto"_str) };
    usize     start {};
    for (usize i {}; i <= stem.len(); ++i) {
        if (i != stem.len() && stem.as_bytes()[i] != u8('/')) continue;
        auto component = *stem.get(start, i);
        if (component.is_empty() || component == "."_str || component == ".."_str ||
            component.contains("\\"_str) || component.contains(":"_str))
            return Err(String::make("invalid proto path component"_str));
        auto encoded = encoded_name(component, true);
        if (start != usize()) names.output.push_ascii('/');
        names.output.push_str(encoded.as_str());
        names.module.push_str(".p_"_str);
        names.module.push_str(encoded.as_str());
        start = i + usize(1);
    }
    names.output.push_str(".cppm"_str);
    return Ok(rstd::move(names));
}
} // namespace liproto::codegen
