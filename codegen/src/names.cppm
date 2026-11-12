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

void append_escape(String& output, unsigned char value) {
    constexpr char hex[] = "0123456789abcdef";
    output.push_ascii('z');
    output.push_ascii(hex[value >> 4]);
    output.push_ascii(hex[value & 15]);
}
auto cpp_identifier(ref<str> value) -> String {
    auto   bytes = value.as_bytes();
    String output;
    for (usize index {}; index < bytes.len(); ++index) {
        auto c      = bytes[index].to_primitive();
        bool escape = false;
        if (c == '_') {
            // Keep components clear of reserved double underscores and the nested-type separator.
            escape = index == usize() || index + usize(1) == bytes.len() ||
                     bytes[index - usize(1)] == u8('_') || bytes[index + usize(1)] == u8('_') ||
                     (index + usize(2) < bytes.len() && bytes[index + usize(1)] == u8('n') &&
                      bytes[index + usize(2)] == u8('_'));
        } else if (c == 'z') {
            // Literal escape-looking text must remain distinct from an escaped byte.
            escape = index + usize(2) < bytes.len() &&
                     ((bytes[index + usize(1)] == u8('5') && bytes[index + usize(2)] == u8('f')) ||
                      (bytes[index + usize(1)] == u8('7') && bytes[index + usize(2)] == u8('a')));
        }
        if (escape)
            append_escape(output, c);
        else
            output.push_ascii(static_cast<char>(c));
    }
    return output;
}

auto variant_identifier(ref<str> value) -> String {
    auto               name       = cpp_identifier(value);
    constexpr ref<str> keywords[] = {
        "alignas"_str,       "alignof"_str,     "and"_str,
        "and_eq"_str,        "asm"_str,         "auto"_str,
        "bitand"_str,        "bitor"_str,       "bool"_str,
        "break"_str,         "case"_str,        "catch"_str,
        "char"_str,          "char8_t"_str,     "char16_t"_str,
        "char32_t"_str,      "class"_str,       "compl"_str,
        "concept"_str,       "const"_str,       "consteval"_str,
        "constexpr"_str,     "constinit"_str,   "const_cast"_str,
        "continue"_str,      "co_await"_str,    "co_return"_str,
        "co_yield"_str,      "decltype"_str,    "default"_str,
        "delete"_str,        "do"_str,          "double"_str,
        "dynamic_cast"_str,  "else"_str,        "enum"_str,
        "explicit"_str,      "export"_str,      "extern"_str,
        "false"_str,         "float"_str,       "for"_str,
        "friend"_str,        "goto"_str,        "if"_str,
        "inline"_str,        "int"_str,         "long"_str,
        "mutable"_str,       "namespace"_str,   "new"_str,
        "noexcept"_str,      "not"_str,         "not_eq"_str,
        "nullptr"_str,       "operator"_str,    "or"_str,
        "or_eq"_str,         "private"_str,     "protected"_str,
        "public"_str,        "register"_str,    "reinterpret_cast"_str,
        "requires"_str,      "return"_str,      "short"_str,
        "signed"_str,        "sizeof"_str,      "static"_str,
        "static_assert"_str, "static_cast"_str, "struct"_str,
        "switch"_str,        "template"_str,    "this"_str,
        "thread_local"_str,  "throw"_str,       "true"_str,
        "try"_str,           "typedef"_str,     "typeid"_str,
        "typename"_str,      "union"_str,       "unsigned"_str,
        "using"_str,         "virtual"_str,     "void"_str,
        "volatile"_str,      "wchar_t"_str,     "while"_str,
        "xor"_str,           "xor_eq"_str
    };
    for (auto keyword : keywords) {
        if (name != keyword) continue;
        // cpp_identifier escapes trailing underscores, so this suffix cannot collide.
        name.push_ascii('_');
        break;
    }
    return name;
}

auto portable_name(ref<str> value) -> String {
    String output;
    for (auto byte : value.as_bytes()) {
        auto c = byte.to_primitive();
        if ((c >= 'a' && c <= 'y') || (c >= '0' && c <= '9'))
            output.push_ascii(static_cast<char>(c));
        else
            append_escape(output, c);
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
        result.push_str(cpp_identifier(part).as_str());
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
        auto encoded = portable_name(component);
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
