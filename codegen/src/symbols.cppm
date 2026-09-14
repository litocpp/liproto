module liproto.codegen:symbols;
import :descriptor;
import :names;

using namespace rstd::prelude;
using rstd::literals::operator""_str;
using ::alloc::string::String;
using ::alloc::vec::Vec;

namespace liproto::codegen
{
enum class SymbolKind
{
    Message,
    Enum
};
struct Symbol {
    usize                    file;
    SymbolKind               kind;
    const MessageDescriptor* message {};
    const EnumDescriptor*    enumeration {};
    String                   proto_name;
    String                   cpp_name;
    String                   cpp_type;
};
struct FileNode {
    const FileDescriptor* source;
    FileNames             names;
    String                cpp_namespace;
    Vec<usize>            imports;
    Vec<usize>            public_imports;
    Vec<usize>            visible;
};
auto contains(const Vec<usize>& values, usize value) -> bool {
    for (auto existing : values)
        if (existing == value) return true;
    return false;
}
void include(Vec<usize>& values, usize value) {
    if (! contains(values, value)) values.push(usize(value));
}

class DescriptorGraph {
    Vec<FileNode> files_;
    Vec<Symbol>   symbols_;

    auto add_symbol(usize                    file,
                    SymbolKind               kind,
                    const MessageDescriptor* message,
                    const EnumDescriptor*    enumeration,
                    ref<str>                 prefix,
                    ref<str>                 cpp_prefix,
                    ref<str>                 name) -> Result<empty, String> {
        if (! identifier(name)) return Err(String::make("invalid proto type name"_str));
        Symbol symbol { file, kind, message, enumeration };
        symbol.proto_name = String::make(prefix);
        symbol.proto_name.push_str(name);
        symbol.cpp_name = String::make(kind == SymbolKind::Message ? "M_"_str : "E_"_str);
        symbol.cpp_name.push_str(cpp_prefix);
        symbol.cpp_name.push_str(encoded_name(name).as_str());
        symbol.cpp_type = String::make("::"_str);
        symbol.cpp_type.push_str(files_[file].cpp_namespace.as_str());
        symbol.cpp_type.push_str("::"_str);
        symbol.cpp_type.push_str(symbol.cpp_name.as_str());
        for (auto& prior : symbols_)
            if (prior.proto_name == symbol.proto_name || prior.cpp_type == symbol.cpp_type)
                return Err(String::make("duplicate proto symbol or generated type"_str));
        symbols_.push(rstd::move(symbol));
        return Ok(empty {});
    }
    auto add_message(usize                    file,
                     const MessageDescriptor& message,
                     ref<str>                 prefix,
                     ref<str>                 cpp_prefix,
                     unsigned                 depth) -> Result<empty, String> {
        if (depth > 100) return Err(String::make("schema nesting limit exceeded"_str));
        auto result = add_symbol(file,
                                 SymbolKind::Message,
                                 &message,
                                 nullptr,
                                 prefix,
                                 cpp_prefix,
                                 message.name.as_str());
        if (result.is_err()) return result;
        String nested = String::make(prefix);
        nested.push_str(message.name.as_str());
        nested.push_ascii('.');
        String cpp = String::make(cpp_prefix);
        cpp.push_str(encoded_name(message.name.as_str()).as_str());
        cpp.push_str("_n_"_str);
        for (auto& enumeration : message.enums) {
            result = add_symbol(file,
                                SymbolKind::Enum,
                                nullptr,
                                &enumeration,
                                nested.as_str(),
                                cpp.as_str(),
                                enumeration.name.as_str());
            if (result.is_err()) return result;
        }
        for (auto& child : message.nested) {
            result = add_message(file, child, nested.as_str(), cpp.as_str(), depth + 1);
            if (result.is_err()) return result;
        }
        return Ok(empty {});
    }

public:
    auto files() const -> slice<FileNode> { return files_.as_slice(); }
    auto symbols() const -> slice<Symbol> { return symbols_.as_slice(); }
    auto file_index(ref<str> name) const -> Option<usize> {
        for (usize i {}; i < files_.len(); ++i)
            if (files_[i].source->name == name) return Some(i);
        return None();
    }
    auto resolve(usize consumer, ref<str> name, SymbolKind expected) const
        -> Result<const Symbol*, String> {
        for (auto& symbol : symbols_)
            if (symbol.proto_name == name) {
                if (symbol.kind != expected)
                    return Err(String::make("referenced symbol has the wrong kind"_str));
                if (! contains(files_[consumer].visible, symbol.file))
                    return Err(
                        String::make("referenced symbol is not visible through imports"_str));
                return Ok(&symbol);
            }
        return Err(String::make("referenced symbol was not found"_str));
    }
    static auto build(const Request& request) -> Result<DescriptorGraph, String> {
        if (request.files.len() > usize(1024))
            return Err(String::make("too many descriptor files"_str));
        DescriptorGraph graph;
        for (auto& file : request.files) {
            if (graph.file_index(file.name.as_str()).is_some())
                return Err(String::make("duplicate descriptor file"_str));
            auto names = file_names(file.name.as_str());
            if (names.is_err()) return Err(rstd::move(names.unwrap_err()));
            auto cpp = cpp_namespace(file.package.as_str());
            if (cpp.is_err()) return Err(rstd::move(cpp.unwrap_err()));
            graph.files_.push(FileNode { &file, rstd::move(*names), rstd::move(*cpp) });
        }
        for (usize i {}; i < graph.files_.len(); ++i) {
            auto& node = graph.files_[i];
            for (auto& name : node.source->imports) {
                auto target = graph.file_index(name.as_str());
                if (target.is_none()) return Err(String::make("missing import descriptor"_str));
                if (contains(node.imports, *target))
                    return Err(String::make("duplicate proto import"_str));
                node.imports.push(usize(*target));
            }
            for (auto index : node.source->public_imports) {
                if (usize(index.to_primitive()) >= node.imports.len())
                    return Err(String::make("invalid public dependency index"_str));
                auto target = node.imports[usize(index.to_primitive())];
                if (contains(node.public_imports, target))
                    return Err(String::make("duplicate public dependency index"_str));
                node.public_imports.push(usize(target));
            }
        }
        Vec<usize> sorted;
        while (sorted.len() < graph.files_.len()) {
            auto previous = sorted.len();
            for (usize i {}; i < graph.files_.len(); ++i) {
                if (contains(sorted, i)) continue;
                bool ready = true;
                for (auto dep : graph.files_[i].imports)
                    if (! contains(sorted, dep)) ready = false;
                if (ready) sorted.push(usize(i));
            }
            if (previous == sorted.len()) return Err(String::make("cyclic proto imports"_str));
        }
        // Public edges export declarations; a dependency's private edges do not.
        Vec<Vec<usize>> exported;
        for (usize i {}; i < graph.files_.len(); ++i) exported.push(Vec<usize> {});
        for (auto i : sorted) {
            include(exported[i], i);
            for (auto dep : graph.files_[i].public_imports)
                for (auto visible : exported[dep]) include(exported[i], visible);
            include(graph.files_[i].visible, i);
            for (auto dep : graph.files_[i].imports)
                for (auto visible : exported[dep]) include(graph.files_[i].visible, visible);
        }
        for (usize i {}; i < graph.files_.len(); ++i) {
            auto&  file   = *graph.files_[i].source;
            String prefix = String::make("."_str);
            if (! file.package.as_str().is_empty()) {
                prefix.push_str(file.package.as_str());
                prefix.push_ascii('.');
            }
            for (auto& enumeration : file.enums) {
                auto result = graph.add_symbol(i,
                                               SymbolKind::Enum,
                                               nullptr,
                                               &enumeration,
                                               prefix.as_str(),
                                               ""_str,
                                               enumeration.name.as_str());
                if (result.is_err()) return Err(rstd::move(result.unwrap_err()));
            }
            for (auto& message : file.messages) {
                auto result = graph.add_message(i, message, prefix.as_str(), ""_str, 0);
                if (result.is_err()) return Err(rstd::move(result.unwrap_err()));
            }
        }
        return Ok(rstd::move(graph));
    }
};
} // namespace liproto::codegen
