export module liproto:wire;
export import rstd.core;
export import rstd.alloc;
import rstd.parse.core;

using namespace rstd::prelude;
using ::alloc::vec::Vec;

export namespace liproto
{

enum class WireType : unsigned char
{
    Varint     = 0,
    Fixed64    = 1,
    Bytes      = 2,
    StartGroup = 3,
    EndGroup   = 4,
    Fixed32    = 5
};
struct FieldKey {
    u32      number;
    WireType wire;
};
enum class ErrorKind
{
    Truncated,
    InvalidVarint,
    InvalidKey,
    WrongWire,
    InvalidUtf8,
    DepthLimit,
    ResourceLimit,
    UnexpectedEndGroup,
    SizeOverflow,
    Capacity
};
struct Error {
    ErrorKind kind;
    usize     offset {};
    u32       field {};
};
struct DecodeOptions {
    usize    max_bytes { 64 * 1024 * 1024 };
    usize    max_fields { 1000000 };
    unsigned max_depth { 100 };
};
struct DecodeContext {
    DecodeOptions options;
    usize         fields {};
    explicit DecodeContext(DecodeOptions options = {}): options(options) {}
};

class Reader {
    rstd::parse::TextCursor cursor_;
    DecodeContext&          context_;
    usize                   base_;
    unsigned                depth_;

public:
    Reader(slice<u8> bytes, DecodeContext& context, usize base = {}, unsigned depth = 0)
        : cursor_(rstd::parse::TextInput(bytes)), context_(context), base_(base), depth_(depth) {}
    auto offset() const -> usize { return base_ + cursor_.position(); }
    auto remaining() const -> usize { return cursor_.remaining(); }
    auto empty() const -> bool { return cursor_.is_eof(); }
    auto context() -> DecodeContext& { return context_; }
    auto depth() const -> unsigned { return depth_; }
    auto error(ErrorKind kind, u32 field = {}) const -> Error { return { kind, offset(), field }; }
    auto validate() const -> Result<rstd::empty, Error> {
        if (cursor_.len() > context_.options.max_bytes) return Err(error(ErrorKind::ResourceLimit));
        if (depth_ > context_.options.max_depth) return Err(error(ErrorKind::DepthLimit));
        return Ok(rstd::empty {});
    }
    auto varint() -> Result<u64, Error> {
        auto valid = validate();
        if (valid.is_err()) return Err(valid.unwrap_err());
        unsigned long long value = 0;
        for (unsigned i = 0; i < 10; ++i) {
            if (empty()) return Err(error(ErrorKind::Truncated));
            auto byte = cursor_.remaining_input()[usize()].to_primitive();
            (void)cursor_.advance(usize(1));
            if (i == 9 && byte > 1) return Err(error(ErrorKind::InvalidVarint));
            value |= static_cast<unsigned long long>(byte & 127) << (i * 7);
            if (! (byte & 128)) return Ok(u64(value));
        }
        return Err(error(ErrorKind::InvalidVarint));
    }
    auto key() -> Result<FieldKey, Error> {
        if (context_.fields >= context_.options.max_fields)
            return Err(error(ErrorKind::ResourceLimit));
        auto raw = varint();
        if (raw.is_err()) return Err(raw.unwrap_err());
        auto value = raw->to_primitive();
        if (value > 0xffffffffULL || (value >> 3) == 0 || (value & 7) > 5)
            return Err(error(ErrorKind::InvalidKey));
        ++context_.fields;
        return Ok(FieldKey { u32(value >> 3), static_cast<WireType>(value & 7) });
    }
    auto take(usize length) -> Result<slice<u8>, Error> {
        auto valid = validate();
        if (valid.is_err()) return Err(valid.unwrap_err());
        if (length > remaining()) return Err(error(ErrorKind::Truncated));
        auto start = cursor_.position();
        (void)cursor_.advance(length);
        return Ok(cursor_.view({ start, cursor_.position() }));
    }
    auto bytes() -> Result<slice<u8>, Error> {
        auto length = varint();
        if (length.is_err()) return Err(length.unwrap_err());
        if (length->to_primitive() > remaining().to_primitive())
            return Err(error(ErrorKind::Truncated));
        return take(usize(length->to_primitive()));
    }
    auto text() -> Result<ref<str>, Error> {
        auto value = bytes();
        if (value.is_err()) return Err(value.unwrap_err());
        auto checked = rstd::str_::from_utf8(*value);
        if (checked.is_err())
            return Err(Error { ErrorKind::InvalidUtf8,
                               offset() - value->len() + checked.unwrap_err().valid_up_to() });
        return Ok(*checked);
    }
    auto fixed(unsigned count) -> Result<u64, Error> {
        if (count != 4 && count != 8) return Err(error(ErrorKind::WrongWire));
        auto bytes = take(usize(count));
        if (bytes.is_err()) return Err(bytes.unwrap_err());
        unsigned long long value = 0;
        for (unsigned i = 0; i < count; ++i)
            value |= static_cast<unsigned long long>((*bytes)[usize(i)].to_primitive()) << (i * 8);
        return Ok(u64(value));
    }
    auto skip(FieldKey key) -> Result<rstd::empty, Error> {
        if (key.wire == WireType::Varint) {
            auto value = varint();
            if (value.is_err()) return Err(value.unwrap_err());
        } else if (key.wire == WireType::Bytes) {
            auto value = bytes();
            if (value.is_err()) return Err(value.unwrap_err());
        } else if (key.wire == WireType::Fixed32 || key.wire == WireType::Fixed64) {
            auto value = fixed(key.wire == WireType::Fixed32 ? 4 : 8);
            if (value.is_err()) return Err(value.unwrap_err());
        } else if (key.wire == WireType::StartGroup) {
            if (depth_ >= context_.options.max_depth) return Err(error(ErrorKind::DepthLimit));
            Reader group(cursor_.remaining_input(), context_, offset(), depth_ + 1);
            while (! group.empty()) {
                auto inner = group.key();
                if (inner.is_err()) return Err(inner.unwrap_err());
                if (inner->wire == WireType::EndGroup) {
                    if (inner->number != key.number)
                        return Err(group.error(ErrorKind::UnexpectedEndGroup));
                    (void)cursor_.advance(group.offset() - offset());
                    return Ok(rstd::empty {});
                }
                auto skipped = group.skip(*inner);
                if (skipped.is_err()) return Err(skipped.unwrap_err());
            }
            return Err(group.error(ErrorKind::Truncated));
        } else
            return Err(error(ErrorKind::UnexpectedEndGroup));
        return Ok(rstd::empty {});
    }
};

constexpr auto varint_len(u64 value) -> usize {
    auto  raw = value.to_primitive();
    usize count(1);
    while (raw >= 128) {
        raw >>= 7;
        ++count;
    }
    return count;
}
constexpr auto zigzag(i64 value) -> u64 {
    auto bits = static_cast<unsigned long long>(value.to_primitive());
    return u64((bits << 1) ^ (0ULL - (bits >> 63)));
}
constexpr auto unzigzag(u64 value) -> i64 {
    auto bits      = value.to_primitive();
    auto magnitude = static_cast<long long>(bits >> 1);
    return i64((bits & 1) ? -magnitude - 1 : magnitude);
}

class Writer {
    Vec<u8>*      output_ {};
    mut_ref<u8[]> buffer_ {};
    usize         limit_;
    usize         written_ {};

public:
    explicit Writer(Vec<u8>& output, usize limit = usize(64 * 1024 * 1024))
        : output_(&output), limit_(limit), written_(output.len()) {}
    explicit Writer(mut_ref<u8[]> buffer): buffer_(buffer), limit_(buffer.len()) {}
    explicit Writer(usize limit): limit_(limit) {}
    auto written() const -> usize { return written_; }
    auto available() const -> usize { return written_ <= limit_ ? limit_ - written_ : usize(); }
    auto raw(slice<u8> bytes) -> Result<rstd::empty, Error> {
        if (written_ > limit_ || bytes.len() > available())
            return Err(Error { ErrorKind::Capacity, written_ });
        if (output_)
            output_->extend_from_slice(bytes);
        else if (! buffer_.is_empty()) {
            for (usize i {}; i < bytes.len(); ++i) buffer_[written_ + i] = bytes[i];
        }
        written_ += bytes.len();
        return Ok(rstd::empty {});
    }
    auto varint(u64 value) -> Result<rstd::empty, Error> {
        rstd::byte buffer[10];
        unsigned   length = 0;
        auto       bits   = value.to_primitive();
        do {
            auto byte = static_cast<unsigned char>(bits & 127);
            bits >>= 7;
            buffer[length++] = static_cast<rstd::byte>(byte | (bits ? 128 : 0));
        } while (bits);
        return raw(slice<u8>::from_raw_parts(buffer, usize(length)));
    }
    auto key(FieldKey key) -> Result<rstd::empty, Error> {
        if (key.number == u32() || key.number > u32(0x1fffffff) ||
            static_cast<unsigned>(key.wire) > 5)
            return Err(Error { ErrorKind::InvalidKey, written_ });
        return varint(u64((static_cast<unsigned long long>(key.number.to_primitive()) << 3) |
                          static_cast<unsigned>(key.wire)));
    }
    auto bytes(slice<u8> value) -> Result<rstd::empty, Error> {
        auto prefix_size = varint_len(u64(value.len().to_primitive()));
        if (prefix_size > available() || value.len() > available() - prefix_size)
            return Err(Error { ErrorKind::Capacity, written_ });
        auto prefix = varint(u64(value.len().to_primitive()));
        if (prefix.is_err()) return prefix;
        return raw(value);
    }
    auto fixed(u64 value, unsigned count) -> Result<rstd::empty, Error> {
        if (count != 4 && count != 8) return Err(Error { ErrorKind::WrongWire, written_ });
        rstd::byte buffer[8];
        for (unsigned i = 0; i < count; ++i)
            buffer[i] = static_cast<rstd::byte>((value.to_primitive() >> (i * 8)) & 255);
        return raw(slice<u8>::from_raw_parts(buffer, usize(count)));
    }
};
} // namespace liproto
