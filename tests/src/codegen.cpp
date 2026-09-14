#include <rstd/test/gtest.hpp>
import liproto.codegen;
using namespace rstd::prelude;
using rstd::literals::operator""_str;
using rstd::literals::operator""_bytes;
using ::alloc::string::String;
using ::alloc::vec::Vec;
using namespace liproto;
using namespace liproto::codegen;

TEST(Codegen, ValidatesSchemaBeforeEmission) {
    Request request;
    request.roots.push(String::make("demo.proto"_str));
    FileDescriptor file;
    file.name   = String::make("demo.proto"_str);
    file.syntax = String::make("proto3"_str);
    MessageDescriptor message;
    message.name = String::make("Item"_str);
    Field field;
    field.name   = String::make("count"_str);
    field.number = 1;
    field.type   = 13;
    field.label  = 1;
    message.fields.push(rstd::move(field));
    file.messages.push(rstd::move(message));
    request.files.push(rstd::move(file));
    auto schema = build_schema(request);
    ASSERT_TRUE(schema.is_ok());
    auto files = generate(*schema);
    ASSERT_EQ(files.len(), usize(1));
    EXPECT_EQ(files[usize()].path, "demo.cppm"_str);
    EXPECT_TRUE(encode_response(files.as_slice()).is_ok());
    request.files[usize()].messages[usize()].fields[usize()].number = 19000;
    EXPECT_TRUE(build_schema(request).is_err());
}

TEST(Codegen, RejectsInvalidRequestAndTraversal) {
    EXPECT_TRUE(decode_request("\x7a\xff"_bytes).is_err());
    Request request;
    request.roots.push(String::make("../demo.proto"_str));
    FileDescriptor file;
    file.name   = String::make("../demo.proto"_str);
    file.syntax = String::make("proto3"_str);
    request.files.push(rstd::move(file));
    EXPECT_TRUE(build_schema(request).is_err());
}

TEST(Codegen, PublicImportsAndUnifiedSymbols) {
    Request request;
    request.roots.push(String::make("main.proto"_str));
    ref<str> names[] = { "main.proto"_str, "bridge.proto"_str, "types.proto"_str };
    for (auto name : names) {
        FileDescriptor file;
        file.name   = String::make(name);
        file.syntax = String::make("proto3"_str);
        request.files.push(rstd::move(file));
    }
    request.files[usize(0)].imports.push(String::make("bridge.proto"_str));
    request.files[usize(1)].imports.push(String::make("types.proto"_str));
    request.files[usize(1)].public_imports.push(u32(0));
    MessageDescriptor owner;
    owner.name = String::make("Owner"_str);
    EnumDescriptor enumeration;
    enumeration.name = String::make("Kind"_str);
    enumeration.values.push(EnumValue { String::make("ZERO"_str), i32(0) });
    owner.enums.push(rstd::move(enumeration));
    request.files[usize(2)].messages.push(rstd::move(owner));
    MessageDescriptor consumer;
    consumer.name = String::make("Consumer"_str);
    Field field;
    field.name      = String::make("kind"_str);
    field.type      = 14;
    field.type_name = String::make(".Owner.Kind"_str);
    field.number    = 1;
    field.label     = 1;
    consumer.fields.push(rstd::move(field));
    request.files[usize(0)].messages.push(rstd::move(consumer));
    EXPECT_TRUE(build_schema(request).is_ok());
    request.files[usize(1)].public_imports.clear();
    EXPECT_TRUE(build_schema(request).is_err());
    request.files[usize(1)].public_imports.push(u32(1));
    EXPECT_TRUE(build_schema(request).is_err());
    request.files[usize(1)].public_imports[usize()]                = u32(0);
    request.files[usize(0)].messages[usize()].fields[usize()].type = 11;
    EXPECT_TRUE(build_schema(request).is_err());
}

TEST(Codegen, NestedEnumsAliasesAndPortableNames) {
    Request request;
    request.roots.push(String::make("dir/Case.proto"_str));
    FileDescriptor file;
    file.name    = String::make("dir/Case.proto"_str);
    file.syntax  = String::make("proto3"_str);
    file.package = String::make("class._private"_str);
    MessageDescriptor message;
    message.name = String::make("_Item"_str);
    EnumDescriptor enumeration;
    enumeration.name = String::make("Kind"_str);
    enumeration.values.push(EnumValue { String::make("ZERO"_str), i32(0) });
    enumeration.values.push(EnumValue { String::make("ALIAS"_str), i32(0) });
    message.enums.push(rstd::move(enumeration));
    file.messages.push(rstd::move(message));
    request.files.push(rstd::move(file));
    EXPECT_TRUE(build_schema(request).is_err());
    request.files[usize()].messages[usize()].enums[usize()].allow_alias = true;
    auto schema                                                         = build_schema(request);
    ASSERT_TRUE(schema.is_ok());
    EXPECT_EQ(schema->files()[usize()].path, "dir/z43ase.cppm"_str);
    EXPECT_EQ(schema->files()[usize()].cpp_namespace, "p_class::p_z5fprivate"_str);
    EXPECT_EQ(schema->files()[usize()].enums[usize()].name, "E_z5fItem_n_Kind"_str);
    EXPECT_EQ(schema->files()[usize()].messages[usize()].name, "M_z5fItem"_str);
}

TEST(Codegen, PublicDependencyWireForms) {
    slice<u8> encodings[] = { "\x0a\x04\x50\x00\x50\x01"_bytes, "\x0a\x04\x52\x02\x00\x01"_bytes };
    for (auto bytes : encodings) {
        auto decoded = decode_descriptor_set(bytes, "demo.proto"_str);
        ASSERT_TRUE(decoded.is_ok());
        ASSERT_EQ(decoded->files.len(), usize(1));
        ASSERT_EQ(decoded->files[usize()].public_imports.len(), usize(2));
        EXPECT_EQ(decoded->files[usize()].public_imports[usize(1)], u32(1));
    }
}

TEST(Codegen, MapEnumValueResolution) {
    Request request;
    request.roots.push(String::make("demo.proto"_str));
    FileDescriptor file;
    file.name   = String::make("demo.proto"_str);
    file.syntax = String::make("proto3"_str);
    MessageDescriptor owner;
    owner.name = String::make("Owner"_str);
    EnumDescriptor enumeration;
    enumeration.name = String::make("Kind"_str);
    enumeration.values.push(EnumValue { String::make("ZERO"_str), i32(0) });
    owner.enums.push(rstd::move(enumeration));
    MessageDescriptor entry;
    entry.name      = String::make("Entry"_str);
    entry.map_entry = true;
    Field key;
    key.name   = String::make("key"_str);
    key.number = 1;
    key.type   = 9;
    key.label  = 1;
    entry.fields.push(rstd::move(key));
    Field value;
    value.name      = String::make("value"_str);
    value.number    = 2;
    value.type      = 14;
    value.label     = 1;
    value.type_name = String::make(".Owner.Kind"_str);
    entry.fields.push(rstd::move(value));
    owner.nested.push(rstd::move(entry));
    Field field;
    field.name      = String::make("states"_str);
    field.number    = 1;
    field.type      = 11;
    field.label     = 3;
    field.type_name = String::make(".Owner.Entry"_str);
    owner.fields.push(rstd::move(field));
    file.messages.push(rstd::move(owner));
    request.files.push(rstd::move(file));
    EXPECT_TRUE(build_schema(request).is_ok());
    auto& type =
        request.files[usize()].messages[usize()].nested[usize()].fields[usize(1)].type_name;
    type = String::make(".Missing"_str);
    EXPECT_TRUE(build_schema(request).is_err());
    type = String::make(".Owner"_str);
    EXPECT_TRUE(build_schema(request).is_err());
}
