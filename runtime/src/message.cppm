export module liproto:message;
export import :wire;

using namespace rstd::prelude;
using ::alloc::vec::Vec;

export namespace liproto
{
struct Message {};

template<typename T>
auto merge_fields(T& value, Reader& reader) -> Result<empty, Error> {
    auto valid = reader.validate();
    if (valid.is_err()) return valid;
    while (! reader.empty()) {
        auto key = reader.key();
        if (key.is_err()) return Err(key.unwrap_err());
        if (key->wire == WireType::EndGroup)
            return Err(reader.error(ErrorKind::UnexpectedEndGroup, key->number));
        auto result = rstd::Impl<Message, T>::merge_field(value, *key, reader);
        if (result.is_err()) {
            auto error = result.unwrap_err();
            if (error.field == u32()) error.field = key->number;
            return Err(error);
        }
    }
    return Ok(empty {});
}

template<typename T>
auto merge(T& value, slice<u8> bytes, DecodeOptions options = {}) -> Result<empty, Error> {
    DecodeContext context(options);
    Reader        reader(bytes, context);
    return merge_fields(value, reader);
}

template<typename T>
auto decode(slice<u8> bytes, DecodeOptions options = {}) -> Result<T, Error> {
    T    value {};
    auto result = merge(value, bytes, options);
    if (result.is_err()) return Err(result.unwrap_err());
    return Ok(rstd::move(value));
}

template<typename T>
auto merge_message(T& value, Reader& parent) -> Result<empty, Error> {
    auto bytes = parent.bytes();
    if (bytes.is_err()) return Err(bytes.unwrap_err());
    if (parent.depth() >= parent.context().options.max_depth)
        return Err(parent.error(ErrorKind::DepthLimit));
    Reader child(*bytes, parent.context(), parent.offset() - bytes->len(), parent.depth() + 1);
    return merge_fields(value, child);
}

template<typename T>
auto read_message_field(Option<::alloc::boxed::Box<T>>& value, FieldKey key, Reader& reader)
    -> Result<empty, Error> {
    if (key.wire != WireType::Bytes) return Err(reader.error(ErrorKind::WrongWire, key.number));
    if (value.is_none()) value = Some(::alloc::boxed::Box<T>::make());
    return merge_message(**value, reader);
}

template<typename T>
auto read_message_field(Vec<::alloc::boxed::Box<T>>& value, FieldKey key, Reader& reader)
    -> Result<empty, Error> {
    if (key.wire != WireType::Bytes) return Err(reader.error(ErrorKind::WrongWire, key.number));
    auto message = ::alloc::boxed::Box<T>::make();
    auto result  = merge_message(*message, reader);
    if (result.is_err()) return result;
    value.push(rstd::move(message));
    return Ok(empty {});
}

template<typename T>
auto encoded_len(const T& value) -> Result<usize, Error> {
    Writer counter(usize(~rstd::size_t(0)));
    auto   result = rstd::Impl<Message, T>::encode_fields(value, counter);
    if (result.is_err()) return Err(Error { ErrorKind::SizeOverflow, counter.written() });
    return Ok(counter.written());
}

template<typename T>
auto encode(const T& value, Writer& writer) -> Result<empty, Error> {
    auto length = encoded_len(value);
    if (length.is_err()) return Err(length.unwrap_err());
    if (*length > writer.available()) return Err(Error { ErrorKind::Capacity, writer.written() });
    return rstd::Impl<Message, T>::encode_fields(value, writer);
}

template<typename T>
auto encode_to_vec(const T& value, usize limit = usize(64 * 1024 * 1024))
    -> Result<Vec<u8>, Error> {
    auto   bytes = Vec<u8>::make();
    Writer writer(bytes, limit);
    auto   result = encode(value, writer);
    if (result.is_err()) return Err(result.unwrap_err());
    return Ok(rstd::move(bytes));
}

template<typename T>
auto encode_message(const T& value, Writer& writer) -> Result<empty, Error> {
    auto length = encoded_len(value);
    if (length.is_err()) return Err(length.unwrap_err());
    auto prefix = varint_len(u64(length->to_primitive()));
    if (prefix > writer.available() || *length > writer.available() - prefix)
        return Err(Error { ErrorKind::Capacity, writer.written() });
    auto result = writer.varint(u64(length->to_primitive()));
    if (result.is_err()) return result;
    return rstd::Impl<Message, T>::encode_fields(value, writer);
}

template<typename T>
auto decode_length_delimited(Reader& reader) -> Result<T, Error> {
    T    value {};
    auto result = merge_message(value, reader);
    if (result.is_err()) return Err(result.unwrap_err());
    return Ok(rstd::move(value));
}
} // namespace liproto
