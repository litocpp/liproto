export module liproto.codegen:schema;
export import :descriptor;
import :names;
import :symbols;

using namespace rstd::prelude;
using rstd::literals::operator""_str;
using ::alloc::string::String;
using ::alloc::vec::Vec;

namespace liproto::codegen
{
export struct GeneratedFile {
    String path;
    String content;
};
export struct ResolvedField {
    String   name;
    unsigned number;
    unsigned kind;
    bool     repeated;
    bool     optional;
    String   message_type;
    String   oneof;
    String   variant;
    unsigned map_key {};
    unsigned map_value {};
    bool     packed {};
};
export struct ResolvedMessage {
    String             name;
    Vec<ResolvedField> fields;
};
export struct ResolvedFile {
    String               path;
    String               module;
    String               cpp_namespace;
    Vec<ResolvedMessage> messages;
    Vec<EnumDescriptor>  enums;
    Vec<String>          imports;
};
export class Schema {
    Vec<ResolvedFile> files_;
    Schema() = default;
    friend auto build_schema(const Request&) -> Result<Schema, String>;

public:
    Schema(Schema&&)                    = default;
    auto operator=(Schema&&) -> Schema& = default;
    auto files() const -> slice<ResolvedFile> { return files_.as_slice(); }
};
export auto build_schema(const Request& request) -> Result<Schema, String> {
    if (! request.parameter.as_str().is_empty())
        return Err(String::make("generator parameters are not supported yet"_str));
    auto graph = DescriptorGraph::build(request);
    if (graph.is_err()) return Err(rstd::move(graph.unwrap_err()));
    Schema schema;
    for (usize root_index {}; root_index < request.roots.len(); ++root_index) {
        auto& root = request.roots[root_index];
        for (usize previous {}; previous < root_index; ++previous)
            if (root == request.roots[previous])
                return Err(String::make("duplicate generation root"_str));
        auto index = graph->file_index(root.as_str());
        if (index.is_none()) return Err(String::make("missing generation root descriptor"_str));
        auto& node = graph->files()[*index];
        auto* file = node.source;
        if (file->syntax != "proto3"_str)
            return Err(String::make("only proto3 business schemas are supported"_str));
        if (file->unsupported)
            return Err(String::make("services, extensions and editions are not supported yet"_str));
        ResolvedFile result;
        result.path          = node.names.output.clone();
        result.module        = node.names.module.clone();
        result.cpp_namespace = node.cpp_namespace.clone();
        for (auto dependency : node.imports)
            result.imports.push(graph->files()[dependency].names.module.clone());
        for (auto& symbol : graph->symbols()) {
            if (symbol.file != *index || symbol.kind != SymbolKind::Enum) continue;
            auto& source = *symbol.enumeration;
            if (source.values.is_empty() || source.values[usize()].number != i32() ||
                source.unsupported)
                return Err(String::make("invalid or unsupported enum"_str));
            EnumDescriptor enumeration;
            enumeration.name = symbol.cpp_name.clone();
            for (auto& item : source.values) {
                if (! identifier(item.name.as_str()))
                    return Err(String::make("invalid enum value"_str));
                for (auto& prior : enumeration.values)
                    if (prior.name == cpp_identifier(item.name.as_str()) ||
                        (! source.allow_alias && prior.number == item.number))
                        return Err(String::make("duplicate enum value"_str));
                enumeration.values.push(
                    EnumValue { cpp_identifier(item.name.as_str()), item.number });
            }
            result.enums.push(rstd::move(enumeration));
        }
        for (auto& symbol : graph->symbols()) {
            if (symbol.file != *index || symbol.kind != SymbolKind::Message) continue;
            auto& source = *symbol.message;
            if (source.map_entry) continue;
            if (! identifier(source.name.as_str()) || symbol.cpp_name.as_str().contains("__"_str))
                return Err(String::make("invalid message name"_str));
            if (source.unsupported)
                return Err(String::make(
                    "nested messages, maps and message options are not supported yet"_str));
            ResolvedMessage message;
            message.name = symbol.cpp_name.clone();
            for (usize i {}; i < source.oneofs.len(); ++i) {
                if (! identifier(source.oneofs[i].as_str()))
                    return Err(String::make("invalid oneof name"_str));
                for (usize j {}; j < i; ++j)
                    if (source.oneofs[i] == source.oneofs[j])
                        return Err(String::make("duplicate oneof name"_str));
                unsigned count     = 0;
                bool     synthetic = false;
                for (auto& field : source.fields)
                    if (field.oneof.is_some() && *field.oneof == i.to_primitive()) {
                        ++count;
                        synthetic |= field.optional;
                    }
                if (! count || (synthetic && count != 1))
                    return Err(String::make("invalid synthetic or empty oneof"_str));
            }
            for (auto& existing : result.messages)
                if (existing.name == message.name)
                    return Err(String::make("duplicate message name"_str));
            for (auto& source_field : source.fields) {
                if (! identifier(source_field.name.as_str()) || source_field.number == 0 ||
                    source_field.number > 0x1fffffff ||
                    (source_field.number >= 19000 && source_field.number <= 19999))
                    return Err(String::make("invalid field name or number"_str));
                if (source_field.unsupported || source_field.type == 10 || source_field.type < 1 ||
                    source_field.type > 18)
                    return Err(String::make("unsupported field type or options"_str));
                if (source_field.label != 1 && source_field.label != 3)
                    return Err(String::make("invalid proto3 field label"_str));
                if (source_field.oneof.is_some() &&
                    (source_field.label != 1 ||
                     *source_field.oneof >= source.oneofs.len().to_primitive()))
                    return Err(String::make("invalid oneof descriptor"_str));
                if (source_field.optional &&
                    (source_field.label != 1 || source_field.oneof.is_none() ||
                     *source_field.oneof >= source.oneofs.len().to_primitive()))
                    return Err(String::make("invalid proto3 optional descriptor"_str));
                if (source_field.packed.is_some() &&
                    (source_field.label != 3 || source_field.type == 9 || source_field.type == 12))
                    return Err(String::make("packed requires a repeated numeric field"_str));
                ResolvedField field { String::make("f_"_str),
                                      source_field.number,
                                      source_field.type,
                                      source_field.label == 3,
                                      source_field.optional };
                field.packed = field.repeated && field.kind != 9 && field.kind != 12 &&
                               field.kind != 11 &&
                               (source_field.packed.is_none() || *source_field.packed);
                if (source_field.type == 14) {
                    auto enumeration =
                        graph->resolve(*index, source_field.type_name.as_str(), SymbolKind::Enum);
                    if (enumeration.is_err()) return Err(rstd::move(enumeration.unwrap_err()));
                }
                if (source_field.type == 11) {
                    auto target = graph->resolve(
                        *index, source_field.type_name.as_str(), SymbolKind::Message);
                    if (target.is_err()) return Err(rstd::move(target.unwrap_err()));
                    const MessageDescriptor* map =
                        (*target)->message->map_entry ? (*target)->message : nullptr;
                    if (map) {
                        bool owned = false;
                        for (auto& nested : source.nested)
                            if (&nested == map) owned = true;
                        if (source_field.label != 3 || source_field.oneof.is_some() ||
                            map->fields.len() != usize(2) || map->unsupported || ! owned)
                            return Err(String::make("invalid map field"_str));
                        const Field* key   = nullptr;
                        const Field* value = nullptr;
                        for (auto& part : map->fields) {
                            if (part.number == 1) key = &part;
                            if (part.number == 2) value = &part;
                        }
                        if (! key || ! value || key->label != 1 || value->label != 1 ||
                            key->unsupported || value->unsupported)
                            return Err(String::make("invalid map entry"_str));
                        if (key->type == 1 || key->type == 2 || key->type == 10 ||
                            key->type == 11 || key->type == 12 || key->type == 14 ||
                            key->type < 1 || key->type > 18 || value->type < 1 ||
                            value->type > 18 || value->type == 10)
                            return Err(String::make("invalid map key or value type"_str));
                        field.map_key   = key->type;
                        field.map_value = value->type;
                        field.repeated  = false;
                        field.optional  = false;
                        if (value->type == 11 || value->type == 14) {
                            auto resolved = graph->resolve(*index,
                                                           value->type_name.as_str(),
                                                           value->type == 11 ? SymbolKind::Message
                                                                             : SymbolKind::Enum);
                            if (resolved.is_err()) return Err(rstd::move(resolved.unwrap_err()));
                            if (value->type == 11 && (*resolved)->message->map_entry)
                                return Err(String::make("map value cannot be a map entry"_str));
                            field.message_type = (*resolved)->cpp_type.clone();
                        }
                    } else {
                        field.message_type = (*target)->cpp_type.clone();
                        field.optional     = ! field.repeated;
                    }
                }
                field.name.push_str(cpp_identifier(source_field.name.as_str()).as_str());
                if (source_field.oneof.is_some() && ! source_field.optional) {
                    auto name = source.oneofs[usize(*source_field.oneof)].as_str();
                    if (! identifier(name)) return Err(String::make("invalid oneof name"_str));
                    field.oneof = String::make("o_"_str);
                    field.oneof.push_str(cpp_identifier(name).as_str());
                    field.variant = variant_identifier(source_field.name.as_str());
                }
                for (auto& existing : message.fields)
                    if (existing.name == field.name || existing.number == field.number ||
                        (! field.oneof.as_str().is_empty() && existing.oneof == field.oneof &&
                         existing.variant == field.variant))
                        return Err(String::make("duplicate field name or number"_str));
                message.fields.push(rstd::move(field));
            }
            result.messages.push(rstd::move(message));
        }
        for (auto& prior : schema.files_)
            if (prior.module == result.module || prior.path == result.path)
                return Err(String::make("generated module collision"_str));
        schema.files_.push(rstd::move(result));
    }
    return Ok(rstd::move(schema));
}
} // namespace liproto::codegen
