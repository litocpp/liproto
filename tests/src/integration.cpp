#include <rstd/test/gtest.hpp>
import rstd;
import liproto;
using namespace rstd::prelude;
using rstd::literals::operator""_str;
using rstd::literals::operator""_bytes;
using ::alloc::string::String;

TEST(Integration, ProtocGeneratedModuleCompilesAndRuns) {
    auto plugin      = rstd::env::var_os("LIPROTO_TEST_PLUGIN"_str);
    auto source      = rstd::env::var_os("LIPROTO_TEST_SOURCE"_str);
    auto rstd_source = rstd::env::var_os("LIPROTO_TEST_RSTD"_str);
    if (plugin.is_none() || source.is_none() || rstd_source.is_none()) GTEST_SKIP();
    auto temporary = rstd::fs::TempDir::make("liproto-generated"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(temporary.path().as_os_str().to_os_string());
    auto write     = [&](ref<str> name, ref<str> contents) {
        return rstd::fs::write(root.join(name).as_path(), contents.as_bytes());
    };
    ASSERT_TRUE(rstd::fs::soft_link(rstd::ref<rstd::path::Path>(source->as_os_str()),
                                    root.join("liproto"_str).as_path())
                    .is_ok());
    ASSERT_TRUE(rstd::fs::soft_link(rstd::ref<rstd::path::Path>(rstd_source->as_os_str()),
                                    root.join("rstd"_str).as_path())
                    .is_ok());
    ASSERT_TRUE(rstd::fs::soft_link(rstd::ref<rstd::path::Path>(plugin->as_os_str()),
                                    root.join("plugin"_str).as_path())
                    .is_ok());
    ASSERT_TRUE(rstd::fs::create_dir_all(root.join(".lito"_str).as_path()).is_ok());
    ASSERT_TRUE(write(".lito/config.toml"_str, R"([patch.litocpp]
rstd-basic = { path = "rstd" }
rstd-runtime = { path = "rstd" }
rstd-core = { path = "rstd" }
rstd-alloc = { path = "rstd" }
rstd-parse = { path = "rstd" }
rstd-nostd = { path = "rstd" }
rstd-std = { path = "rstd" }
)"_str)
                    .is_ok());
    ASSERT_TRUE(write("demo.proto"_str, R"(syntax = "proto3";
package demo;
import "other.proto";
enum Status { UNKNOWN = 0; READY = 1; }
message Sample {
  enum Local { option allow_alias = true; ZERO = 0; ALIAS = 0; ACTIVE = 1; }
  Local local = 17;
  map<string, Local> states = 18;
  uint32 count = 1;
  optional string label = 2;
  repeated sint64 values = 3;
  double ratio = 4;
  bytes payload = 5;
  Sample child = 6;
  Peer peer = 7;
  repeated Sample siblings = 8;
  oneof selection { uint32 first = 9; uint32 second = 10; Sample selected = 11; }
  message Nested { uint32 count = 1; }
  Nested nested = 12;
  other.External external = 13;
  Status status = 14;
  map<string, uint32> counts = 15;
  map<uint32, Sample> objects = 16;
}
message Peer { Sample parent = 1; }
message Empty {}
)"_str)
                    .is_ok());
    ASSERT_TRUE(
        write("other.proto"_str,
              R"(syntax = "proto3"; package other; message External { uint32 value = 1; })"_str)
            .is_ok());
    auto plugin_argument = rstd::ffi::OsString::from("--plugin=protoc-gen-liproto="_str);
    plugin_argument.push(plugin->as_os_str());
    auto generated = rstd::process::Command::make("protoc"_str)
                         .current_dir(root.as_path())
                         .arg("-I."_str)
                         .arg(plugin_argument.as_os_str())
                         .arg("--liproto_out=."_str)
                         .arg("demo.proto"_str)
                         .arg("other.proto"_str)
                         .output();
    ASSERT_TRUE(generated.is_ok());
    if (! generated->status.success()) {
        auto error = rstd::io::stderr();
        (void)rstd::io::write_all(error, generated->stderr_buf.as_slice());
    }
    ASSERT_TRUE(generated->status.success());
    auto oracle = [&](ref<str> mode, slice<u8> input) {
        auto child = rstd::process::Command::make("protoc"_str)
                         .current_dir(root.as_path())
                         .arg("-I."_str)
                         .arg(mode)
                         .arg("demo.proto"_str)
                         .set_stdin(rstd::process::Stdio::piped())
                         .set_stdout(rstd::process::Stdio::piped())
                         .set_stderr(rstd::process::Stdio::piped())
                         .spawn()
                         .unwrap();
        {
            auto pipe = child.take_stdin().unwrap();
            rstd::io::write_all(pipe, input).unwrap();
        }
        return child.wait_with_output().unwrap();
    };
    auto official = oracle(
        "--encode=demo.Sample"_str,
        "count: 150 label: \"\" values: -1 values: 2 local: ACTIVE states { key: \"k\" value: ACTIVE }"_bytes);
    ASSERT_TRUE(official.status.success());
    auto           header = String::make("#define ORACLE_BYTES \""_str);
    constexpr char hex[]  = "0123456789abcdef";
    for (auto byte : official.stdout_buf.as_slice()) {
        header.push_str("\\x"_str);
        header.push_ascii(hex[byte.to_primitive() >> 4]);
        header.push_ascii(hex[byte.to_primitive() & 15]);
    }
    header.push_str("\"_bytes\n"_str);
    ASSERT_TRUE(write("oracle.hpp"_str, header.as_str()).is_ok());
    ASSERT_TRUE(write("lito.toml"_str, R"([package]
name = "generated-check"
version = "0.1.0"
[lib]
name = "generated-check"
module = "generated.check"
archive = "generated-check"
sources = ["lib.cppm"]
[[bin]]
name = "generated-check"
sources = ["main.cpp"]
link-stdlib = false
[dependencies]
liproto = { path = "liproto/runtime", pub = true }
rstd-nostd = { path = "rstd/src/nostd" }
rstd-std = { path = "rstd/src/std" }
[build-tools.protoc]
path = "protoc"
[build-tools.plugin]
path = "./plugin"
[profile]
exceptions = false
rtti = false
)"_str)
                    .is_ok());
    ASSERT_TRUE(
        write("lib.cppm"_str, "export module generated.check;\nexport import proto.p_demo;\n"_str)
            .is_ok());
    ASSERT_TRUE(write("build.lua"_str,
                      R"(local target = lito.target({kind = "lib", name = "generated-check"})
local generated = lito.run({
  tool = lito.tool("protoc"), tools = {lito.tool("plugin")}, cwd = ".", output_cwd = 1,
  args = {"-I.", "--plugin=protoc-gen-liproto=@TOOL:1@", "--liproto_out=.", "demo.proto", "other.proto"},
  inputs = {"demo.proto", "other.proto"}, outputs = {"src/demo.cppm", "src/other.cppm"},
})
lito.target_add_generated_source(target, generated.outputs[1])
lito.target_add_generated_source(target, generated.outputs[2])
)"_str)
                    .is_ok());
    ASSERT_TRUE(write("main.cpp"_str, R"(#include "oracle.hpp"
import proto.p_demo;
import rstd;
using namespace rstd::prelude;
using rstd::literals::operator""_bytes;
int main() {
    auto empty = liproto::decode<p_demo::M_Empty>(""_bytes);
    if (empty.is_err() || liproto::encoded_len(*empty).unwrap() != usize()) return 18;
    auto decoded = liproto::decode<p_demo::M_Sample>(ORACLE_BYTES);
    if (decoded.is_err()) return 1;
    if (decoded->f_local != i32(1) || decoded->f_states.len() != usize(1)) return 19;
    if (p_demo::known_E_Sample_n_Local(i32(0)).is_none()) return 20;
    if (decoded->f_count != u32(150) || decoded->f_label.is_none() || decoded->f_values.len() != usize(2)) return 2;
    if (decoded->f_values[usize()] != i64(-1) || decoded->f_values[usize(1)] != i64(2)) return 3;
    decoded->f_ratio = f64(1.5);
    auto merged = liproto::merge(*decoded, "\x32\x02\x08\x07\x32\x02\x12\x00"_bytes);
    if (merged.is_err() || decoded->f_child.is_none()) return 6;
    if ((*decoded->f_child)->f_count != u32(7) || (*decoded->f_child)->f_label.is_none()) return 7;
    if (liproto::merge(*decoded, "\x48\x01\x50\x02"_bytes).is_err()) return 8;
    if (decoded->o_selection.is_none() || !decoded->o_selection->is<10u>()) return 9;
    if (liproto::merge(*decoded, "\x5a\x02\x08\x07\x5a\x02\x12\x00"_bytes).is_err()) return 10;
    if (!decoded->o_selection->is<11u>() || decoded->o_selection->as<11u>()->f_label.is_none()) return 11;
    if (liproto::merge(*decoded, "\x62\x02\x08\x01\x6a\x02\x08\x02\x70\x63"_bytes).is_err()) return 12;
    if (decoded->f_nested.is_none() || decoded->f_external.is_none() || decoded->f_status != i32(99)) return 13;
    if (p_demo::known_E_Status(decoded->f_status).is_some()) return 14;
    if (liproto::merge(*decoded, "\x7a\x05\x0a\x01k\x10\x01\x7a\x05\x0a\x01k\x10\x02\x82\x01\x06\x08\x01\x12\x02\x08\x03"_bytes).is_err()) return 15;
    if (decoded->f_counts.len() != usize(1) || decoded->f_objects.len() != usize(1)) return 16;
    auto encoded = liproto::encode_to_vec(*decoded);
    if (encoded.is_err()) return 4;
    auto roundtrip = liproto::decode<p_demo::M_Sample>(encoded->as_slice());
    if (roundtrip.is_err() || roundtrip->f_label.is_none() || roundtrip->f_ratio != f64(1.5)) return 5;
    auto stream = rstd::io::stdout();
    if (rstd::io::write_all(stream, encoded->as_slice()).is_err()) return 17;
    return 0;
}
)"_str)
                    .is_ok());
    auto built = rstd::process::Command::make("lito"_str)
                     .current_dir(root.as_path())
                     .arg("build"_str)
                     .arg("--profile"_str)
                     .arg("release"_str)
                     .arg("-j"_str)
                     .arg("8"_str)
                     .output();
    ASSERT_TRUE(built.is_ok());
    if (! built->status.success()) {
        auto error = rstd::io::stderr();
        (void)rstd::io::write_all(error, built->stderr_buf.as_slice());
    }
    ASSERT_TRUE(built->status.success());
    auto executable = root.join("build/release/bin/generated-check/generated-check"_str);
    auto result     = rstd::process::Command::make(executable.as_path().as_os_str()).output();
    ASSERT_TRUE(result.is_ok());
    ASSERT_TRUE(result->status.success());
    auto inspected = oracle("--decode=demo.Sample"_str, result->stdout_buf.as_slice());
    ASSERT_TRUE(inspected.status.success());
    auto rendered = rstd::str_::from_utf8(inspected.stdout_buf.as_slice()).unwrap();
    EXPECT_TRUE(rendered.contains("count: 150"_str));
    EXPECT_TRUE(rendered.contains("label: \"\""_str));
    EXPECT_TRUE(rendered.contains("status: 99"_str));
    ASSERT_TRUE(
        write(
            "other.proto"_str,
            "syntax = \"proto3\"; package other; message External { uint32 value = 1; uint32 revision = 2; }"_str)
            .is_ok());
    ASSERT_TRUE(
        write(
            "main.cpp"_str,
            "import generated.check;\nusing rstd::u32;\nint main() { p_other::M_External value; value.f_revision = u32(2); return value.f_revision == u32(2) ? 0 : 1; }\n"_str)
            .is_ok());
    auto rebuilt = rstd::process::Command::make("lito"_str)
                       .current_dir(root.as_path())
                       .arg("build"_str)
                       .arg("--profile"_str)
                       .arg("release"_str)
                       .arg("-j"_str)
                       .arg("8"_str)
                       .output();
    ASSERT_TRUE(rebuilt.is_ok());
    if (! rebuilt->status.success()) {
        auto error = rstd::io::stderr();
        (void)rstd::io::write_all(error, rebuilt->stderr_buf.as_slice());
    }
    ASSERT_TRUE(rebuilt->status.success());
    EXPECT_TRUE(
        rstd::process::Command::make(executable.as_path().as_os_str()).status().unwrap().success());
}
