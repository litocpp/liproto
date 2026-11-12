export module liproto.codegen:emit;
export import :schema;
using namespace rstd::prelude;
using rstd::literals::operator""_str;
using ::alloc::string::String;
using ::alloc::vec::Vec;

namespace liproto::codegen
{
auto decimal(unsigned value) -> String {
    char     digits[10];
    unsigned length = 0;
    do {
        digits[length++] = static_cast<char>('0' + value % 10);
        value /= 10;
    } while (value);
    String result;
    while (length) result.push_ascii(digits[--length]);
    return result;
}
auto field_type(const ResolvedField& field) -> String {
    String type;
    if (field.kind == 11) {
        type.push_str("::alloc::boxed::Box<"_str);
        type.push_str(field.message_type.as_str());
        type.push_str(">"_str);
    } else {
        type.push_str("liproto::Scalar<"_str);
        type.push_str(decimal(field.kind).as_str());
        type.push_str(">::Type"_str);
    }
    return type;
}
auto map_arguments(const ResolvedField& field) -> String {
    auto arguments = decimal(field.map_key);
    arguments.push_str(", "_str);
    arguments.push_str(decimal(field.map_value).as_str());
    if (field.map_value == 11) {
        arguments.push_str(", "_str);
        arguments.push_str(field.message_type.as_str());
    }
    return arguments;
}
template<class... Parts>
void emit(String& text, Parts... parts) {
    (text.push_str(parts), ...);
}
void emit_oneof(String& text, const ResolvedMessage& message, const ResolvedField& first) {
    auto name = String::make("C_"_str);
    name.push_str(first.oneof.as_str());
    emit(text,
         "    class "_str,
         name.as_str(),
         " {\n    public:\n        enum class Tag : unsigned {\n"_str);
    for (auto& field : message.fields) {
        if (field.oneof != first.oneof) continue;
        emit(text,
             "            "_str,
             field.variant.as_str(),
             " = "_str,
             decimal(field.number).as_str(),
             "u,\n"_str);
    }
    emit(text, "        };\n    private:\n        using Storage = rstd::Choice<"_str);
    bool separator = false;
    for (auto& field : message.fields) {
        if (field.oneof != first.oneof) continue;
        if (separator) text.push_str(", "_str);
        separator = true;
        emit(text,
             "rstd::choice_case<Tag::"_str,
             field.variant.as_str(),
             ", "_str,
             field_type(field).as_str(),
             ">"_str);
    }
    emit(text,
         ">;\n        Storage storage_;\n        explicit "_str,
         name.as_str(),
         "(Storage storage) : storage_(rstd::move(storage)) {}\n    public:\n"
         "        constexpr auto tag() const noexcept -> Tag { return storage_.which(); }\n"_str);
    for (auto& field : message.fields) {
        if (field.oneof != first.oneof) continue;
        auto variant = field.variant.as_str();
        auto type    = field_type(field);
        emit(text,
             "        static auto make_"_str,
             variant,
             "("_str,
             type.as_str(),
             " value) -> "_str,
             name.as_str(),
             " { return "_str,
             name.as_str(),
             "(Storage::with<Tag::"_str,
             variant,
             ">(rstd::move(value))); }\n"
             "        constexpr auto is_"_str,
             variant,
             "() const noexcept -> bool { return storage_.is<Tag::"_str,
             variant,
             ">(); }\n"
             "        void replace_"_str,
             variant,
             "("_str,
             type.as_str(),
             " value) { storage_.set<Tag::"_str,
             variant,
             ">(rstd::move(value)); }\n"_str);
        constexpr ref<str> qualifiers[] = { "&"_str, "const&"_str, "&&"_str, "const&&"_str };
        for (auto qualifier : qualifiers) {
            emit(text,
                 "        constexpr auto as_"_str,
                 variant,
                 "() "_str,
                 qualifier,
                 " noexcept -> "_str,
                 type.as_str(),
                 " "_str,
                 qualifier,
                 " { return "_str);
            if (qualifier == "&&"_str)
                text.push_str("rstd::move(storage_)"_str);
            else if (qualifier == "const&&"_str)
                text.push_str("static_cast<Storage const&&>(storage_)"_str);
            else
                text.push_str("storage_"_str);
            emit(text, ".as<Tag::"_str, variant, ">(); }\n"_str);
        }
    }
    constexpr ref<str> qualifiers[] = { "&"_str, "const&"_str, "&&"_str, "const&&"_str };
    for (auto qualifier : qualifiers) {
        emit(
            text,
            "        template<class Visitor>\n        constexpr decltype(auto) visit(Visitor&& visitor) "_str,
            qualifier,
            " { return "_str);
        if (qualifier == "&&"_str)
            text.push_str("rstd::move(storage_)"_str);
        else if (qualifier == "const&&"_str)
            text.push_str("static_cast<Storage const&&>(storage_)"_str);
        else
            text.push_str("storage_"_str);
        text.push_str(".visit(rstd::forward<Visitor>(visitor)); }\n"_str);
    }
    emit(text,
         "    };\n    Option<"_str,
         name.as_str(),
         "> "_str,
         first.oneof.as_str(),
         " {};\n"_str);
}
export auto generate(const Schema& schema) -> Vec<GeneratedFile> {
    Vec<GeneratedFile> files;
    for (auto& file : schema.files()) {
        String text = String::make("export module "_str);
        text.push_str(file.module.as_str());
        text.push_str(";\nexport import liproto;\n"_str);
        for (auto& dependency : file.imports) {
            text.push_str("export import "_str);
            text.push_str(dependency.as_str());
            text.push_str(";\n"_str);
        }
        text.push_str("using namespace rstd::prelude;\n\nexport namespace "_str);
        text.push_str(file.cpp_namespace.as_str());
        text.push_str(" {\n"_str);
        for (auto& enumeration : file.enums) {
            text.push_str("enum class "_str);
            text.push_str(enumeration.name.as_str());
            text.push_str(" : int {\n"_str);
            for (auto& item : enumeration.values) {
                text.push_str("    v_"_str);
                text.push_str(item.name.as_str());
                text.push_str(" = "_str);
                auto number = static_cast<long long>(item.number.to_primitive());
                if (number < 0) {
                    text.push_str("-"_str);
                    number = -number;
                }
                text.push_str(decimal(static_cast<unsigned>(number)).as_str());
                text.push_str(",\n"_str);
            }
            text.push_str("};\ninline auto known_"_str);
            text.push_str(enumeration.name.as_str());
            text.push_str("(i32 value) -> Option<"_str);
            text.push_str(enumeration.name.as_str());
            text.push_str("> {\n"_str);
            for (auto& item : enumeration.values) {
                text.push_str("    if (value.to_primitive() == static_cast<int>("_str);
                text.push_str(enumeration.name.as_str());
                text.push_str("::v_"_str);
                text.push_str(item.name.as_str());
                text.push_str(")) return Some("_str);
                text.push_str(enumeration.name.as_str());
                text.push_str("::v_"_str);
                text.push_str(item.name.as_str());
                text.push_str(");\n"_str);
            }
            text.push_str("    return None();\n}\n"_str);
        }
        for (auto& message : file.messages) {
            text.push_str("struct "_str);
            text.push_str(message.name.as_str());
            text.push_str(";\n"_str);
        }
        for (auto& message : file.messages) {
            text.push_str("struct "_str);
            text.push_str(message.name.as_str());
            text.push_str(" {\n"_str);
            for (usize i {}; i < message.fields.len(); ++i) {
                auto& field = message.fields[i];
                if (field.oneof.as_str().is_empty()) continue;
                bool prior = false;
                for (usize j {}; j < i; ++j)
                    if (message.fields[j].oneof == field.oneof) prior = true;
                if (prior) continue;
                emit_oneof(text, message, field);
            }
            for (auto& field : message.fields) {
                if (! field.oneof.as_str().is_empty()) continue;
                if (field.map_key) {
                    text.push_str("    liproto::Map<"_str);
                    text.push_str(map_arguments(field).as_str());
                    text.push_str("> "_str);
                    text.push_str(field.name.as_str());
                    text.push_str(" {};\n"_str);
                    continue;
                }
                text.push_str("    "_str);
                if (field.repeated)
                    text.push_str("::alloc::vec::Vec<"_str);
                else if (field.optional)
                    text.push_str("Option<"_str);
                if (field.kind == 11) {
                    text.push_str("::alloc::boxed::Box<"_str);
                    text.push_str(field.message_type.as_str());
                    text.push_str(">"_str);
                } else {
                    text.push_str("liproto::Scalar<"_str);
                    text.push_str(decimal(field.kind).as_str());
                    text.push_str(">::Type"_str);
                }
                if (field.repeated || field.optional) text.push_str(">"_str);
                text.push_str(" "_str);
                text.push_str(field.name.as_str());
                text.push_str(" {};\n"_str);
            }
            text.push_str("};\n"_str);
        }
        text.push_str("}\n\nexport namespace rstd {\n"_str);
        for (auto& message : file.messages) {
            String type = String::make("::"_str);
            type.push_str(file.cpp_namespace.as_str());
            type.push_str("::"_str);
            type.push_str(message.name.as_str());
            text.push_str("template<> struct Impl<liproto::Message, "_str);
            text.push_str(type.as_str());
            text.push_str("> {\n    using T = "_str);
            text.push_str(type.as_str());
            text.push_str(
                ";\n    static void clear(T& value) { value = T{}; }\n    static auto merge_field(T& value, liproto::FieldKey key, liproto::Reader& reader) -> Result<empty, liproto::Error> {\n        switch (key.number.to_primitive()) {\n"_str);
            for (auto& field : message.fields) {
                auto kind = decimal(field.kind);
                text.push_str("        case "_str);
                text.push_str(decimal(field.number).as_str());
                text.push_str(": {\n"_str);
                if (field.map_key) {
                    text.push_str("            return liproto::read_map<"_str);
                    text.push_str(map_arguments(field).as_str());
                    text.push_str(">(value."_str);
                    text.push_str(field.name.as_str());
                    text.push_str(", key, reader);\n"_str);
                } else if (! field.oneof.as_str().is_empty()) {
                    if (field.kind == 11) {
                        text.push_str(
                            "            if (key.wire != liproto::WireType::Bytes) return Err(reader.error(liproto::ErrorKind::WrongWire));\n            if (value."_str);
                        text.push_str(field.oneof.as_str());
                        text.push_str(".is_some() && value."_str);
                        text.push_str(field.oneof.as_str());
                        text.push_str("->is_"_str);
                        text.push_str(field.variant.as_str());
                        text.push_str("()) return liproto::merge_message(*value."_str);
                        text.push_str(field.oneof.as_str());
                        text.push_str("->as_"_str);
                        text.push_str(field.variant.as_str());
                        text.push_str("(), reader);\n            auto item = "_str);
                        text.push_str(field_type(field).as_str());
                        text.push_str(
                            "::make();\n            auto result = liproto::merge_message(*item, reader);\n            if (result.is_err()) return result;\n"_str);
                    } else {
                        text.push_str("            auto result = liproto::read_scalar<"_str);
                        text.push_str(kind.as_str());
                        text.push_str(
                            ">(key, reader);\n            if (result.is_err()) return Err(result.unwrap_err());\n            auto item = rstd::move(*result);\n"_str);
                    }
                    text.push_str("            value."_str);
                    text.push_str(field.oneof.as_str());
                    text.push_str(" = Some(T::C_"_str);
                    text.push_str(field.oneof.as_str());
                    text.push_str("::make_"_str);
                    text.push_str(field.variant.as_str());
                    text.push_str("(rstd::move(item)));\n            return Ok(empty{});\n"_str);
                } else if (field.kind == 11) {
                    text.push_str("            return liproto::read_message_field(value."_str);
                    text.push_str(field.name.as_str());
                    text.push_str(", key, reader);\n"_str);
                } else if (field.repeated) {
                    text.push_str("            return liproto::read_repeated<"_str);
                    text.push_str(kind.as_str());
                    text.push_str(">(value."_str);
                    text.push_str(field.name.as_str());
                    text.push_str(", key, reader);\n"_str);
                } else {
                    text.push_str("            auto result = liproto::read_scalar<"_str);
                    text.push_str(kind.as_str());
                    text.push_str(
                        ">(key, reader);\n            if (result.is_err()) return Err(result.unwrap_err());\n            value."_str);
                    text.push_str(field.name.as_str());
                    text.push_str(" = "_str);
                    if (field.optional) text.push_str("Some("_str);
                    text.push_str("rstd::move(*result)"_str);
                    if (field.optional) text.push_str(")"_str);
                    text.push_str(";\n            return Ok(empty{});\n"_str);
                }
                text.push_str("        }\n"_str);
            }
            text.push_str(
                "        default: return reader.skip(key);\n        }\n    }\n    static auto encode_fields(const T& value, liproto::Writer& writer) -> Result<empty, liproto::Error> {\n"_str);
            for (auto& field : message.fields) {
                if (field.packed) {
                    text.push_str("        { auto result = liproto::write_packed<"_str);
                    text.push_str(decimal(field.kind).as_str());
                    text.push_str(">(u32("_str);
                    text.push_str(decimal(field.number).as_str());
                    text.push_str("), value."_str);
                    text.push_str(field.name.as_str());
                    text.push_str(", writer); if (result.is_err()) return result; }\n"_str);
                    continue;
                }
                if (field.map_key) {
                    text.push_str("        { auto result = liproto::write_map<"_str);
                    text.push_str(map_arguments(field).as_str());
                    text.push_str(">(u32("_str);
                    text.push_str(decimal(field.number).as_str());
                    text.push_str("), value."_str);
                    text.push_str(field.name.as_str());
                    text.push_str(", writer); if (result.is_err()) return result; }\n"_str);
                    continue;
                }
                String access = String::make("value."_str);
                access.push_str(field.name.as_str());
                if (! field.oneof.as_str().is_empty()) {
                    text.push_str("        if (value."_str);
                    text.push_str(field.oneof.as_str());
                    text.push_str(".is_some() && value."_str);
                    text.push_str(field.oneof.as_str());
                    text.push_str("->is_"_str);
                    text.push_str(field.variant.as_str());
                    text.push_str("()) {\n"_str);
                    access = String::make("value."_str);
                    access.push_str(field.oneof.as_str());
                    access.push_str("->as_"_str);
                    access.push_str(field.variant.as_str());
                    access.push_str("()"_str);
                } else if (field.repeated) {
                    text.push_str("        for (const auto& item : "_str);
                    text.push_str(access.as_str());
                    text.push_str(") {\n"_str);
                } else {
                    text.push_str("        if ("_str);
                    text.push_str(access.as_str());
                    if (field.optional)
                        text.push_str(".is_some()"_str);
                    else if (field.kind == 9)
                        text.push_str(".as_str().len() != usize()"_str);
                    else if (field.kind == 12)
                        text.push_str(".len() != usize()"_str);
                    else {
                        text.push_str(" != liproto::Scalar<"_str);
                        text.push_str(decimal(field.kind).as_str());
                        text.push_str(">::Type{}"_str);
                    }
                    text.push_str(") {\n"_str);
                }
                if (field.kind == 11) {
                    text.push_str("            auto result = writer.key({u32("_str);
                    text.push_str(decimal(field.number).as_str());
                    text.push_str(
                        "), liproto::WireType::Bytes});\n            if (result.is_err()) return result;\n            result = liproto::encode_message("_str);
                } else {
                    text.push_str("            auto result = liproto::write_scalar<"_str);
                    text.push_str(decimal(field.kind).as_str());
                    text.push_str(">(u32("_str);
                    text.push_str(decimal(field.number).as_str());
                    text.push_str("), "_str);
                }
                if (field.kind == 11) text.push_str("*"_str);
                if (field.repeated)
                    text.push_str("item"_str);
                else {
                    if (field.optional && field.oneof.as_str().is_empty()) text.push_str("*"_str);
                    text.push_str(access.as_str());
                }
                text.push_str(
                    ", writer);\n            if (result.is_err()) return result;\n        }\n"_str);
            }
            text.push_str("        return Ok(empty{});\n    }\n};\n"_str);
        }
        text.push_str("}\n"_str);
        files.push(GeneratedFile { String::make(file.path.as_str()), rstd::move(text) });
    }
    return files;
}
export auto encode_response(slice<GeneratedFile> files, ref<str> error = ""_str)
    -> Result<Vec<u8>, Error> {
    Vec<u8> bytes;
    Writer  writer(bytes);
    if (! error.is_empty()) {
        auto result = writer.key({ u32(1), WireType::Bytes });
        if (result.is_err()) return Err(result.unwrap_err());
        result = writer.bytes(error.as_bytes());
        if (result.is_err()) return Err(result.unwrap_err());
    } else {
        auto result = writer.key({ u32(2), WireType::Varint });
        if (result.is_err()) return Err(result.unwrap_err());
        result = writer.varint(u64(1));
        if (result.is_err()) return Err(result.unwrap_err());
        for (auto& file : files) {
            Vec<u8> contents;
            Writer  entry(contents);
            result = entry.key({ u32(1), WireType::Bytes });
            if (result.is_err()) return Err(result.unwrap_err());
            result = entry.bytes(file.path.as_str().as_bytes());
            if (result.is_err()) return Err(result.unwrap_err());
            result = entry.key({ u32(15), WireType::Bytes });
            if (result.is_err()) return Err(result.unwrap_err());
            result = entry.bytes(file.content.as_str().as_bytes());
            if (result.is_err()) return Err(result.unwrap_err());
            result = writer.key({ u32(15), WireType::Bytes });
            if (result.is_err()) return Err(result.unwrap_err());
            result = writer.bytes(contents.as_slice());
            if (result.is_err()) return Err(result.unwrap_err());
        }
    }
    return Ok(rstd::move(bytes));
}
} // namespace liproto::codegen
