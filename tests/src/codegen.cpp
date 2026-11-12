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
    request.roots.push(String::make("dir/Case_z.proto"_str));
    FileDescriptor file;
    file.name    = String::make("dir/Case_z.proto"_str);
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
    EXPECT_EQ(schema->files()[usize()].path, "dir/z43asez5fz7a.cppm"_str);
    EXPECT_EQ(schema->files()[usize()].module, "proto.p_dir.p_z43asez5fz7a"_str);
    EXPECT_EQ(schema->files()[usize()].cpp_namespace, "p_class::p_z5fprivate"_str);
    EXPECT_EQ(schema->files()[usize()].enums[usize()].name, "E_z5fItem_n_Kind"_str);
    EXPECT_EQ(schema->files()[usize()].messages[usize()].name, "M_z5fItem"_str);
    auto files = generate(*schema);
    ASSERT_EQ(files.len(), usize(1));
    EXPECT_EQ(files[usize()].path, "dir/z43asez5fz7a.cppm"_str);
    auto content = files[usize()].content.as_str();
    EXPECT_TRUE(content.contains("export module proto.p_dir.p_z43asez5fz7a;"_str));
    EXPECT_TRUE(content.contains("v_ZERO = 0,"_str));
    EXPECT_TRUE(content.contains("v_ALIAS = 0,"_str));
}

TEST(Codegen, ReadableNamesAndKeywordPrefixes) {
    Request request;
    request.roots.push(String::make("names.proto"_str));
    FileDescriptor file;
    file.name    = String::make("names.proto"_str);
    file.syntax  = String::make("proto3"_str);
    file.package = String::make("class.network_settings"_str);
    EnumDescriptor enumeration;
    enumeration.name = String::make("enum"_str);
    enumeration.values.push(EnumValue { String::make("GET_NETWORK_SETTINGS_RSP"_str), i32(0) });
    enumeration.values.push(EnumValue { String::make("default"_str), i32(1) });
    file.enums.push(rstd::move(enumeration));
    MessageDescriptor message;
    message.name = String::make("class"_str);
    message.oneofs.push(String::make("payload_data"_str));
    message.oneofs.push(String::make("switch"_str));
    ref<str> names[] = { "server_url"_str, "size"_str, "class"_str, "payload"_str, "return"_str };
    for (auto name : names) {
        Field field;
        field.name   = String::make(name);
        field.number = static_cast<unsigned>(message.fields.len().to_primitive()) + 1;
        field.type   = 9;
        field.label  = 1;
        if (field.number >= 4) field.oneof = Some(field.number - 4);
        message.fields.push(rstd::move(field));
    }
    file.messages.push(rstd::move(message));
    request.files.push(rstd::move(file));
    auto schema = build_schema(request);
    ASSERT_TRUE(schema.is_ok());
    ASSERT_EQ(schema->files().len(), usize(1));
    const auto& resolved = schema->files()[usize()];
    EXPECT_EQ(resolved.cpp_namespace, "p_class::p_network_settings"_str);
    ASSERT_EQ(resolved.messages.len(), usize(1));
    EXPECT_EQ(resolved.messages[usize()].name, "M_class"_str);
    const auto& fields = resolved.messages[usize()].fields;
    ASSERT_EQ(fields.len(), usize(5));
    EXPECT_EQ(fields[usize()].name, "f_server_url"_str);
    EXPECT_EQ(fields[usize(1)].name, "f_size"_str);
    EXPECT_EQ(fields[usize(2)].name, "f_class"_str);
    EXPECT_EQ(fields[usize(3)].oneof, "o_payload_data"_str);
    EXPECT_EQ(fields[usize(4)].oneof, "o_switch"_str);
    EXPECT_EQ(fields[usize(3)].variant, "payload"_str);
    EXPECT_EQ(fields[usize(4)].variant, "return_"_str);
    auto files = generate(*schema);
    ASSERT_EQ(files.len(), usize(1));
    auto content = files[usize()].content.as_str();
    EXPECT_TRUE(content.contains("export namespace p_class::p_network_settings {"_str));
    EXPECT_TRUE(content.contains("enum class E_enum : int {"_str));
    EXPECT_TRUE(content.contains("v_GET_NETWORK_SETTINGS_RSP = 0,"_str));
    EXPECT_TRUE(content.contains("v_default = 1,"_str));
    EXPECT_TRUE(content.contains("struct M_class {"_str));
    EXPECT_TRUE(content.contains("f_server_url {};"_str));
    EXPECT_TRUE(content.contains("f_size {};"_str));
    EXPECT_TRUE(content.contains("f_class {};"_str));
    EXPECT_TRUE(content.contains("Option<C_o_payload_data> o_payload_data {};"_str));
    EXPECT_TRUE(content.contains("Option<C_o_switch> o_switch {};"_str));
    EXPECT_TRUE(content.contains("class C_o_payload_data {"_str));
    EXPECT_TRUE(content.contains("payload = 4u,"_str));
    EXPECT_TRUE(content.contains("return_ = 5u,"_str));
    EXPECT_TRUE(content.contains("auto make_return_("_str));
    EXPECT_TRUE(content.contains("auto as_return_() const&&"_str));
    EXPECT_FALSE(content.contains("->template as<"_str));
}

TEST(Codegen, OneofKeywordNamesRemainDistinct) {
    Request request;
    request.roots.push(String::make("oneof.proto"_str));
    FileDescriptor file;
    file.name   = String::make("oneof.proto"_str);
    file.syntax = String::make("proto3"_str);
    MessageDescriptor message;
    message.name = String::make("Names"_str);
    message.oneofs.push(String::make("payload"_str));
    ref<str> names[] = { "class"_str,    "class_"_str,   "classz5f"_str,  "tag"_str,
                         "requires"_str, "_private"_str, "two__words"_str };
    for (auto name : names) {
        Field field;
        field.name   = String::make(name);
        field.number = static_cast<unsigned>(message.fields.len().to_primitive()) + 1;
        field.type   = 13;
        field.label  = 1;
        field.oneof  = Some(0u);
        message.fields.push(rstd::move(field));
    }
    file.messages.push(rstd::move(message));
    request.files.push(rstd::move(file));
    auto schema = build_schema(request);
    ASSERT_TRUE(schema.is_ok());
    const auto& fields = schema->files()[usize()].messages[usize()].fields;
    EXPECT_EQ(fields[usize()].variant, "class_"_str);
    EXPECT_EQ(fields[usize(1)].variant, "classz5f"_str);
    EXPECT_EQ(fields[usize(2)].variant, "classz7a5f"_str);
    EXPECT_EQ(fields[usize(3)].variant, "tag"_str);
    EXPECT_EQ(fields[usize(4)].variant, "requires_"_str);
    EXPECT_EQ(fields[usize(5)].variant, "z5fprivate"_str);
    EXPECT_EQ(fields[usize(6)].variant, "twoz5fz5fwords"_str);
}

TEST(Codegen, LiteralZEscapesOnlyEscapeSequences) {
    struct Name {
        ref<str> proto;
        ref<str> cpp;
    };
    const Name names[] = { { "z"_str, "f_z"_str },         { "z5"_str, "f_z5"_str },
                           { "z5F"_str, "f_z5F"_str },     { "zebra"_str, "f_zebra"_str },
                           { "sized"_str, "f_sized"_str }, { "zab"_str, "f_zab"_str },
                           { "z5f"_str, "f_z7a5f"_str },   { "z7a"_str, "f_z7a7a"_str } };
    Request    request;
    request.roots.push(String::make("names.proto"_str));
    FileDescriptor file;
    file.name   = String::make("names.proto"_str);
    file.syntax = String::make("proto3"_str);
    MessageDescriptor message;
    message.name = String::make("Names"_str);
    for (const auto& name : names) {
        Field field;
        field.name   = String::make(name.proto);
        field.number = static_cast<unsigned>(message.fields.len().to_primitive()) + 1;
        field.type   = 13;
        field.label  = 1;
        message.fields.push(rstd::move(field));
    }
    file.messages.push(rstd::move(message));
    request.files.push(rstd::move(file));
    auto schema = build_schema(request);
    ASSERT_TRUE(schema.is_ok());
    const auto& fields = schema->files()[usize()].messages[usize()].fields;
    ASSERT_EQ(fields.len(), usize(8));
    auto files = generate(*schema);
    ASSERT_EQ(files.len(), usize(1));
    usize index {};
    for (const auto& name : names) {
        EXPECT_EQ(fields[index++].name, name.cpp);
        auto declaration = String::make(name.cpp);
        declaration.push_str(" {};"_str);
        EXPECT_TRUE(files[usize()].content.as_str().contains(declaration.as_str()));
    }
}

TEST(Codegen, ReservedUnderscoresRemainDistinctFromLiteralEscapes) {
    struct Name {
        ref<str> proto;
        ref<str> cpp;
    };
    const Name names[] = { { "_value"_str, "f_z5fvalue"_str },
                           { "value_"_str, "f_valuez5f"_str },
                           { "value__part"_str, "f_valuez5fz5fpart"_str },
                           { "z5fvalue"_str, "f_z7a5fvalue"_str },
                           { "value_n_part"_str, "f_valuez5fn_part"_str },
                           { "value_part"_str, "f_value_part"_str } };
    Request    request;
    request.roots.push(String::make("names.proto"_str));
    FileDescriptor file;
    file.name   = String::make("names.proto"_str);
    file.syntax = String::make("proto3"_str);
    MessageDescriptor message;
    message.name = String::make("Names"_str);
    for (const auto& name : names) {
        Field field;
        field.name   = String::make(name.proto);
        field.number = static_cast<unsigned>(message.fields.len().to_primitive()) + 1;
        field.type   = 13;
        field.label  = 1;
        message.fields.push(rstd::move(field));
    }
    file.messages.push(rstd::move(message));
    request.files.push(rstd::move(file));
    auto schema = build_schema(request);
    ASSERT_TRUE(schema.is_ok());
    const auto& fields = schema->files()[usize()].messages[usize()].fields;
    ASSERT_EQ(fields.len(), usize(6));
    auto files = generate(*schema);
    ASSERT_EQ(files.len(), usize(1));
    usize index {};
    for (const auto& name : names) {
        EXPECT_EQ(fields[index].name, name.cpp);
        EXPECT_FALSE(fields[index++].name.as_str().contains("__"_str));
        auto declaration = String::make(name.cpp);
        declaration.push_str(" {};"_str);
        EXPECT_TRUE(files[usize()].content.as_str().contains(declaration.as_str()));
    }
}

TEST(Codegen, NestedAndLiteralTypeNamesDoNotCollide) {
    Request request;
    request.roots.push(String::make("names.proto"_str));
    FileDescriptor file;
    file.name    = String::make("names.proto"_str);
    file.syntax  = String::make("proto3"_str);
    file.package = String::make("demo"_str);
    MessageDescriptor parent;
    parent.name = String::make("A"_str);
    MessageDescriptor child;
    child.name = String::make("B"_str);
    parent.nested.push(rstd::move(child));
    ref<str> types[] = { ".demo.A.B"_str, ".demo.A_n_B"_str };
    for (auto type : types) {
        Field field;
        field.number    = static_cast<unsigned>(parent.fields.len().to_primitive()) + 1;
        field.name      = String::make(field.number == 1 ? "nested"_str : "literal"_str);
        field.type      = 11;
        field.label     = 1;
        field.type_name = String::make(type);
        parent.fields.push(rstd::move(field));
    }
    file.messages.push(rstd::move(parent));
    MessageDescriptor literal;
    literal.name = String::make("A_n_B"_str);
    file.messages.push(rstd::move(literal));
    request.files.push(rstd::move(file));
    auto schema = build_schema(request);
    ASSERT_TRUE(schema.is_ok());
    const auto& messages = schema->files()[usize()].messages;
    ASSERT_EQ(messages.len(), usize(3));
    EXPECT_EQ(messages[usize(1)].name, "M_A_n_B"_str);
    EXPECT_EQ(messages[usize(2)].name, "M_Az5fn_B"_str);
    ASSERT_EQ(messages[usize()].fields.len(), usize(2));
    EXPECT_EQ(messages[usize()].fields[usize()].message_type, "::p_demo::M_A_n_B"_str);
    EXPECT_EQ(messages[usize()].fields[usize(1)].message_type, "::p_demo::M_Az5fn_B"_str);
    auto files = generate(*schema);
    ASSERT_EQ(files.len(), usize(1));
    auto content = files[usize()].content.as_str();
    EXPECT_TRUE(content.contains("struct M_A_n_B {"_str));
    EXPECT_TRUE(content.contains("struct M_Az5fn_B {"_str));
    EXPECT_TRUE(content.contains("::p_demo::M_A_n_B>"_str));
    EXPECT_TRUE(content.contains("::p_demo::M_Az5fn_B>"_str));
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
