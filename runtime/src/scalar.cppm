export module liproto:scalar;
export import :wire;
using namespace rstd::prelude;
using ::alloc::string::String;
using ::alloc::vec::Vec;
export namespace liproto
{
template<unsigned Kind>
struct Scalar;

template<>
struct Scalar<1> {
    using Type                     = f64;
    static constexpr WireType wire = WireType::Fixed64;
    static auto               read(Reader& reader) -> Result<Type, Error> {
        auto raw = reader.fixed(8);
        if (raw.is_err()) return Err(raw.unwrap_err());
        return Ok(Type(
            rstd::mem::transmute<double>(static_cast<unsigned long long>(raw->to_primitive()))));
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return writer.fixed(u64(rstd::mem::transmute<unsigned long long>(value.to_primitive())), 8);
    }
};

template<>
struct Scalar<2> {
    using Type                     = f32;
    static constexpr WireType wire = WireType::Fixed32;
    static auto               read(Reader& reader) -> Result<Type, Error> {
        auto raw = reader.fixed(4);
        if (raw.is_err()) return Err(raw.unwrap_err());
        return Ok(Type(rstd::mem::transmute<float>(static_cast<unsigned>(raw->to_primitive()))));
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return writer.fixed(u64(rstd::mem::transmute<unsigned>(value.to_primitive())), 4);
    }
};

template<>
struct Scalar<3> {
    using Type                     = i64;
    static constexpr WireType wire = WireType::Varint;
    static auto               read(Reader& reader) -> Result<Type, Error> {
        auto raw = reader.varint();
        if (raw.is_err()) return Err(raw.unwrap_err());
        return Ok(Type(
            rstd::mem::transmute<long long>(static_cast<unsigned long long>(raw->to_primitive()))));
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return writer.varint(u64(static_cast<unsigned long long>(value.to_primitive())));
    }
};

template<>
struct Scalar<4> {
    using Type                     = u64;
    static constexpr WireType wire = WireType::Varint;
    static auto               read(Reader& reader) -> Result<Type, Error> {
        auto raw = reader.varint();
        if (raw.is_err()) return Err(raw.unwrap_err());
        return Ok(Type(raw->to_primitive()));
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return writer.varint(value);
    }
};

template<>
struct Scalar<5> {
    using Type                     = i32;
    static constexpr WireType wire = WireType::Varint;
    static auto               read(Reader& reader) -> Result<Type, Error> {
        auto raw = reader.varint();
        if (raw.is_err()) return Err(raw.unwrap_err());
        return Ok(Type(rstd::mem::transmute<int>(static_cast<unsigned>(raw->to_primitive()))));
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return writer.varint(
            u64(static_cast<unsigned long long>(static_cast<long long>(value.to_primitive()))));
    }
};

template<>
struct Scalar<6> {
    using Type                     = u64;
    static constexpr WireType wire = WireType::Fixed64;
    static auto               read(Reader& reader) -> Result<Type, Error> {
        auto raw = reader.fixed(8);
        if (raw.is_err()) return Err(raw.unwrap_err());
        return Ok(Type(raw->to_primitive()));
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return writer.fixed(value, 8);
    }
};

template<>
struct Scalar<7> {
    using Type                     = u32;
    static constexpr WireType wire = WireType::Fixed32;
    static auto               read(Reader& reader) -> Result<Type, Error> {
        auto raw = reader.fixed(4);
        if (raw.is_err()) return Err(raw.unwrap_err());
        return Ok(Type(raw->to_primitive()));
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return writer.fixed(u64(value.to_primitive()), 4);
    }
};

template<>
struct Scalar<8> {
    using Type                     = bool;
    static constexpr WireType wire = WireType::Varint;
    static auto               read(Reader& reader) -> Result<Type, Error> {
        auto raw = reader.varint();
        if (raw.is_err()) return Err(raw.unwrap_err());
        return Ok(Type(raw->to_primitive() != 0));
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return writer.varint(u64(value ? 1 : 0));
    }
};

template<>
struct Scalar<13> {
    using Type                     = u32;
    static constexpr WireType wire = WireType::Varint;
    static auto               read(Reader& reader) -> Result<Type, Error> {
        auto raw = reader.varint();
        if (raw.is_err()) return Err(raw.unwrap_err());
        return Ok(Type(raw->to_primitive()));
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return writer.varint(u64(value.to_primitive()));
    }
};

template<>
struct Scalar<14> {
    using Type                     = i32;
    static constexpr WireType wire = WireType::Varint;
    static auto               read(Reader& reader) -> Result<Type, Error> {
        auto raw = reader.varint();
        if (raw.is_err()) return Err(raw.unwrap_err());
        return Ok(Type(rstd::mem::transmute<int>(static_cast<unsigned>(raw->to_primitive()))));
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return writer.varint(
            u64(static_cast<unsigned long long>(static_cast<long long>(value.to_primitive()))));
    }
};

template<>
struct Scalar<15> {
    using Type                     = i32;
    static constexpr WireType wire = WireType::Fixed32;
    static auto               read(Reader& reader) -> Result<Type, Error> {
        auto raw = reader.fixed(4);
        if (raw.is_err()) return Err(raw.unwrap_err());
        return Ok(Type(rstd::mem::transmute<int>(static_cast<unsigned>(raw->to_primitive()))));
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return writer.fixed(u64(static_cast<unsigned>(value.to_primitive())), 4);
    }
};

template<>
struct Scalar<16> {
    using Type                     = i64;
    static constexpr WireType wire = WireType::Fixed64;
    static auto               read(Reader& reader) -> Result<Type, Error> {
        auto raw = reader.fixed(8);
        if (raw.is_err()) return Err(raw.unwrap_err());
        return Ok(Type(
            rstd::mem::transmute<long long>(static_cast<unsigned long long>(raw->to_primitive()))));
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return writer.fixed(u64(static_cast<unsigned long long>(value.to_primitive())), 8);
    }
};

template<>
struct Scalar<17> {
    using Type                     = i32;
    static constexpr WireType wire = WireType::Varint;
    static auto               read(Reader& reader) -> Result<Type, Error> {
        auto raw = reader.varint();
        if (raw.is_err()) return Err(raw.unwrap_err());
        return Ok(Type(unzigzag(u64(static_cast<unsigned>(raw->to_primitive()))).to_primitive()));
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return writer.varint(zigzag(i64(value.to_primitive())));
    }
};

template<>
struct Scalar<18> {
    using Type                     = i64;
    static constexpr WireType wire = WireType::Varint;
    static auto               read(Reader& reader) -> Result<Type, Error> {
        auto raw = reader.varint();
        if (raw.is_err()) return Err(raw.unwrap_err());
        return Ok(Type(unzigzag(*raw).to_primitive()));
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return writer.varint(zigzag(value));
    }
};

template<>
struct Scalar<9> {
    using Type                     = String;
    static constexpr WireType wire = WireType::Bytes;
    static auto               read(Reader& reader) -> Result<Type, Error> {
        auto text = reader.text();
        if (text.is_err()) return Err(text.unwrap_err());
        return Ok(String::make(*text));
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return writer.bytes(value.as_str().as_bytes());
    }
};
template<>
struct Scalar<12> {
    using Type                     = Vec<u8>;
    static constexpr WireType wire = WireType::Bytes;
    static auto               read(Reader& reader) -> Result<Type, Error> {
        auto bytes = reader.bytes();
        if (bytes.is_err()) return Err(bytes.unwrap_err());
        auto result = Vec<u8>::make();
        result.extend_from_slice(*bytes);
        return Ok(rstd::move(result));
    }
    static auto write(const Type& value, Writer& writer) -> Result<empty, Error> {
        return writer.bytes(value.as_slice());
    }
};
template<unsigned Kind>
auto read_scalar(FieldKey key, Reader& reader) -> Result<typename Scalar<Kind>::Type, Error> {
    if (key.wire != Scalar<Kind>::wire) return Err(reader.error(ErrorKind::WrongWire, key.number));
    return Scalar<Kind>::read(reader);
}
template<unsigned Kind>
auto write_scalar(u32 number, const typename Scalar<Kind>::Type& value, Writer& writer)
    -> Result<empty, Error> {
    auto key = writer.key({ number, Scalar<Kind>::wire });
    if (key.is_err()) return key;
    return Scalar<Kind>::write(value, writer);
}
template<unsigned Kind>
auto read_repeated(Vec<typename Scalar<Kind>::Type>& values, FieldKey key, Reader& reader)
    -> Result<empty, Error> {
    if constexpr (Scalar<Kind>::wire != WireType::Bytes) {
        if (key.wire == WireType::Bytes) {
            auto bytes = reader.bytes();
            if (bytes.is_err()) return Err(bytes.unwrap_err());
            Reader packed(*bytes, reader.context(), reader.offset() - bytes->len(), reader.depth());
            while (! packed.empty()) {
                if (reader.context().fields >= reader.context().options.max_fields)
                    return Err(packed.error(ErrorKind::ResourceLimit));
                ++reader.context().fields;
                auto value = Scalar<Kind>::read(packed);
                if (value.is_err()) return Err(value.unwrap_err());
                values.push(rstd::move(*value));
            }
            return Ok(empty {});
        }
    }
    auto value = read_scalar<Kind>(key, reader);
    if (value.is_err()) return Err(value.unwrap_err());
    values.push(rstd::move(*value));
    return Ok(empty {});
}
template<unsigned Kind>
auto write_packed(u32 number, const Vec<typename Scalar<Kind>::Type>& values, Writer& writer)
    -> Result<empty, Error> {
    static_assert(Scalar<Kind>::wire != WireType::Bytes);
    if (values.is_empty()) return Ok(empty {});
    Writer count(usize(~rstd::size_t(0)));
    for (const auto& value : values) {
        auto result = Scalar<Kind>::write(value, count);
        if (result.is_err()) return result;
    }
    auto result = writer.key({ number, WireType::Bytes });
    if (result.is_err()) return result;
    result = writer.varint(u64(count.written().to_primitive()));
    if (result.is_err()) return result;
    for (const auto& value : values) {
        result = Scalar<Kind>::write(value, writer);
        if (result.is_err()) return result;
    }
    return Ok(empty {});
}
} // namespace liproto
