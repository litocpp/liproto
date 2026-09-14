export module liproto:map;
export import :scalar;
export import :message;
using namespace rstd::prelude;

export namespace liproto
{
template<unsigned Kind, typename M>
struct MapValue {
    using Type = typename Scalar<Kind>::Type;
    static auto make() -> Type { return {}; }
    static auto read(Type& value, FieldKey key, Reader& reader) -> Result<empty, Error> {
        auto parsed = read_scalar<Kind>(key, reader);
        if (parsed.is_err()) return Err(parsed.unwrap_err());
        value = rstd::move(*parsed);
        return Ok(empty {});
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return write_scalar<Kind>(u32(2), value, writer);
    }
};
template<typename M>
struct MapValue<11, M> {
    using Type = ::alloc::boxed::Box<M>;
    static auto make() -> Type { return Type::make(); }
    static auto read(Type& value, FieldKey key, Reader& reader) -> Result<empty, Error> {
        if (key.wire != WireType::Bytes) return Err(reader.error(ErrorKind::WrongWire));
        return merge_message(*value, reader);
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        auto key = writer.key({ u32(2), WireType::Bytes });
        if (key.is_err()) return key;
        return encode_message(*value, writer);
    }
};
template<unsigned KeyKind, unsigned ValueKind, typename M = empty>
using Map = ::alloc::collections::BTreeMap<typename Scalar<KeyKind>::Type,
                                           typename MapValue<ValueKind, M>::Type>;

template<unsigned KeyKind, unsigned ValueKind, typename M = empty>
auto read_map(Map<KeyKind, ValueKind, M>& map, FieldKey key, Reader& reader)
    -> Result<empty, Error> {
    if (key.wire != WireType::Bytes) return Err(reader.error(ErrorKind::WrongWire));
    auto bytes = reader.bytes();
    if (bytes.is_err()) return Err(bytes.unwrap_err());
    if (reader.depth() >= reader.context().options.max_depth)
        return Err(reader.error(ErrorKind::DepthLimit));
    Reader entry(*bytes, reader.context(), reader.offset() - bytes->len(), reader.depth() + 1);
    typename Scalar<KeyKind>::Type entry_key {};
    auto                           value = MapValue<ValueKind, M>::make();
    while (! entry.empty()) {
        auto field = entry.key();
        if (field.is_err()) return Err(field.unwrap_err());
        if (field->number == u32(1)) {
            auto parsed = read_scalar<KeyKind>(*field, entry);
            if (parsed.is_err()) return Err(parsed.unwrap_err());
            entry_key = rstd::move(*parsed);
        } else {
            auto result = field->number == u32(2)
                              ? MapValue<ValueKind, M>::read(value, *field, entry)
                              : entry.skip(*field);
            if (result.is_err()) return result;
        }
    }
    (void)map.insert(rstd::move(entry_key), rstd::move(value));
    return Ok(empty {});
}

template<unsigned KeyKind, unsigned ValueKind, typename M = empty>
auto write_map(u32 number, const Map<KeyKind, ValueKind, M>& map, Writer& writer)
    -> Result<empty, Error> {
    for (auto [key, value] : map.iter()) {
        auto emit = [&](Writer& output) -> Result<empty, Error> {
            auto result = write_scalar<KeyKind>(u32(1), *key, output);
            if (result.is_err()) return result;
            return MapValue<ValueKind, M>::write(*value, output);
        };
        Writer count(usize(~rstd::size_t(0)));
        auto   result = emit(count);
        if (result.is_err()) return result;
        result = writer.key({ number, WireType::Bytes });
        if (result.is_err()) return result;
        result = writer.varint(u64(count.written().to_primitive()));
        if (result.is_err()) return result;
        result = emit(writer);
        if (result.is_err()) return result;
    }
    return Ok(empty {});
}
} // namespace liproto
