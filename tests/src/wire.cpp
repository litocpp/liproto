#include <rstd/test/gtest.hpp>
import liproto;
using namespace rstd::prelude;
using ::alloc::vec::Vec;
using namespace liproto;
using rstd::literals::operator""_bytes;

struct Item {
    u32 count {};
};
namespace rstd
{
template<>
struct Impl<liproto::Message, Item> {
    static auto merge_field(Item& value, FieldKey key, Reader& reader) -> Result<empty, Error> {
        if (key.number != u32(1)) return reader.skip(key);
        auto field = read_scalar<13>(key, reader);
        if (field.is_err()) return Err(field.unwrap_err());
        value.count = *field;
        return Ok(empty {});
    }
    static auto encode_fields(const Item& value, Writer& writer) -> Result<empty, Error> {
        return value.count == u32() ? Result<empty, Error>(Ok(empty {}))
                                    : write_scalar<13>(u32(1), value.count, writer);
    }
};
} // namespace rstd

TEST(Wire, VarintsAndZigzag) {
    auto   data = Vec<u8>::make();
    Writer writer(data);
    ASSERT_TRUE(writer.varint(u64(150)).is_ok());
    ASSERT_TRUE(writer.varint(u64(0xffffffffffffffffULL)).is_ok());
    DecodeContext context;
    Reader        reader(data.as_slice(), context);
    EXPECT_EQ(reader.varint().unwrap(), u64(150));
    EXPECT_EQ(reader.varint().unwrap(), u64(0xffffffffffffffffULL));
    EXPECT_TRUE(reader.empty());
    EXPECT_EQ(unzigzag(zigzag(i64(-1))), i64(-1));
    EXPECT_EQ(unzigzag(u64(0xffffffffffffffffULL)), i64(-9223372036854775807LL - 1));
}

TEST(Wire, RejectsMalformedInput) {
    rstd::byte    invalid[] { rstd::byte(0x80) };
    DecodeContext context;
    Reader        reader(slice<u8>::from_raw_parts(invalid, usize(1)), context);
    EXPECT_EQ(reader.varint().unwrap_err().kind, ErrorKind::Truncated);
    rstd::byte zero[] { rstd::byte(0) };
    Reader     key(slice<u8>::from_raw_parts(zero, usize(1)), context);
    EXPECT_EQ(key.key().unwrap_err().kind, ErrorKind::InvalidKey);
}

TEST(Wire, BoundsBudgetsAndGroups) {
    DecodeContext context;
    Reader        invalid("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x02"_bytes, context);
    EXPECT_EQ(invalid.varint().unwrap_err().kind, ErrorKind::InvalidVarint);
    Reader length("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"_bytes, context);
    EXPECT_EQ(length.bytes().unwrap_err().kind, ErrorKind::Truncated);
    Reader group("\x0b\x10\x01\x0c\x08\x01"_bytes, context);
    ASSERT_TRUE(group.skip(group.key().unwrap()).is_ok());
    EXPECT_EQ(group.offset(), usize(4));
    Reader mismatch("\x0b\x14"_bytes, context);
    EXPECT_EQ(mismatch.skip(mismatch.key().unwrap()).unwrap_err().kind,
              ErrorKind::UnexpectedEndGroup);
    DecodeContext bounded(DecodeOptions { usize(64), usize(1), 0 });
    Reader        deep("\x0b\x0c"_bytes, bounded);
    EXPECT_EQ(deep.skip(deep.key().unwrap()).unwrap_err().kind, ErrorKind::DepthLimit);
    Reader more("\x08\x01"_bytes, bounded);
    EXPECT_EQ(more.key().unwrap_err().kind, ErrorKind::ResourceLimit);
}

TEST(Wire, TextAndPackedBoundaries) {
    DecodeContext context;
    Reader        invalid("\x01\xff"_bytes, context);
    EXPECT_EQ(invalid.text().unwrap_err().kind, ErrorKind::InvalidUtf8);
    Reader   packed("\x03\x01\x96\x01"_bytes, context, usize(20));
    Vec<u32> values;
    ASSERT_TRUE(read_repeated<13>(values, { u32(1), WireType::Bytes }, packed).is_ok());
    ASSERT_EQ(values.len(), usize(2));
    EXPECT_EQ(values[usize(1)], u32(150));
    Reader truncated("\x01\x80"_bytes, context, usize(20));
    auto   failed = read_repeated<13>(values, { u32(1), WireType::Bytes }, truncated);
    ASSERT_TRUE(failed.is_err());
    EXPECT_EQ(failed.unwrap_err().offset, usize(22));
    DecodeContext limited(DecodeOptions { usize(64), usize(1), 100 });
    Reader        repeated("\x02\x01\x02"_bytes, limited);
    EXPECT_EQ(read_repeated<13>(values, { u32(1), WireType::Bytes }, repeated).unwrap_err().kind,
              ErrorKind::ResourceLimit);
}

TEST(Wire, FixedAndSignedTypes) {
    Vec<u8> bytes;
    Writer  writer(bytes);
    ASSERT_TRUE(Scalar<5>::write(i32(-1), writer).is_ok());
    EXPECT_EQ(bytes.len(), usize(10));
    ASSERT_TRUE(Scalar<1>::write(f64(1.5), writer).is_ok());
    ASSERT_TRUE(Scalar<15>::write(i32(-123), writer).is_ok());
    DecodeContext context;
    Reader        reader(bytes.as_slice(), context);
    EXPECT_EQ(Scalar<5>::read(reader).unwrap(), i32(-1));
    EXPECT_EQ(Scalar<1>::read(reader).unwrap(), f64(1.5));
    EXPECT_EQ(Scalar<15>::read(reader).unwrap(), i32(-123));
}

TEST(Message, EncodeDecodeMergeAndDelimited) {
    auto value = decode<Item>("\x08\x01\x08\x96\x01"_bytes).unwrap();
    EXPECT_EQ(value.count, u32(150));
    auto length = encoded_len(value).unwrap();
    EXPECT_EQ(length, usize(3));
    auto bytes = encode_to_vec(value).unwrap();
    EXPECT_EQ(bytes.len(), length);
    EXPECT_EQ(decode<Item>(bytes.as_slice()).unwrap().count, value.count);
    ASSERT_TRUE(merge(value, "\x10\x02"_bytes).is_ok());
    EXPECT_EQ(value.count, u32(150));
    rstd::byte small[2] {};
    Writer     bounded(mut_ref<u8[]>::from_raw_parts(small, usize(2)));
    EXPECT_EQ(encode(value, bounded).unwrap_err().kind, ErrorKind::Capacity);
    EXPECT_EQ(bounded.written(), usize());
    Vec<u8> framed;
    Writer  writer(framed);
    ASSERT_TRUE(encode_message(value, writer).is_ok());
    ASSERT_TRUE(encode_message(Item { u32(2) }, writer).is_ok());
    DecodeContext context;
    Reader        reader(framed.as_slice(), context);
    EXPECT_EQ(decode_length_delimited<Item>(reader).unwrap().count, u32(150));
    EXPECT_EQ(decode_length_delimited<Item>(reader).unwrap().count, u32(2));
    EXPECT_TRUE(reader.empty());
}
