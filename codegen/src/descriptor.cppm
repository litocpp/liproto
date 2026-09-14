export module liproto.codegen:descriptor;
export import liproto;

using namespace rstd::prelude;
using ::alloc::string::String;
using ::alloc::vec::Vec;

namespace liproto::codegen
{
export struct EnumValue {
    String name;
    i32    number {};
};
export struct EnumDescriptor {
    String         name;
    Vec<EnumValue> values;
    bool           unsupported {};
    bool           allow_alias {};
};
export struct Field {
    String           name;
    String           type_name;
    unsigned         number {};
    unsigned         type {};
    unsigned         label {};
    Option<unsigned> oneof;
    Option<bool>     packed;
    bool             optional {};
    bool             unsupported {};
};
export struct MessageDescriptor {
    String                 name;
    Vec<Field>             fields;
    Vec<MessageDescriptor> nested;
    Vec<EnumDescriptor>    enums;
    Vec<String>            oneofs;
    bool                   map_entry {};
    bool                   unsupported {};
};
export struct FileDescriptor {
    String                 name;
    String                 package;
    String                 syntax;
    Vec<String>            imports;
    Vec<u32>               public_imports;
    Vec<MessageDescriptor> messages;
    Vec<EnumDescriptor>    enums;
    bool                   unsupported {};
};
export struct Request {
    Vec<String>         roots;
    String              parameter;
    Vec<FileDescriptor> files;
};

auto read_string(Reader& reader, FieldKey key, String& output) -> Result<empty, Error> {
    if (key.wire != WireType::Bytes) return Err(reader.error(ErrorKind::WrongWire, key.number));
    auto text = reader.text();
    if (text.is_err()) return Err(text.unwrap_err());
    output = String::make(*text);
    return Ok(empty {});
}
auto read_number(Reader& reader, FieldKey key) -> Result<unsigned, Error> {
    if (key.wire != WireType::Varint) return Err(reader.error(ErrorKind::WrongWire, key.number));
    auto number = reader.varint();
    if (number.is_err()) return Err(number.unwrap_err());
    if (number->to_primitive() > 0xffffffffULL) return Err(reader.error(ErrorKind::InvalidVarint));
    return Ok(static_cast<unsigned>(number->to_primitive()));
}
template<typename F>
auto read_nested(Reader& parent, FieldKey key, F&& consume) -> Result<empty, Error> {
    if (key.wire != WireType::Bytes) return Err(parent.error(ErrorKind::WrongWire, key.number));
    auto bytes = parent.bytes();
    if (bytes.is_err()) return Err(bytes.unwrap_err());
    if (parent.depth() >= parent.context().options.max_depth)
        return Err(parent.error(ErrorKind::DepthLimit));
    Reader child(*bytes, parent.context(), parent.offset() - bytes->len(), parent.depth() + 1);
    while (! child.empty()) {
        auto field = child.key();
        if (field.is_err()) return Err(field.unwrap_err());
        auto result = consume(child, *field);
        if (result.is_err()) return result;
    }
    return Ok(empty {});
}

auto field_field(Field& value, Reader& reader, FieldKey key) -> Result<empty, Error> {
    auto tag = key.number.to_primitive();
    if (tag == 1) return read_string(reader, key, value.name);
    if (tag == 6) return read_string(reader, key, value.type_name);
    if (tag == 3 || tag == 4 || tag == 5 || tag == 9 || tag == 17) {
        auto number = read_number(reader, key);
        if (number.is_err()) return Err(number.unwrap_err());
        if (tag == 3) value.number = *number;
        if (tag == 4) value.label = *number;
        if (tag == 5) value.type = *number;
        if (tag == 9) value.oneof = Some(*number);
        if (tag == 17) value.optional = *number != 0;
        return Ok(empty {});
    }
    if (tag == 2 || tag == 7) value.unsupported = true;
    if (tag == 8)
        return read_nested(
            reader, key, [&](Reader& input, FieldKey option) -> Result<empty, Error> {
                if (option.number == u32(2)) {
                    auto number = read_number(input, option);
                    if (number.is_err()) return Err(number.unwrap_err());
                    value.packed = Some(*number != 0);
                    return Ok(empty {});
                }
                if (option.number != u32(3)) value.unsupported = true;
                return input.skip(option);
            });
    return reader.skip(key);
}

auto enum_field(EnumDescriptor& value, Reader& reader, FieldKey key) -> Result<empty, Error> {
    if (key.number == u32(1)) return read_string(reader, key, value.name);
    if (key.number == u32(2)) {
        EnumValue item;
        auto      result =
            read_nested(reader, key, [&](Reader& input, FieldKey field) -> Result<empty, Error> {
                if (field.number == u32(1)) return read_string(input, field, item.name);
                if (field.number == u32(2)) {
                    auto number = read_scalar<14>(field, input);
                    if (number.is_err()) return Err(number.unwrap_err());
                    item.number = *number;
                    return Ok(empty {});
                }
                return input.skip(field);
            });
        if (result.is_err()) return result;
        value.values.push(rstd::move(item));
        return Ok(empty {});
    }
    if (key.number == u32(3))
        return read_nested(
            reader, key, [&](Reader& input, FieldKey option) -> Result<empty, Error> {
                if (option.number == u32(2)) {
                    auto flag = read_scalar<8>(option, input);
                    if (flag.is_err()) return Err(flag.unwrap_err());
                    value.allow_alias = *flag;
                    return Ok(empty {});
                }
                if (option.number != u32(2) && option.number != u32(3)) value.unsupported = true;
                return input.skip(option);
            });
    return reader.skip(key);
}

auto message_field(MessageDescriptor& value, Reader& reader, FieldKey key) -> Result<empty, Error> {
    auto tag = key.number.to_primitive();
    if (tag == 1) return read_string(reader, key, value.name);
    if (tag == 2) {
        Field field;
        auto  result = read_nested(reader, key, [&](Reader& input, FieldKey key) {
            return field_field(field, input, key);
        });
        if (result.is_err()) return result;
        value.fields.push(rstd::move(field));
        return Ok(empty {});
    }
    if (tag == 3) {
        MessageDescriptor child;
        auto              result = read_nested(reader, key, [&](Reader& input, FieldKey key) {
            return message_field(child, input, key);
        });
        if (result.is_err()) return result;
        value.nested.push(rstd::move(child));
        return Ok(empty {});
    }
    if (tag == 8) {
        String name;
        auto   result = read_nested(reader, key, [&](Reader& input, FieldKey field) {
            return field.number == u32(1) ? read_string(input, field, name) : input.skip(field);
        });
        if (result.is_err()) return result;
        value.oneofs.push(rstd::move(name));
        return Ok(empty {});
    }
    if (tag == 7)
        return read_nested(
            reader, key, [&](Reader& input, FieldKey option) -> Result<empty, Error> {
                if (option.number == u32(7)) {
                    auto flag = read_number(input, option);
                    if (flag.is_err()) return Err(flag.unwrap_err());
                    value.map_entry = *flag != 0;
                    return Ok(empty {});
                }
                if (option.number != u32(3)) value.unsupported = true;
                return input.skip(option);
            });
    if (tag == 4) {
        EnumDescriptor enumeration;
        auto           result = read_nested(reader, key, [&](Reader& input, FieldKey field) {
            return enum_field(enumeration, input, field);
        });
        if (result.is_err()) return result;
        value.enums.push(rstd::move(enumeration));
        return Ok(empty {});
    }
    if (tag == 5 || tag == 6) value.unsupported = true;
    return reader.skip(key);
}

auto file_field(FileDescriptor& value, Reader& reader, FieldKey key) -> Result<empty, Error> {
    auto tag = key.number.to_primitive();
    if (tag == 1) return read_string(reader, key, value.name);
    if (tag == 2) return read_string(reader, key, value.package);
    if (tag == 12) return read_string(reader, key, value.syntax);
    if (tag == 10) return read_repeated<13>(value.public_imports, key, reader);
    if (tag == 3) {
        String name;
        auto   result = read_string(reader, key, name);
        if (result.is_err()) return result;
        value.imports.push(rstd::move(name));
        return Ok(empty {});
    }
    if (tag == 4) {
        MessageDescriptor message;
        auto              result = read_nested(reader, key, [&](Reader& input, FieldKey key) {
            return message_field(message, input, key);
        });
        if (result.is_err()) return result;
        value.messages.push(rstd::move(message));
        return Ok(empty {});
    }
    if (tag == 5) {
        EnumDescriptor enumeration;
        auto           result = read_nested(reader, key, [&](Reader& input, FieldKey field) {
            return enum_field(enumeration, input, field);
        });
        if (result.is_err()) return result;
        value.enums.push(rstd::move(enumeration));
        return Ok(empty {});
    }
    if (tag == 8)
        return read_nested(reader, key, [&](Reader& input, FieldKey option) {
            if (option.number == u32(50) || option.number >= u32(999)) value.unsupported = true;
            return input.skip(option);
        });
    if (tag == 6 || tag == 7 || tag == 14) value.unsupported = true;
    return reader.skip(key);
}

export auto decode_request(slice<u8> bytes) -> Result<Request, Error> {
    Request       request;
    DecodeContext context;
    Reader        reader(bytes, context);
    auto          valid = reader.validate();
    if (valid.is_err()) return Err(valid.unwrap_err());
    while (! reader.empty()) {
        auto key = reader.key();
        if (key.is_err()) return Err(key.unwrap_err());
        Result<empty, Error> result = Ok(empty {});
        if (key->number == u32(1)) {
            String root;
            result = read_string(reader, *key, root);
            if (result.is_ok()) request.roots.push(rstd::move(root));
        } else if (key->number == u32(2))
            result = read_string(reader, *key, request.parameter);
        else if (key->number == u32(15)) {
            FileDescriptor file;
            result = read_nested(reader, *key, [&](Reader& input, FieldKey key) {
                return file_field(file, input, key);
            });
            if (result.is_ok()) request.files.push(rstd::move(file));
        } else
            result = reader.skip(*key);
        if (result.is_err()) return Err(result.unwrap_err());
    }
    return Ok(rstd::move(request));
}
export auto decode_descriptor_set(slice<u8> bytes, ref<str> root) -> Result<Request, Error> {
    Request request;
    request.roots.push(String::make(root));
    DecodeContext context;
    Reader        reader(bytes, context);
    auto          valid = reader.validate();
    if (valid.is_err()) return Err(valid.unwrap_err());
    while (! reader.empty()) {
        auto key = reader.key();
        if (key.is_err()) return Err(key.unwrap_err());
        if (key->number == u32(1)) {
            FileDescriptor file;
            auto           result = read_nested(reader, *key, [&](Reader& input, FieldKey field) {
                return file_field(file, input, field);
            });
            if (result.is_err()) return Err(result.unwrap_err());
            request.files.push(rstd::move(file));
        } else {
            auto skipped = reader.skip(*key);
            if (skipped.is_err()) return Err(skipped.unwrap_err());
        }
    }
    return Ok(rstd::move(request));
}
} // namespace liproto::codegen
