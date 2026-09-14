import json
import shutil
from pathlib import Path

root = Path(".")
XM = root / "extlibs/xmail/config/modules.json"
xm = json.loads(XM.read_text(encoding="utf-8"))

REMOVE = {
    # xsmtp
    "smtp", "smtp_client", "smtp_client_tls", "smtp_submit", "smtp_auth",
    "smtp_client_example", "smtp_client_tls_runtime_tests",
    # xpop3
    "pop3", "pop3_client", "pop3_client_tls", "pop3_auth",
    "pop3_client_example", "pop3_message", "pop3_message_runtime_tests",
    "pop3_client_tls_runtime_tests",
    # ximap
    "imap", "imap_data", "imap_body", "imap_client", "imap_client_tls",
    "imap_auth", "imap_command", "imap_message", "imap_append",
    "imap_compress", "imap_client_tls_runtime_tests",
    "imap_compress_tls_runtime_tests",
    # 跨协议
    "mail_protocol_oom_tests",
}
LIBS = {
    "xsmtp": {"proto": "smtp", "Proto": "Smtp", "PROTO": "SMTP",
              "mods": ["smtp", "smtp_client", "smtp_client_tls",
                       "smtp_submit", "smtp_auth", "smtp_client_example",
                       "smtp_client_tls_runtime_tests"],
              "extra_tests": ["smtp_protocol_oom_tests"],
              "bench_metric": "smtp_ops_per_sec",
              "headers": ["smtp.h"],
              "cond_headers": [
                  ("smtp_client.h", ["SMTP_CLIENT", "SMTP_CLIENT_TLS"]),
                  ("smtp_auth.h", ["SMTP_AUTH"]),
                  ("smtp_submit.h", ["SMTP_SUBMIT"]),
              ]},
    "xpop3": {"proto": "pop3", "Proto": "Pop3", "PROTO": "POP3",
              "mods": ["pop3", "pop3_client", "pop3_client_tls", "pop3_auth",
                       "pop3_client_example", "pop3_message",
                       "pop3_message_runtime_tests",
                       "pop3_client_tls_runtime_tests"],
              "extra_tests": ["pop3_protocol_oom_tests"],
              "bench_metric": "pop3_ops_per_sec",
              "headers": ["pop3.h"],
              "cond_headers": [
                  ("pop3_client.h", ["POP3_CLIENT", "POP3_CLIENT_TLS"]),
                  ("pop3_auth.h", ["POP3_AUTH"]),
                  ("pop3_message.h", ["POP3_MESSAGE"]),
              ]},
    "ximap": {"proto": "imap", "Proto": "Imap", "PROTO": "IMAP",
              "mods": ["imap", "imap_data", "imap_body", "imap_client",
                       "imap_client_tls", "imap_auth", "imap_command",
                       "imap_message", "imap_append", "imap_compress",
                       "imap_client_tls_runtime_tests",
                       "imap_compress_tls_runtime_tests"],
              "extra_tests": ["imap_protocol_oom_tests"],
              "bench_metric": "imap_ops_per_sec",
              "headers": ["imap.h", "imap_data.h", "imap_body.h"],
              "cond_headers": [
                  ("imap_client.h", ["IMAP_CLIENT", "IMAP_CLIENT_TLS"]),
                  ("imap_auth.h", ["IMAP_AUTH"]),
                  ("imap_command.h", ["IMAP_COMMAND"]),
                  ("imap_message.h", ["IMAP_MESSAGE"]),
                  ("imap_append.h", ["IMAP_APPEND"]),
                  ("imap_compress.h", ["IMAP_COMPRESS"]),
              ]},
}

ASSET_FIELDS = ("public_headers", "internal_headers", "sources", "tests",
                "single_tests", "examples", "docs", "benchmarks", "legacy")


def pathmap(lib, entries, drop_xmail_design):
    out = []
    for e in entries:
        if not isinstance(e, str) or not e.startswith("extlibs/xmail/"):
            out.append(e)
            continue
        if drop_xmail_design and "/docs/design/" in e:
            continue
        out.append(e.replace("extlibs/xmail/", f"extlibs/{lib}/", 1))
    return out


# ============ 1. xmail 手术 ============
keep, removed = [], {l: [] for l in LIBS}
for m in xm["modules"]:
    if m["name"] in REMOVE:
        owner = next((l for l, c in LIBS.items()
                      if m["name"] in c["mods"]), None)
        if owner is not None:
            removed[owner].append(m)
    else:
        keep.append(m)

by_name = {m["name"]: m for m in keep}
# 伞模块重定义
UMBRELLA_DEPS = ["mail_charset", "mail_codec", "mail_word", "mail_address",
                 "mail_date", "mail_id", "mail_param", "mail_multipart",
                 "mail_message", "mail_tree", "mail_build", "mail_compose",
                 "mail_wire", "mail_net", "mail_net_tls", "mail_net_deflate"]
umb = by_name["xmail"]
umb["depends"] = UMBRELLA_DEPS
for f in ("tests", "single_tests", "examples"):
    umb[f] = [p for p in umb.get(f, []) if (root / p).is_file()]
# 基准：bench_mail_transport.c 换成 wire 版
for m in keep:
    for f in ("benchmarks",):
        if "bench_mail_transport.c" in "\n".join(m.get(f, [])):
            m[f] = [p.replace("bench_mail_transport.c",
                              "bench_mail_wire.c") for p in m[f]]
# xmail_tests 重定义
by_name["xmail_tests"]["depends"] = [
    "xmail", "mail_net_tls_runtime_tests", "mail_compose_oom_tests",
    "mail_tree_oom_tests"]
# scope 修正
for sys_ in xm["scope"]["systems"]:
    if sys_["name"] == "mail_transport":
        sys_["source_roots"] = ["transport"]
# api_reference 收窄
ar = xm["api_reference"]
ar["function_prefixes"] = ["xrtMail"]
ar["constant_prefixes"] = ["XMAIL_"]
ar["type_prefixes"] = ["xmail"]
xm["modules"] = keep
XM.write_text(json.dumps(xm, ensure_ascii=False, indent="\t") + "\n",
              encoding="utf-8", newline="\n")
print(f"xmail manifest: {len(keep)} modules kept")

# ============ 2. 三个新 manifest ============
for lib, cfg in LIBS.items():
    UP, proto = lib.upper(), cfg["proto"]
    mods = []
    for m in removed[lib]:
        nm = dict(m)
        for f in ASSET_FIELDS:
            if f in nm:
                nm[f] = pathmap(lib, nm[f], drop_xmail_design=(f == "docs"))
        if isinstance(nm.get("feature"), str):
            nm["feature"] = nm["feature"].replace(
                f"XMAIL_FEATURE_{PROTO_PLACEHOLDER}" if False else
                f"XMAIL_FEATURE_{proto}", f"{UP}_FEATURE_{proto}")
        # 纯协议模块登记各自 bench
        if nm["name"] == proto:
            nm["benchmarks"] = [f"extlibs/{lib}/bench/bench_{proto}_protocol.c"]
        mods.append(nm)
    # OOM 测试模块
    mods.append({
        "name": f"{proto}_protocol_oom_tests", "state": "implemented",
        "feature": None, "depends": [proto, "memory_debug"],
        "public_headers": [], "internal_headers": [], "sources": [],
        "tests": [f"extlibs/{lib}/tests/test_{lib}_protocol_oom.c"],
        "single_tests": [], "examples": [], "docs": [], "legacy": [],
    })
    # 产品伞
    mods.append({
        "name": lib, "state": "implemented", "feature": None,
        "depends": [m["name"] for m in removed[lib]],
        "public_headers": [f"extlibs/{lib}/include/{lib}/features.h",
                           f"extlibs/{lib}/include/{lib}.h"],
        "internal_headers": [], "sources": [], "tests": [], "single_tests": [],
        "examples": [], "docs": [f"extlibs/{lib}/README.md"], "legacy": [],
    })
    # 测试聚合
    mods.append({
        "name": f"{lib}_tests", "state": "implemented", "feature": None,
        "collect_dependency_assets": ["tests", "single_tests"],
        "depends": ([m["name"] for m in removed[lib]
                     if m["name"].endswith(("_example", "_runtime_tests"))] +
                    [f"{proto}_protocol_oom_tests", lib]),
        "public_headers": [], "internal_headers": [], "sources": [],
        "tests": [], "single_tests": [], "examples": [],
        "docs": [f"extlibs/{lib}/README.md"], "legacy": [],
    })

    manifest = {
        "schema": 1,
        "product": lib,
        "dependency_manifests": ["extlibs/xmail/config/modules.json"],
        "module_prefix": f"{UP}_MODULE_",
        "single_header": f"extlibs/{lib}/single/{lib}.h",
        "declaration_header": f"extlibs/{lib}/single/{lib}_decl.h",
        "implementation_macro": f"{UP}_IMPLEMENTATION",
        "implementation_once_macro": f"{UP}_IMPLEMENTATION_ONCE",
        "single_guard": f"{UP}_SINGLE_HEADER_H",
        "single_marker": f"{UP}_SINGLE_HEADER",
        "declaration_guard": f"{UP}_DECLARATIONS_H",
        "declaration_marker": f"{UP}_DECLARATIONS",
        "package_consumer": f"extlibs/{lib}/tests/package/test_consumer.c",
        "product_root": f"extlibs/{lib}",
        "performance_config": f"extlibs/{lib}/config/performance_profiles.json",
        "size_config": f"extlibs/{lib}/config/size_profiles.json",
        "scope": {"product": lib, "external_integrations": [],
                  "systems": [{"name": f"{proto}_protocol", "state": "retained",
                               "source_roots": [proto]}]},
        "api_reference": {
            "title": f"{lib} 公共符号参考",
            "function_prefix": f"xrt{cfg['Proto']}",
            "constant_prefix": f"X{cfg['PROTO']}_",
            "type_prefix": f"x{proto}",
            "guide": "../../README.md",
            "output": f"extlibs/{lib}/docs/api/reference.md",
        },
        "features_guard": f"{UP}_FEATURES_H",
        "features_header": f"extlibs/{lib}/include/{lib}/features.h",
        "include_dirs": [f"extlibs/{lib}/include"],
        "header_roots": [f"extlibs/{lib}/include",
                         f"extlibs/{lib}/src/internal"],
        "modules": mods,
    }
    p = root / f"extlibs/{lib}/config/modules.json"
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(manifest, ensure_ascii=False, indent="\t") + "\n",
                 encoding="utf-8", newline="\n")
    print(f"{lib} manifest: {len(mods)} modules")

# ============ 3. profiles ============
# xmail size：删协议 profile
sp = root / "extlibs/xmail/config/size_profiles.json"
d = json.loads(sp.read_text(encoding="utf-8"))
d["profiles"] = [p for p in d["profiles"]
                 if not any(k in p["name"] for k in
                            ("smtp", "pop3", "imap", "mail_protocols"))]
sp.write_text(json.dumps(d, ensure_ascii=False, indent="\t") + "\n",
              encoding="utf-8", newline="\n")
# xmail perf：mail_transport 收窄为 wire
pp = root / "extlibs/xmail/config/performance_profiles.json"
d = json.loads(pp.read_text(encoding="utf-8"))
for prof in d["profiles"]:
    if prof["name"] == "mail_transport":
        prof["description"] = "邮件线路读写与 dot 编码零分配热路径"
        prof["benchmarks"] = [{
            "name": "mail_wire", "suite": "mail_wire",
            "source": "extlibs/xmail/bench/bench_mail_wire.c",
            "args": ["200000"], "smoke_args": ["100"],
            "metrics": {"wire_mib_per_sec": {"direction": "higher",
                                             "unit": "MiB/s"}},
        }]
pp.write_text(json.dumps(d, ensure_ascii=False, indent="\t") + "\n",
              encoding="utf-8", newline="\n")

# 新库 profiles
for lib, cfg in LIBS.items():
    proto = cfg["proto"]
    size = {"schema": 1, "manifest": f"extlibs/{lib}/config/modules.json",
            "profiles": [
                {"name": f"{proto}_protocol", "system": f"{proto}_protocol",
                 "suite": proto, "description": f"{proto.upper()} 协议纯解析与写出"},
                {"name": lib, "system": f"{proto}_protocol", "suite": lib,
                 "description": f"{lib} 当前完整扩展闭包"}],
            "default_growth": {"text": 0.1, "data": 0.2, "bss": 0.2,
                               "file": 0.15}}
    perf = {"schema": 1, "manifest": f"extlibs/{lib}/config/modules.json",
            "defaults": {"repeats": 5, "warmups": 1, "timeout_seconds": 120,
                         "policy": "baseline-unpinned", "cflags": ["-O2"],
                         "ldflags": [], "max_relative_mad": 0.20,
                         "max_relative_central_range": 0.30,
                         "regression": {"higher": 0.10, "lower": 0.20}},
            "profiles": [
                {"name": f"{proto}_protocol", "system": f"{proto}_protocol",
                 "description": f"{proto.upper()} 协议零分配热路径",
                 "benchmarks": [{
                     "name": f"{proto}_protocol", "suite": proto,
                     "source": f"extlibs/{lib}/bench/bench_{proto}_protocol.c",
                     "args": ["200000"], "smoke_args": ["100"],
                     "metrics": {cfg["bench_metric"]: {
                         "direction": "higher", "unit": "operations/s"}},
                 }]}]}
    (root / f"extlibs/{lib}/config/size_profiles.json").write_text(
        json.dumps(size, ensure_ascii=False, indent="\t") + "\n",
        encoding="utf-8", newline="\n")
    (root / f"extlibs/{lib}/config/performance_profiles.json").write_text(
        json.dumps(perf, ensure_ascii=False, indent="\t") + "\n",
        encoding="utf-8", newline="\n")
print("profiles ok")

# ============ 4. bench 拆分 ============
COMMON = '#include "../../../dev/bench/bench_common.h"\n\n'
src = (root / "extlibs/xmail/bench/bench_mail_transport.c").read_text(
    encoding="utf-8")

def section(marker_start, marker_end):
    i = src.index(marker_start)
    j = src.index(marker_end) if marker_end else len(src)
    return src[i:j]

wire_loop = section("static const char sWire[]", "xbenchTimerStop(&Timer);\n\tiWireElapsed")
smtp_loop = section("xsmtpreplyline Reply;", "iSmtpElapsed")
pop3_loop = section("xpop3replyview Reply;", "iPop3Elapsed")
imap_loop = section("ximapresponseview Response;", "iImapElapsed")

wire_file = (root / "extlibs/xmail/bench/bench_mail_wire.c")
wire_file.write_text(COMMON + '#define XMAIL_MODULE_MAIL_WIRE\n#include <xmail.h>\n\n\n'
    '/* 测量邮件线路读写与 dot 编码的零分配热路径。 */\nint main(int argc, char** argv)\n{\n'
    + src[src.index("	static const char sWire[]"):src.index("	uint64 iWireElapsed;")]
    .replace("uint64 iSmtpElapsed;\n\tuint64 iPop3Elapsed;\n\tuint64 iImapElapsed;\n", "")
    .replace("uint64 iWireElapsed;", "uint64 iWireElapsed;")
    + src[src.index("	if ( iIterations == 0 )"):src.index("	xbenchTimerStart(&Timer);\n	for ( uint32 i = 0; i < iIterations; i++ ) {\n		xsmtpreplyline")]
    + wire_loop.replace("xbenchTimerStop(&Timer);\n\tiWireElapsed",
                        "xbenchTimerStop(&Timer);\n\tiWireElapsed")
    + ' = xbenchTimerElapsedNs(&Timer);\n\n'
    '	printf("xrt Mail wire benchmark\\n");\n'
    '	xbenchPrintMetricDouble(\n'
    '		"wire_mib_per_sec",\n'
    '		xbenchSafeRate(\n'
    '			(uint64)iIterations * (sizeof(sWire) - 1u),\n'
    '			iWireElapsed\n'
    '		) / (1024.0 * 1024.0)\n'
    '	);\n'
    '	xbenchPrintMetricU64("checksum", iChecksum);\n'
    '	return 0;\n}\n', encoding="utf-8", newline="\n")

def proto_bench(lib, proto_l, Proto, decl, body, metric, ret):
    return (COMMON + f'#define {lib.upper()}_MODULE_{proto_l.upper()}\n'
            f'#include <{lib}.h>\n\n\n'
            f'/* 测量 {proto_l.upper()} 协议零分配解析与命令写出热路径。 */\n'
            'int main(int argc, char** argv)\n{\n'
            '	uint32 iIterations = xbenchArgU32(argc, argv, 1, 200000u);\n'
            '	char arrOutput[256];\n'
            '	size_t iOutputSize;\n'
            '	uint64 iChecksum = 0;\n'
            '	xbenchtimer Timer;\n'
            '	uint64 iElapsed;\n\n'
            '	if ( iIterations == 0 ) {\n'
            '		return 1;\n'
            '	}\n\n'
            '	xbenchTimerStart(&Timer);\n'
            '	for ( uint32 i = 0; i < iIterations; i++ ) {\n'
            + decl + body +
            '	}\n'
            '	xbenchTimerStop(&Timer);\n'
            '	iElapsed = xbenchTimerElapsedNs(&Timer);\n\n'
            f'	printf("xrt {proto_l.upper()} protocol benchmark\\n");\n'
            '	xbenchPrintMetricDouble(\n'
            f'		"{metric}",\n'
            '		xbenchSafeRate(iIterations, iElapsed)\n'
            '	);\n'
            '	xbenchPrintMetricU64("checksum", iChecksum);\n'
            '	return 0;\n}\n')

# SMTP
d_start = src.index("\t\txsmtpreplyline Reply;")
d_end = src.index("\t\txbenchTimerStop(&Timer);\n\tiSmtpElapsed")
smtp_decl_body = src[d_start:d_end] + "\n"
(root / "extlibs/xsmtp/bench/bench_smtp_protocol.c").write_text(
    proto_bench("xsmtp", "smtp", "Smtp", "", smtp_decl_body,
                "smtp_ops_per_sec", 3), encoding="utf-8", newline="\n")
d_start = src.index("\t\txpop3replyview Reply;")
d_end = src.index("\t\txbenchTimerStop(&Timer);\n\tiPop3Elapsed")
pop3_decl_body = src[d_start:d_end] + "\n"
(root / "extlibs/xpop3/bench/bench_pop3_protocol.c").write_text(
    proto_bench("xpop3", "pop3", "Pop3", "", pop3_decl_body,
                "pop3_ops_per_sec", 4), encoding="utf-8", newline="\n")
d_start = src.index("\t\tximapresponseview Response;")
d_end = src.index("\t\txbenchTimerStop(&Timer);\n\tiImapElapsed")
imap_decl_body = src[d_start:d_end] + "\n"
(root / "extlibs/ximap/bench/bench_imap_protocol.c").write_text(
    proto_bench("ximap", "imap", "Imap", "", imap_decl_body,
                "imap_ops_per_sec", 5), encoding="utf-8", newline="\n")
(root / "extlibs/xmail/bench/bench_mail_transport.c").unlink()
print("bench split ok")

# ============ 5. 伞头 ============
for lib, cfg in LIBS.items():
    UP, proto = lib.upper(), cfg["proto"]
    t = [f"#ifndef {UP}_H", f"#define {UP}_H", "",
         f"#include <{lib}/features.h>", "#include <xmail.h>"]
    for h in cfg["headers"]:
        t.append(f"#include <xrt/{h}>")
    t.append("")
    for h, feats in cfg["cond_headers"]:
        if len(feats) == 1:
            t.append(f"#if defined({UP}_FEATURE_{feats[0]})")
        else:
            t.append("#if defined(" + f"{UP}_FEATURE_{feats[0]}" + ") || \\")
            for extra in feats[1:-1]:
                t.append(f"\tdefined({UP}_FEATURE_{extra}) || \\")
            t.append(f"\tdefined({UP}_FEATURE_{feats[-1]})")
        t.append(f'\t#include <xrt/{h}>')
        t.append("#endif")
        t.append("")
    t += [f"#endif", ""]
    (root / f"extlibs/{lib}/include/{lib}.h").write_text(
        "\n".join(t), encoding="utf-8", newline="\n")
print("umbrella headers ok")

# ============ 6. test consumers + README ============
consumers = {
    "xsmtp": ('#define XSMTP_MODULE_SMTP\n#include <xsmtp.h>\n\n\n'
              '/* 验证发布包只安装公共头时仍可解析 SMTP 响应行。 */\n'
              'int main(void)\n{\n\txsmtpreplyline Reply;\n\n'
              '\treturn xrtSmtpReplyLineParse(\n'
              '\t\tXRT_STR_LITERAL("250-SIZE 10485760"),\n'
              '\t\t&Reply\n\t) ? 0 : 1;\n}\n'),
    "xpop3": ('#define XPOP3_MODULE_POP3\n#include <xpop3.h>\n\n\n'
              '/* 验证发布包只安装公共头时仍可解析 POP3 响应。 */\n'
              'int main(void)\n{\n\txpop3replyview Reply;\n\n'
              '\treturn xrtPop3ReplyParse(\n'
              '\t\tXRT_STR_LITERAL("+OK mailbox ready"),\n'
              '\t\t&Reply\n\t) ? 0 : 1;\n}\n'),
    "ximap": ('#define XIMAP_MODULE_IMAP\n#include <ximap.h>\n\n\n'
              '/* 验证发布包只安装公共头时仍可解析 IMAP 响应。 */\n'
              'int main(void)\n{\n\tximapresponseview Response;\n\n'
              '\treturn xrtImapResponseParse(\n'
              '\t\tXRT_STR_LITERAL("* 23 FETCH (BODY[] {4096}"),\n'
              '\t\t&Response\n\t) ? 0 : 1;\n}\n'),
}
readmes = {
    "xsmtp": "xsmtp 是构建在 xmail 邮件基座（MIME 内容层与传输层）之上的 SMTP 客户端扩展库：协议解析、同步客户端、STARTTLS/隐式 TLS、SASL 认证与从 xmailmessage 派生的流式提交。通过 `XSMTP_MODULE_*` 宏裁剪。\n",
    "xpop3": "xpop3 是构建在 xmail 邮件基座之上的 POP3 客户端扩展库：协议解析、同步客户端、STLS、SASL 认证与 RETR/TOP 到 MIME 树的桥接。通过 `XPOP3_MODULE_*` 宏裁剪。\n",
    "ximap": "ximap 是构建在 xmail 邮件基座之上的 IMAP 客户端扩展库：协议解析、命令层、FETCH/BODYSTRUCTURE 数据视图、流式 APPEND、IDLE 与 RFC 4978 COMPRESS=DEFLATE。通过 `XIMAP_MODULE_*` 宏裁剪。\n",
}
for lib in LIBS:
    (root / f"extlibs/{lib}/tests/package/test_consumer.c").write_text(
        consumers[lib], encoding="utf-8", newline="\n")
    (root / f"extlibs/{lib}/README.md").write_text(
        f"# {lib}\n\n" + readmes[lib], encoding="utf-8", newline="\n")
print("consumers + readmes ok")
