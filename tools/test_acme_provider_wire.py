"""Actual DNS provider requests against an independent signed TLS service.

Uses only fixed dummy keys. Signature oracles follow the official ACS3 and
SigV4, TC3 and SDK-HMAC specifications using Python's standard library.
This validates wire contracts and controlled faults, not live cloud accounts.
"""

from __future__ import annotations

import argparse
import collections
import datetime
import hashlib
import hmac
import json
import os
import re
import selectors
import socket
import ssl
import subprocess
import tempfile
import threading
import time
import urllib.parse
import xml.etree.ElementTree as ET
from pathlib import Path
from xml.sax.saxutils import escape

from http_tls_test_fixture import certificates, run_command

ROOT = Path(__file__).resolve().parents[1]
CLIENT = ROOT / "extlibs/xacme/tests/acme/test_provider_wire_client.c"
OWNER = "_acme-challenge.api.example.com"
OTHER = "_acme-challenge.other.example.org"
MANUAL = '"manual & preserved"'
REST_ZONE_FAULTS = ("parent-child", "appears", "disappears", "owner-apex", "replacement",
                    "denied", "malformed", "ambiguous", "no-zone", "no-zone-stale-error")
REST_ZONE_SCENARIOS = frozenset("rest-zone-" + fault for fault in REST_ZONE_FAULTS)
REST_ZONE_INPUT_FAULTS = ("oom-empty", "oom-found", "401", "403", "429", "503", "201", "202", "206", "stale-401", "stale-503",
                          "stale-malformed", "stale-shape", "nul-empty", "nul-found", "terminal-nul", "trailing-text")
REST_OWNED_FAULTS = ("normal", "owner-case", "repeat-many", "capacity", "concurrent-start", "zone-changed",
                     "read-id", "read-owner", "read-type", "read-value", "read-denied", "read-500",
                     "read-malformed", "read-nul", "read-lost", "read-lost-once", "read-oom", "handle-invalid", "missing")
TC_ZONE_FAULTS = ("parent-child", "appears", "disappears", "replacement", "owner-apex", "owner-case", "owner-long",
                  "invalid-owner", "denied", "503", "malformed", "nul", "wrong-domain", "missing-request",
                  "no-zone", "no-zone-stale-error", "apex-read-owner", "apex-read-domain-id", "apex-read-value", "apex-read-oom")


def build_client(root: Path, compiler: str, sanitize: bool, coverage: bool = False) -> Path:
    if sanitize and (os.name == "nt" or coverage):
        raise ValueError("sanitizers require POSIX and a separate non-coverage build")
    binary = root / ("acme_provider_wire_client" + (".exe" if os.name == "nt" else ""))
    flags = ["-fno-omit-frame-pointer", "-fsanitize=address,undefined"] if sanitize else []
    profile = ["--coverage", "-fprofile-update=atomic"] if coverage else []
    libs = ["-lws2_32", "-lbcrypt", "-ladvapi32", "-liphlpapi"] if os.name == "nt" else ["-pthread", "-lm"]
    run_command(compiler, "-std=c11", "-D_GNU_SOURCE", "-O0" if coverage else "-O2",
                "-Wall", "-Wextra", "-Werror", *profile, *flags,
                "-I", str(ROOT / "single"), "-I", str(ROOT / "include"),
                "-I", str(ROOT / "extlibs/xacme/include"), str(CLIENT),
                "-o", str(binary), *libs, *flags)
    return binary


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def mac(key: bytes, message: str) -> bytes:
    return hmac.new(key, message.encode("utf-8"), hashlib.sha256).digest()


def signed_request(method: str, target: str, headers: dict[str, str], body: bytes,
                   provider: str, nonces: set[str]) -> dict[str, str]:
    path, _, query = target.partition("?")
    pairs = urllib.parse.parse_qsl(query, keep_blank_values=True, strict_parsing=True)
    if len(pairs) != len(dict(pairs)):
        raise AssertionError("duplicate business query keys")
    quote = lambda value: urllib.parse.quote(value, safe="-_.~")
    canonical_query = "&".join(sorted(f"{quote(k)}={quote(v)}" for k, v in pairs))
    if canonical_query != query:
        raise AssertionError("query is not encoded and sorted canonically")
    authorization = headers["authorization"]
    algorithm, fields = authorization.split(" ", 1)
    auth = dict(part.strip().split("=", 1) for part in fields.split(","))
    signed = auth["SignedHeaders"].split(";")
    expected = (["host", "x-amz-content-sha256", "x-amz-date"] if provider == "aws" else
                ["content-type", "host", "x-acs-action", "x-acs-content-sha256", "x-acs-date",
                 "x-acs-signature-nonce", "x-acs-version"])
    if signed != expected:
        raise AssertionError("signed header set differs from provider contract")
    canonical_headers = "".join(f"{name}:{' '.join(headers[name].split())}\n" for name in signed)
    canonical = "\n".join([method, path, canonical_query, canonical_headers,
                            ";".join(signed), digest(body)])
    if provider == "aws":
        if algorithm != "AWS4-HMAC-SHA256":
            raise AssertionError("wrong AWS algorithm")
        key_id, date, region, service, terminator = auth["Credential"].split("/")
        if (key_id, region, service, terminator) != ("test-id", "us-east-1", "route53", "aws4_request"):
            raise AssertionError("wrong AWS credential scope")
        stamp = headers["x-amz-date"]
        if stamp[:8] != date or headers["x-amz-content-sha256"] != digest(body):
            raise AssertionError("AWS payload hash or date mismatch")
        sent = datetime.datetime.strptime(stamp, "%Y%m%dT%H%M%SZ").replace(tzinfo=datetime.timezone.utc)
        key = mac(mac(mac(mac(b"AWS4test-key", date), region), service), terminator)
        signature = mac(key, "\n".join([algorithm, stamp, "/".join([date, region, service, terminator]),
                                       digest(canonical.encode())])).hex()
    else:
        if algorithm != "ACS3-HMAC-SHA256" or auth["Credential"] != "test-id" or method != "POST" or path != "/":
            raise AssertionError("wrong ACS3 credential, method or URI")
        nonce = headers["x-acs-signature-nonce"]
        if not re.fullmatch("[0-9a-f]{32}", nonce) or nonce in nonces:
            raise AssertionError("nonce missing or reused")
        nonces.add(nonce)
        if body or headers["x-acs-content-sha256"] != digest(body) or headers["x-acs-version"] != "2015-01-09":
            raise AssertionError("Ali payload hash or API version mismatch")
        sent = datetime.datetime.strptime(headers["x-acs-date"], "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=datetime.timezone.utc)
        signature = mac(b"test-key", algorithm + "\n" + digest(canonical.encode())).hex()
    if abs((datetime.datetime.now(datetime.timezone.utc) - sent).total_seconds()) > 90:
        raise AssertionError("request signing timestamp is stale")
    if not hmac.compare_digest(signature, auth["Signature"]):
        raise AssertionError("independent signature verification failed")
    return dict(pairs)


def read_request(peer: ssl.SSLSocket) -> tuple[str, str, dict[str, str], bytes]:
    data = bytearray()
    while b"\r\n\r\n" not in data:
        piece = peer.recv(4096)
        if not piece or len(data) > 16384:
            raise AssertionError("request header is truncated or oversized")
        data.extend(piece)
    head, body = bytes(data).split(b"\r\n\r\n", 1)
    first, *lines = head.decode("ascii").split("\r\n")
    method, target, version = first.split(" ")
    headers = {}
    for line in lines:
        key, value = line.split(":", 1)
        key = key.lower()
        if key in headers:
            raise AssertionError("duplicate request header")
        headers[key] = value.strip()
    if version != "HTTP/1.1" or "transfer-encoding" in headers:
        raise AssertionError("unsupported request framing")
    size = int(headers.get("content-length", "0"))
    if not 0 <= size <= 262144:
        raise AssertionError("invalid request body size")
    while len(body) < size:
        piece = peer.recv(size - len(body))
        if not piece:
            raise AssertionError("request body is truncated")
        body += piece
    if len(body) != size:
        raise AssertionError("request body exceeds its framing")
    return method, target, headers, body


class Service:
    def __init__(self, provider: str, scenario: str):
        self.provider, self.scenario = provider, scenario
        self.calls = collections.Counter()
        self.nonces: set[str] = set()
        self.ali: dict[str, tuple[str, str, str]] = {}
        self.aws = {OWNER: (721, [MANUAL]), OTHER: (913, [MANUAL])}
        if scenario in {"existing", "existing-escaped", "aws-alias-borrowed"}:
            self.aws[OWNER][1].append('"first_digest-01"')
        self.faults = 0
        self.deletes: list[str] = []
        self.record_reads: list[str] = []
        self.zone_queries: list[str] = []
        self.next_ali_id = 11
        self.live_uppercase = scenario == "live-uppercase"
        self.live_absence = scenario.startswith("live-absence-")
        if self.live_uppercase:
            self.scenario = "multi"
        elif self.live_absence:
            self.scenario = scenario.removeprefix("live-absence-")

    def dispatch(self, method, target, headers, body):
        query = signed_request(method, target, headers, body, self.provider, self.nonces)
        if self.provider == "ali":
            return self.ali_request(headers["x-acs-action"], query)
        return self.aws_request(method, target.partition("?")[0], query, body)

    def ali_request(self, action, query):
        self.calls[action] += 1
        if action == "DescribeDomainRecords":
            child_faults = {"zone-forbidden", "zone-403-missing", "zone-generic-404", "zone-server-503", "zone-throttle"}
            if self.scenario.startswith("zone-") and not self.faults:
                if query["DomainName"] == ("api.example.com" if self.scenario in child_faults else "example.com"):
                    self.faults += 1
                    return self.zone_fault(query)
            if self.scenario in {"concurrent", "alias-concurrent-same"} and self.calls[action] == 1:
                time.sleep(0.2)  # Hold the first request while all caller threads enter Add.
            if self.scenario == "discovery-no-zone":
                return 400, {"Code": "InvalidDomainName.NoExist"}
            managed = {"example.com", "example.org"}
            if self.scenario == "discovery-specific-zone":
                managed.add("api.example.com")
            if self.scenario == "discovery-cache-capacity":
                managed.update(f"example{i}.com" for i in range(6))
            if query["DomainName"] not in managed:
                return 400, {"Code": "DomainNotFound" if self.scenario == "discovery-missing-alias" else "InvalidDomainName.NoExist"}
            if self.scenario == "discovery-full-page":
                return 200, {"TotalCount": 9, "PageSize": 1, "PageNumber": 1, "RequestId": "fixture",
                             "DomainRecords": {"Record": [{"DomainName": query["DomainName"].upper(),
                                                           "RecordId": "outside", "Type": "MX", "RR": "@"}]}}
            return 200, {"TotalCount": 0, "PageSize": int(query.get("PageSize", "20")), "PageNumber": 1,
                         "RequestId": "fixture", "DomainRecords": {"Record": []}}
        if action == "AddDomainRecord":
            assert query["Type"] == "TXT" and (query["RR"].lower() == "_acme-challenge" or query["RR"].lower().startswith("_acme-challenge."))
            if self.scenario == "discovery-specific-zone":
                assert query["DomainName"] == ("api.example.com" if query["Value"] == "child_digest" else "example.com")
            if self.scenario == "flow-uncommitted-503":
                return 503, {"Code": "ServiceUnavailable"}
            if self.scenario.startswith("add-") and self.scenario != "add-uncertain":
                return self.fault_response("add")
            record_id = str(self.next_ali_id)
            self.next_ali_id += 1
            self.ali[record_id] = query["DomainName"].lower(), query["RR"].lower(), query["Value"]
            if self.scenario.startswith("collision-") and self.scenario != "collision-legacy-slots" and self.calls[action] == (8 if self.scenario == "collision-last-slot" else 2):
                if self.scenario == "collision-uncommitted": del self.ali[record_id]
                return 200, {"RecordId": "11", "RequestId": "fixture"}
            if self.scenario.startswith("flow-committed-"):
                return self.fault_response("flow-committed", record_id)
            if self.scenario in {"flow-response-lost", "flow-uncertain-error-oom"}:
                return None
            if self.scenario == "flow-diagnostic-oom":
                return 200, b"{"
            if self.scenario in {"uncertain-isolation", "capacity", "alias-uncertain", "alias-uncertain-upper"} and not self.faults:
                self.faults += 1
                return 200, b"{"
            if self.scenario == "add-uncertain":
                return None
            if self.scenario == "rpc-signing-reset" and self.calls[action] == 1: return None
            return 200, {"RecordId": record_id, "RequestId": "fixture"}
        if action == "DescribeDomainRecordInfo":
            assert set(query) == {"RecordId"}, "record read must target only the tracked id"
            record_id = query["RecordId"]
            self.record_reads.append(record_id)
            drops = {"read-repeat-once": 1, "read-delete-once": 1, "read-repeat-exhausted": 3,
                     "read-flow-exhausted": 3, "read-flow-limit": 9, "read-memory-wrap": 1}
            if len(self.record_reads) <= drops.get(self.scenario, 0): return None
            if self.scenario in {"record-repeat-missing", "record-delete-missing"} and len(self.record_reads) == 1:
                self.ali.pop(record_id)
                # An unrelated id with identical DNS data never becomes owned.
                self.ali["outside"] = "example.com", "_acme-challenge.api", "first_digest-01"
            if record_id not in self.ali:
                return 400, {"Code": "InvalidRR.NoExist" if self.live_absence else "DomainRecordNotBelongToUser", "RequestId": "fixture"}
            if self.scenario.endswith("-persistent") and len(self.record_reads) == 1:
                domain, rr, value = self.ali[record_id]
                self.ali[record_id] = domain, rr, value + "_outside"
            domain, rr, value = self.ali[record_id]
            response = {"RequestId": "fixture", "RecordId": record_id, "DomainName": domain.upper(),
                        "RR": rr.upper(), "Value": value, "Type": "TXT", "Line": "default", "Status": "Enable", "TTL": 600}
            if self.live_uppercase: response["Status"] = "ENABLE"
            if self.scenario.startswith("record-") and len(self.record_reads) == 1 and "-ack-" not in self.scenario and "-raced-" not in self.scenario:
                suffix = self.scenario.split("-", 2)[2]
                if suffix == "disabled": response["Status"] = "Disable"
                elif suffix == "malformed": return 200, b"{"
                elif suffix == "value": response["Value"] = value.upper()
                elif suffix == "owner": response["RR"] = "unrelated"
                elif suffix == "id": response["RecordId"] = "outside"
                elif suffix == "line": response["Line"] = "telecom"
                elif suffix == "type": response["Type"] = "CNAME"
                elif suffix == "status": response["Status"] = "unknown"
                elif suffix == "generic404": return 404, {}
                elif suffix in {"403missing", "503missing", "200missing"}:
                    return int(suffix[:3]), {"Code": "DomainRecordNotBelongToUser", "RequestId": "fixture"}
                elif suffix == "missing-request": return 400, {"Code": "DomainRecordNotBelongToUser"}
                elif suffix == "contradictory": return 400, {"Code": "DomainRecordNotBelongToUser", "RequestId": "fixture", "RecordId": record_id}
                elif suffix == "nul-code": return 400, {"Code": "DomainRecordNotBelongToUser\0extra", "RequestId": "fixture"}
            return 200, response
        assert action == "DeleteDomainRecord"
        record_id = query["RecordId"]
        assert record_id in self.ali, "delete does not belong to this provider"
        self.deletes.append(record_id)
        if self.scenario.startswith("record-delete-raced-"):
            del self.ali[record_id]
            return int(self.scenario.rsplit("-", 1)[1]), {"Code": "InvalidRR.NoExist" if self.live_absence else "DomainRecordNotBelongToUser", "RequestId": "fixture"}
        if self.scenario == "record-delete-ack-permission" and not self.faults:
            self.faults += 1
            return 403, {"Code": "Forbidden.RAM", "RequestId": "fixture"}
        if self.scenario.startswith("delete-committed-") and not self.faults:
            self.faults += 1
            del self.ali[record_id]
            return None if self.scenario.endswith("lost") else self.fault_response("delete-committed", record_id)
        if self.scenario.startswith("delete-") and self.scenario != "delete-parse-oom" and not self.faults:
            self.faults += 1
            return None if self.scenario == "delete-uncertain" else self.fault_response("delete", record_id)
        del self.ali[record_id]
        return 200, {"RecordId": record_id, "RequestId": "fixture"}

    def zone_fault(self, query):
        fixed = {"zone-forbidden": (403, {"Code": "Forbidden"}),
                 "zone-403-missing": (403, {"Code": "InvalidDomainName.NoExist"}),
                 "zone-generic-404": (404, {}),
                 "zone-server-503": (503, {"Code": "InvalidDomainName.NoExist"}),
                 "zone-throttle": (400, {"Code": "Throttling.User"}),
                 "zone-code": (200, {"Code": "Forbidden"}),
                 "zone-200-missing": (200, {"Code": "DomainNotFound"}),
                 "zone-nul-code": (400, {"Code": "InvalidDomainName.NoExist\0extra"}),
                 "zone-malformed": (200, b"{"), "zone-empty": (200, b""),
                 "zone-nonobject": (200, b"[]"),
                 "zone-wrong-shape": (200, {"DomainRecords": {"Record": "invalid"}, "TotalCount": -1}),
                 "zone-duplicate": (200, b'{"TotalCount":0,"TotalCount":0,"PageNumber":1,"PageSize":1,"RequestId":"fixture","DomainRecords":{"Record":[]}}')}
        if self.scenario in fixed:
            return fixed[self.scenario]
        page = {"TotalCount": 0, "PageSize": int(query["PageSize"]), "PageNumber": 1,
                "RequestId": "fixture", "DomainRecords": {"Record": []}}
        if self.scenario == "zone-total-string": page["TotalCount"] = "0"
        elif self.scenario == "zone-total-negative": page["TotalCount"] = -1
        elif self.scenario == "zone-total-float": page["TotalCount"] = 0.0
        elif self.scenario == "zone-wrong-page": page["PageNumber"] = 2
        elif self.scenario == "zone-wrong-size": page["PageSize"] = 20
        elif self.scenario == "zone-count-mismatch": page["TotalCount"] = 1
        elif self.scenario == "zone-nul-request": page["RequestId"] = "fixture\0extra"
        elif self.scenario == "zone-missing-request": del page["RequestId"]
        elif self.scenario == "zone-record-shape": page["DomainRecords"]["Record"] = "invalid"
        elif self.scenario in {"zone-record-domain", "zone-record-id"}:
            page["TotalCount"] = 1
            page["DomainRecords"]["Record"] = [{"DomainName": "other.com" if self.scenario == "zone-record-domain" else query["DomainName"],
                                               "RecordId": "123" if self.scenario == "zone-record-domain" else "bad&id"}]
        else: assert self.scenario == "zone-memory", self.scenario
        return 200, page

    def fault_response(self, operation, record_id="11"):
        suffix = self.scenario.removeprefix(operation + "-")
        if suffix in {"500", "503", "599"}:
            return int(suffix), {"Code": "ServiceUnavailable"}
        if suffix == "404":
            return 404, {"Code": "Forbidden", "Message": "route or authorization failure"}
        if suffix == "empty":
            return 200, b""
        if suffix == "malformed":
            return 200, b"{"
        if suffix == "missing-id":
            return 200, {"RequestId": "fixture"}
        if suffix == "wrong-id":
            return 200, {"RecordId": "unrelated-record"}
        if suffix == "code":
            return 200, {"RecordId": record_id, "Code": "Forbidden"}
        if suffix == "nul-id":
            return 200, {"RecordId": record_id + "\0extra"}
        raise AssertionError("unknown fault scenario " + suffix)

    def aws_request(self, method, path, query, body):
        if path == "/2013-04-01/hostedzonesbyname":
            assert method == "GET" and not body and query["maxitems"] == "100"
            self.calls["zone"] += 1
            zone = query["dnsname"].lower()
            self.zone_queries.append(zone)
            if self.scenario == "aws-alias-concurrent-same" and self.calls["zone"] == 1:
                time.sleep(0.2)
            entry = (f"<HostedZone><Id>/hostedzone/ZTEST</Id><Name>{zone}</Name>"
                     "<Config><PrivateZone>false</PrivateZone></Config></HostedZone>") if zone in {"example.com.", "example.org."} else ""
            return 200, (f"<ListHostedZonesByNameResponse><HostedZones>{entry}</HostedZones>"
                         "<IsTruncated>false</IsTruncated></ListHostedZonesByNameResponse>").encode()
        assert path.rstrip("/") == "/2013-04-01/hostedzone/ZTEST/rrset"
        return self.aws_rrset(method, query, body)

    def aws_rrset(self, method, query, body):
        if method == "GET":
            self.calls["read"] += 1
            assert query["maxitems"] == "1" and query["type"] == "TXT" and not body
            if self.scenario == "read-malformed":
                return 200, b"invalid XML"
            if self.scenario == "read-forbidden":
                return 403, b"<ErrorResponse><Code>AccessDenied</Code></ErrorResponse>"
            owner = query["name"].removesuffix(".").lower()
            ttl, values = self.aws.get(owner, (721, []))
            entry = self.record_xml(owner, ttl, values) if values else ""
            response = (f"<ListResourceRecordSetsResponse><ResourceRecordSets>{entry}</ResourceRecordSets>"
                        "<IsTruncated>false</IsTruncated></ListResourceRecordSetsResponse>").encode()
            if self.scenario == "existing-escaped":
                response = response.replace(b'"first_digest-01"', b'&#x22;first_digest-01&quot;')
            return 200, response
        assert method == "POST"
        self.calls["change"] += 1
        tree = ET.fromstring(body)
        for element in tree.iter():
            element.tag = element.tag.split("}")[-1]
        changes = tree.findall("./ChangeBatch/Changes/Change")
        assert 1 <= len(changes) <= 2
        owner = changes[0].findtext("./ResourceRecordSet/Name").removesuffix(".").lower()
        if self.scenario == "conflict-same-value" and not self.faults:
            self.faults += 1
            self.aws[owner][1].append('"first_digest-01"')
            return 400, b"<ErrorResponse><Error><Code>InvalidChangeBatch</Code></Error></ErrorResponse>"
        if self.scenario == "write-forbidden" or (self.scenario == "delete-forbidden" and self.calls["change"] == 2):
            return 403, b"<ErrorResponse><Error><Code>AccessDenied</Code></Error></ErrorResponse>"
        if self.scenario == "ack-empty":
            return 200, b""
        if self.scenario == "ack-malformed":
            return 200, b"<ChangeResourceRecordSetsResponse><ChangeInfo><Status>PENDING</Status></ChangeInfo></ChangeResourceRecordSetsResponse>"
        if self.scenario in {"conflict", "conflict-limit"} and (not self.faults or self.scenario == "conflict-limit"):
            self.faults += 1
            ttl, values = self.aws[owner]
            self.aws[owner] = ttl, values + [f'"outside-{self.faults}"']
            return 400, b"<ErrorResponse><Error><Code>InvalidChangeBatch</Code></Error></ErrorResponse>"
        drop = ((self.scenario in {"uncertain-add", "uncertain-error-oom", "uncertain-isolation",
                                  "aws-alias-uncertain", "aws-alias-uncertain-upper"} and self.calls["change"] == 1) or
                (self.scenario == "uncertain-remove" and self.calls["change"] == 2) or
                (self.scenario == "uncertain-uncommitted" and self.calls["change"] == 1))
        if drop and self.scenario == "uncertain-uncommitted":
            return None
        if ((self.scenario == "write-uncommitted-503" and self.calls["change"] == 1) or
            (self.scenario == "delete-uncommitted-503" and self.calls["change"] == 2)):
            return 503, b"<ErrorResponse><Error><Code>ServiceUnavailable</Code></Error></ErrorResponse>"
        for change in changes:
            record = change.find("ResourceRecordSet")
            assert record.findtext("Name").removesuffix(".").lower() == owner
            assert record.findtext("Type") == "TXT"
            ttl = int(record.findtext("TTL"))
            values = [value.text for value in record.findall("./ResourceRecords/ResourceRecord/Value")]
            assert len(values) == len(set(values)) and ttl == self.aws[owner][0]
            if change.findtext("Action") == "DELETE":
                assert (ttl, values) == self.aws[owner], "DELETE does not match current record set"
                self.aws[owner] = ttl, []
            else:
                assert change.findtext("Action") == "CREATE" and not self.aws[owner][1]
                self.aws[owner] = ttl, values
        if drop:
            return None
        if ((self.scenario == "write-committed-503" and self.calls["change"] == 1) or
            (self.scenario == "delete-committed-503" and self.calls["change"] == 2)):
            return 503, b"<ErrorResponse><Error><Code>ServiceUnavailable</Code></Error></ErrorResponse>"
        if self.scenario == "ack-committed" and self.calls["change"] == 1:
            return 200, b"<ErrorResponse><Code>bogus-success</Code></ErrorResponse>"
        return 200, (b'<?xml version="1.0" encoding="UTF-8"?>\n'
                     b'<ChangeResourceRecordSetsResponse xmlns="https://route53.amazonaws.com/doc/2013-04-01/">'
                     b'<ChangeInfo><Comment>fixture &amp; signed</Comment><Id>/change/fixture</Id>'
                     b'<Status>PENDING</Status><SubmittedAt>2026-10-01T00:00:00Z</SubmittedAt>'
                     b'</ChangeInfo></ChangeResourceRecordSetsResponse>')

    @staticmethod
    def record_xml(owner, ttl, values):
        records = "".join(f"<ResourceRecord><Value>{escape(value)}</Value></ResourceRecord>" for value in values)
        return (f"<ResourceRecordSet><Name>{owner}.</Name><Type>TXT</Type><TTL>{ttl}</TTL>"
                f"<ResourceRecords>{records}</ResourceRecords></ResourceRecordSet>")

    def verify(self):
        if self.provider == "aws":
            discoveries = {"multi": 3, "cross-instance": 2, "capacity": 9,
                           "existing": 2, "existing-escaped": 2, "conflict-same-value": 2,
                           "uncertain-isolation": 2, "aws-alias-borrowed": 2,
                           "aws-alias-uncertain": 2, "aws-alias-uncertain-upper": 2,
                           "aws-alias-capacity": 8}.get(self.scenario, 1)
            owners = [OWNER] * discoveries
            if self.scenario == "multi":
                owners[-1] = OTHER
            expected_queries = [name for owner in owners for name in
                                (owner + ".", owner.split(".", 1)[1] + ".", owner.split(".", 2)[2] + ".")]
            assert self.zone_queries == expected_queries, self.zone_queries
            assert self.calls["zone"] == len(expected_queries), self.calls
        if self.provider == "ali":
            if self.scenario == "rpc-signing-reset":
                assert self.calls == {"DescribeDomainRecords": 2, "AddDomainRecord": 2,
                                      "DescribeDomainRecordInfo": 1, "DeleteDomainRecord": 1}, self.calls
                assert self.record_reads == ["12"] and self.deletes == ["12"]
                assert self.ali == {"11": ("example.com", "_acme-challenge.api", "first_digest-01")}
                return
            if self.scenario.startswith("read-"):
                reads = {"read-repeat-once": 3, "read-delete-once": 2, "read-repeat-exhausted": 5,
                         "read-flow-exhausted": 5, "read-flow-limit": 10, "read-memory-wrap": 2}[self.scenario]
                assert self.calls == {"DescribeDomainRecords": 2, "AddDomainRecord": 1,
                                      "DescribeDomainRecordInfo": reads, "DeleteDomainRecord": 1}, self.calls
                assert self.record_reads == ["11"] * reads and self.deletes == ["11"] and not self.ali
                return
            if self.scenario.startswith("collision-"):
                last = self.scenario == "collision-last-slot"
                legacy = self.scenario == "collision-legacy-slots"
                reads = [str(i) for i in range(11, 18)] if last else ["11", "11", "12"] if legacy else ["11", "13"]
                deletes = reads if last else ["11", "12"] if legacy else ["11", "13"]
                assert self.calls == {"DescribeDomainRecords": 2, "AddDomainRecord": 8 if last else 2 if legacy else 3,
                                      "DescribeDomainRecordInfo": len(reads), "DeleteDomainRecord": len(deletes)}, self.calls
                assert self.record_reads == reads and self.deletes == deletes
                assert self.ali == ({"18": ("example.com", "_acme-challenge.api", "digest_7")} if last else
                                    {} if legacy or self.scenario == "collision-uncommitted" else
                                    {"12": ("example.com", "_acme-challenge.api", "second_digest-02")}), self.ali
                return
            # Account for every read independently; checking only mutation totals
            # would let an unverified cache hit or broad lookup pass unnoticed.
            if self.scenario.startswith("record-"):
                missing = self.scenario.endswith("-missing")
                repeat = self.scenario.startswith("record-repeat-")
                expected_reads = ["11", "12"] if missing and repeat else ["11"] if missing or "-raced-" in self.scenario or self.scenario == "record-delete-disabled" else ["11", "11"]
                assert self.record_reads == expected_reads, self.record_reads
                persistent = self.scenario.endswith("-persistent")
                expected_deletes = [] if persistent or missing and not repeat else ["12"] if missing else ["11", "11"] if "-ack-" in self.scenario else ["11"]
                assert self.deletes == expected_deletes, self.deletes
                assert self.calls == {"DescribeDomainRecords": 2, "AddDomainRecord": 2 if missing and repeat else 1,
                                      "DescribeDomainRecordInfo": len(expected_reads),
                                      **({"DeleteDomainRecord": len(expected_deletes)} if expected_deletes else {})}, self.calls
                assert self.ali == ({"outside": ("example.com", "_acme-challenge.api", "first_digest-01")} if missing else
                                    {"11": ("example.com", "_acme-challenge.api", "first_digest-01_outside")} if persistent else {})
                return
            if self.scenario.startswith("alias-"):
                expected_reads = ["12"] if "uncertain" in self.scenario else ["11"] * 3 if self.scenario == "alias-repeat-owned" else ["11"] + [str(i) for i in range(11, 19)] if self.scenario == "alias-capacity-owned" else ["11"] * 8 if self.scenario == "alias-concurrent-same" else ["11"]
            elif self.scenario.startswith("discovery-"):
                count = {"discovery-cache-capacity": 8, "discovery-specific-zone": 2, "discovery-case-key": 2, "discovery-no-zone": 0}.get(self.scenario, 1)
                expected_reads = [str(i) for i in range(11, 11 + count)]
            elif self.scenario.startswith("zone-"): expected_reads = ["11"]
            elif self.scenario == "capacity": expected_reads = ["12", "19"] + [str(i) for i in range(13, 19)]
            elif self.scenario == "uncertain-isolation": expected_reads = ["12"]
            elif self.scenario == "concurrent": expected_reads = self.deletes
            elif self.scenario.startswith("delete-"): expected_reads = ["11", "11"]
            elif self.scenario.startswith(("flow-", "add-")) or self.scenario == "invalid-input": expected_reads = []
            else: expected_reads = ["11", "13", "12"]
            assert self.record_reads == expected_reads, self.record_reads
            assert self.calls["DescribeDomainRecordInfo"] == len(expected_reads)
            # Existing scenario contracts below still check their original RPCs.
            calls = self.calls.copy()
            del calls["DescribeDomainRecordInfo"]
            if self.scenario.startswith("alias-"):
                uncertain = self.scenario in {"alias-uncertain", "alias-uncertain-upper"}
                writes = 2 if uncertain else 8 if self.scenario == "alias-capacity-owned" else 1
                deletes = writes - 1 if uncertain else writes
                assert calls == {"DescribeDomainRecords": 2, "AddDomainRecord": writes, "DeleteDomainRecord": deletes}, calls
                assert self.deletes == [str(i) for i in range(12 if uncertain else 11, 11 + writes)]
                assert self.ali == ({"11": ("example.com", "_acme-challenge.api", "first_digest-01")} if uncertain else {})
            elif self.scenario.startswith("zone-"):
                child_faults = {"zone-forbidden", "zone-403-missing", "zone-generic-404", "zone-server-503", "zone-throttle"}
                assert calls["DescribeDomainRecords"] == (3 if self.scenario in child_faults else 4)
                assert calls["AddDomainRecord"] == 1 and self.deletes == ["11"] and not self.ali
            elif self.scenario.startswith("discovery-"):
                expected = {"discovery-cache-capacity": (14, 8), "discovery-specific-zone": (3, 2),
                            "discovery-case-key": (2, 2), "discovery-no-zone": (4, 0), "discovery-long-owner": (4, 1)}
                reads, writes = expected.get(self.scenario, (2, 1))
                assert calls == {"DescribeDomainRecords": reads, **({"AddDomainRecord": writes, "DeleteDomainRecord": writes} if writes else {})}, calls
                assert len(self.deletes) == writes and not self.ali
            elif self.scenario in {"uncertain-isolation", "capacity"}:
                assert calls["DescribeDomainRecords"] == 2
                assert calls["AddDomainRecord"] == (9 if self.scenario == "capacity" else 2)
                assert len(self.deletes) == (8 if self.scenario == "capacity" else 1)
                assert self.ali == {"11": ("example.com", "_acme-challenge.api", "first_digest-01")}
            elif self.scenario == "concurrent":
                assert calls["DescribeDomainRecords"] == 2 and calls["AddDomainRecord"] == 8
                assert set(self.deletes) == {str(i) for i in range(11, 19)} and len(self.deletes) == 8 and not self.ali
            elif self.scenario == "invalid-input":
                assert not calls and not self.ali
            elif self.scenario == "delete-parse-oom":
                assert calls["AddDomainRecord"] == 1 and self.deletes == ["11"] and not self.ali
            elif self.scenario.startswith("flow-"):
                assert calls["DescribeDomainRecords"] == 2
                assert calls["AddDomainRecord"] == 1 and len(self.ali) == (0 if self.scenario == "flow-uncommitted-503" else 1) and not self.deletes
            elif self.scenario == "add-uncertain":
                assert calls["AddDomainRecord"] == 1 and len(self.ali) == 1 and not self.deletes
            elif self.scenario.startswith("add-"):
                assert calls["AddDomainRecord"] == 1 and not self.ali and not self.deletes
            elif self.scenario.startswith("delete-committed-"):
                assert self.deletes == ["11"] and not self.ali
            elif self.scenario.startswith("delete-"):
                assert self.deletes == ["11", "11"] and not self.ali
            else:
                assert calls["AddDomainRecord"] == 3 and self.deletes == ["11", "13", "12"] and not self.ali
        elif self.scenario.startswith("aws-alias-"):
            zones, reads, changes = {
                "aws-alias-owned": (3, 2, 2),
                "aws-alias-repeat-owned": (3, 3, 2),
                "aws-alias-borrowed": (6, 2, 0),
                "aws-alias-uncertain": (6, 3, 3),
                "aws-alias-uncertain-upper": (6, 3, 3),
                "aws-alias-capacity": (24, 17, 16),
                "aws-alias-concurrent-same": (3, 9, 2),
            }[self.scenario]
            assert self.calls == {"zone": zones, "read": reads,
                                  **({"change": changes} if changes else {})}, self.calls
            values = [MANUAL, '\"first_digest-01\"'] if self.scenario in {
                "aws-alias-borrowed", "aws-alias-uncertain", "aws-alias-uncertain-upper"} else [MANUAL]
            assert self.aws == {OWNER: (721, values), OTHER: (913, [MANUAL])}, self.aws
        elif self.scenario == "conflict-limit":
            assert self.calls["change"] == 4 and self.calls["read"] == 4
            assert self.aws[OWNER] == (721, [MANUAL] + [f'"outside-{i}"' for i in range(1, 5)])
        elif self.scenario.startswith("read-"):
            assert not self.calls["change"] and self.calls["read"] == 1
        elif self.scenario in {"ack-empty", "ack-malformed", "write-forbidden", "uncertain-uncommitted", "write-uncommitted-503"}:
            assert self.calls["change"] == 1 and self.calls["read"] == 1
            assert self.aws == {OWNER: (721, [MANUAL]), OTHER: (913, [MANUAL])}
        elif self.scenario in {"uncertain-add", "ack-committed", "uncertain-error-oom", "uncertain-isolation", "write-committed-503"}:
            expected = 3 if self.scenario == "uncertain-isolation" else 1
            assert self.calls["change"] == expected and self.calls["read"] == expected
            assert self.aws == {OWNER: (721, [MANUAL, '"first_digest-01"']), OTHER: (913, [MANUAL])}
        elif self.scenario in {"existing", "existing-escaped", "conflict-same-value"}:
            assert self.calls["change"] == (1 if self.scenario == "conflict-same-value" else 0)
            assert self.calls["read"] == (3 if self.scenario == "conflict-same-value" else 2)
            assert self.aws == {OWNER: (721, [MANUAL, '"first_digest-01"']), OTHER: (913, [MANUAL])}
        elif self.scenario == "capacity":
            assert self.calls["change"] == 18 and self.calls["read"] == 19
            assert self.aws == {OWNER: (721, [MANUAL]), OTHER: (913, [MANUAL])}
        else:
            expected = [MANUAL, '"outside-1"'] if self.scenario == "conflict" else [MANUAL]
            assert self.aws == {OWNER: (721, expected), OTHER: (913, [MANUAL])}
            assert self.calls["change"] == (6 if self.scenario == "multi" else
                                           3 if self.scenario in {"conflict", "uncertain-uncommitted", "delete-forbidden", "delete-uncommitted-503"} else 2)
            assert self.calls["read"] == (6 if self.scenario == "multi" else
                                         3 if self.scenario in {"conflict", "uncertain-remove", "delete-forbidden",
                                                               "repeat-owned", "cross-instance", "delete-committed-503", "delete-uncommitted-503"} else 2)


def aws_error(code, message="fixture", extra=""):
    return (f'<ErrorResponse xmlns="https://route53.amazonaws.com/doc/2013-04-01/">'
            f'<Error><Type>Sender</Type><Code>{code}</Code><Message>{message}</Message>'
            f'{extra}</Error><RequestId>fixture</RequestId></ErrorResponse>').encode()


AWS_ERROR_BODIES = {
    "message": aws_error("InvalidInput", "InvalidChangeBatch"),
    "comment": aws_error("InvalidInput", extra="<!-- InvalidChangeBatch -->"),
    "prefix": aws_error("InvalidChangeBatchExtra"),
    "nested": b"<ErrorResponse><Error><Message><Code>InvalidChangeBatch</Code></Message></Error></ErrorResponse>",
    "truncated": aws_error("InvalidChangeBatch")[:-16],
    "duplicate-code": aws_error("InvalidInput", extra="<Code>InvalidChangeBatch</Code>"),
    "duplicate-error": aws_error("InvalidChangeBatch").replace(b"</Error>", b"</Error><Error><Code>InvalidInput</Code></Error>"),
    "trailing": aws_error("InvalidChangeBatch") + b"garbage",
    "nul": aws_error("InvalidChangeBatch") + b"\0",
    "doctype": b'<!DOCTYPE ErrorResponse [<!ENTITY e "InvalidChangeBatch">]>' + aws_error("InvalidInput"),
    "entity": aws_error("InvalidChangeBatch", "&unknown;"),
    "namespace": aws_error("InvalidChangeBatch").replace(b"https://route53.amazonaws.com/doc/2013-04-01/", b"urn:untrusted"),
    "empty": b"",
    "access-denied": aws_error("AccessDenied"),
    "invalid-token": aws_error("InvalidClientTokenId"),
    "signature": aws_error("SignatureDoesNotMatch"),
}


class AwsErrorService(Service):
    """Errors are classified by their envelope, never text found elsewhere."""

    def __init__(self, provider, scenario):
        super().__init__(provider, scenario)
        self.write_times = []

    def aws_rrset(self, method, query, body):
        if method == "GET":
            return super().aws_rrset(method, query, body)
        self.write_times.append(time.monotonic())
        suffix = self.scenario.removeprefix("aws-error-")
        if suffix in AWS_ERROR_BODIES:
            self.calls["change"] += 1
            return 400, AWS_ERROR_BODIES[suffix]
        if suffix == "status-mismatch":
            self.calls["change"] += 1
            return 403, aws_error("Throttling")
        if suffix in {"delete-committed-malformed", "delete-uncommitted-malformed"}:
            if len(self.write_times) == 2:
                if suffix == "delete-uncommitted-malformed": self.calls["change"] += 1
                else: super().aws_rrset(method, query, body)
                return 400, b"<ErrorResponse><Error><Code>InvalidChangeBatch</Code>"
            return super().aws_rrset(method, query, body)
        operation, code, outcome = suffix.split("-")
        deleting = operation == "delete"
        code = "Throttling" if code == "throttle" else "PriorRequestNotComplete"
        # Add succeeds before the first Delete rejection. Rejections are atomic;
        # an independent writer changes TXT between attempts to expose stale replay.
        active = not deleting or len(self.write_times) > 1
        reject_count = 4 if outcome == "limit" else 3 if outcome == "three" else 1
        if active and self.faults < reject_count:
            self.faults += 1
            self.calls["change"] += 1
            self.aws[OWNER][1].append(f'"outside-{self.faults}"')
            return 400, aws_error(code)
        return super().aws_rrset(method, query, body)

    def verify(self):
        assert self.zone_queries == [OWNER + ".", "api.example.com.", "example.com."], self.zone_queries
        suffix = self.scenario.removeprefix("aws-error-")
        if suffix in AWS_ERROR_BODIES or suffix == "status-mismatch":
            assert self.calls == {"zone": 3, "read": 1, "change": 1}, self.calls
            assert self.aws == {OWNER: (721, [MANUAL]), OTHER: (913, [MANUAL])}, self.aws
            return
        if suffix in {"delete-committed-malformed", "delete-uncommitted-malformed"}:
            changes = 2 if suffix == "delete-committed-malformed" else 3
            assert self.calls == {"zone": 3, "read": 3, "change": changes}, self.calls
            assert self.aws == {OWNER: (721, [MANUAL]), OTHER: (913, [MANUAL])}, self.aws
            if changes == 3: assert self.write_times[2] - self.write_times[1] >= 0.45, self.write_times
            return
        operation, _, outcome = suffix.split("-")
        count = 4 if outcome in {"limit", "three"} else 2
        deleting = operation == "delete"
        writes = count + int(deleting or outcome != "limit")
        assert self.calls == {"zone": 3, "read": writes, "change": writes}, self.calls
        first = 1 if deleting else 0
        for i in range(first + 1, first + count):
            assert self.write_times[i] - self.write_times[i - 1] >= (0.45 * 2 ** (i - first - 1)), self.write_times
        values = [MANUAL] + [f'"outside-{i}"' for i in range(1, self.faults + 1)]
        if deleting and outcome == "limit": values.insert(1, '"first_digest-01"')
        assert self.aws == {OWNER: (721, values), OTHER: (913, [MANUAL])}, self.aws


class AwsZoneService(Service):
    """Keep each zone's records separate, including identical DNS owner names."""

    def __init__(self, provider, scenario):
        super().__init__(provider, scenario)
        self.zone_queries = []
        self.record_targets = []
        self.first = "_acme-challenge.first.example.com"
        self.zone_records = {
            "ZTEST": {OWNER: (721, [MANUAL]), OTHER: (913, [MANUAL]), self.first: (733, [MANUAL])},
            "ZCHILD": {OWNER: (887, [MANUAL])},
            "ZAPEX": {OWNER: (947, [MANUAL])},
            "ZNEW": {OWNER: (991, [MANUAL])},
        }
        if scenario == "aws-zone-borrowed-refresh":
            self.zone_records["ZTEST"][OWNER][1].append('"first_digest-01"')
        self.restored = False

    def aws_request(self, method, path, query, body):
        if path == "/2013-04-01/hostedzonesbyname":
            assert method == "GET" and not body and query["maxitems"] == "100"
            zone = query["dnsname"]
            assert zone == zone.lower() and zone.endswith(".")
            self.calls["zone"] += 1
            self.zone_queries.append(zone)
            child = self.scenario == "aws-zone-parent-child" or self.scenario.startswith("aws-zone-child-") or self.calls["change"] > 0
            if self.scenario == "aws-zone-borrowed-refresh":
                child = self.calls["read"] > 0
            entry, truncated = "", "false"
            if self.scenario.startswith("aws-zone-no-zone"):
                pass
            elif zone == OWNER + "." and self.scenario == "aws-zone-owner-apex":
                entry = self.zone_xml("ZAPEX", zone)
            elif zone == "api.example.com." and child:
                fault = self.scenario.removeprefix("aws-zone-child-")
                if fault in {"forbidden", "malformed", "ambiguous", "truncated"} and not self.faults:
                    self.faults += 1
                    if fault == "forbidden":
                        return 403, b"<ErrorResponse><Error><Code>AccessDenied</Code></Error></ErrorResponse>"
                    if fault == "malformed":
                        return 200, b"invalid XML"
                    entry = self.zone_xml("ZCHILD", zone)
                    if fault == "ambiguous":
                        entry += self.zone_xml("ZNEW", zone)
                    else:
                        truncated = "true"
                else:
                    zone_id = "ZNEW" if fault == "replaced" and self.calls["change"] > 0 else "ZCHILD"
                    entry = self.zone_xml(zone_id, zone, private=fault == "private")
            elif zone in {"example.com.", "example.org."}:
                entry = self.zone_xml("ZTEST", zone)
            next_page = f"<NextDNSName>{zone}</NextDNSName><NextHostedZoneId>ZNEW</NextHostedZoneId>" if truncated == "true" else ""
            return 200, (f"<ListHostedZonesByNameResponse><HostedZones>{entry}</HostedZones>"
                         f"<IsTruncated>{truncated}</IsTruncated>{next_page}</ListHostedZonesByNameResponse>").encode()
        match = re.fullmatch(r"/2013-04-01/hostedzone/(ZTEST|ZCHILD|ZAPEX|ZNEW)/rrset/?", path)
        assert match, path
        zone_id = match[1]
        owner = query["name"].removesuffix(".") if method == "GET" else self.change_owner(body)
        assert owner == owner.lower() and owner in self.zone_records[zone_id], (zone_id, owner)
        self.record_targets.append((method, zone_id, owner))
        if self.scenario == "aws-zone-owned-restore" and self.calls["change"] == 1 and not self.restored:
            self.zone_records["ZTEST"][OWNER] = (721, [MANUAL])  # Outside writer removed our value.
            self.restored = True
        previous = self.aws
        self.aws = self.zone_records[zone_id]
        try:
            return self.aws_rrset(method, query, body)
        finally:
            self.aws = previous

    @staticmethod
    def zone_xml(zone_id, name, private=False):
        return (f"<HostedZone><Id>/hostedzone/{zone_id}</Id><Name>{name}</Name>"
                f"<Config><PrivateZone>{str(private).lower()}</PrivateZone></Config></HostedZone>")

    @staticmethod
    def change_owner(body):
        tree = ET.fromstring(body)
        for element in tree.iter():
            element.tag = element.tag.split("}")[-1]
        return tree.findtext("./ChangeBatch/Changes/Change/ResourceRecordSet/Name").removesuffix(".")

    def verify(self):
        name = OWNER + "."
        fault = self.scenario.removeprefix("aws-zone-child-")
        siblings = self.scenario == "aws-zone-parent-child" or self.scenario.startswith("aws-zone-child-") and fault != "replaced"
        if self.scenario.startswith("aws-zone-no-zone"):
            queries, targets = [name, "api.example.com.", "example.com.", "com."], []
        elif self.scenario == "aws-zone-owner-apex":
            queries = [name]
            targets = [(m, "ZAPEX", OWNER) for m in ("GET", "POST", "GET", "POST")]
        elif self.scenario == "aws-zone-borrowed-refresh":
            queries = [name, "api.example.com.", "example.com.", name, "api.example.com."]
            targets = [("GET", "ZTEST", OWNER)] + [(m, "ZCHILD", OWNER) for m in ("GET", "POST", "GET", "POST")]
        else:
            first_owner = self.first if siblings else OWNER
            first_id = "ZCHILD" if fault == "replaced" else "ZTEST"
            second_id = "ZNEW" if fault == "replaced" else "ZTEST" if fault == "private" else "ZCHILD"
            queries = ([self.first + ".", "first.example.com.", "example.com."] if siblings else
                       [name, "api.example.com."] + ([] if fault == "replaced" else ["example.com."]))
            queries += [name, "api.example.com."]
            if fault in {"forbidden", "malformed", "ambiguous", "truncated"}:
                queries += [name, "api.example.com."]
            elif fault == "private":
                queries += ["example.com."]
            targets = [(m, first_id, first_owner) for m in ("GET", "POST")]
            if self.scenario in {"aws-zone-owned-pinned", "aws-zone-owned-restore"}:
                targets.append(("GET", first_id, first_owner))
                if self.scenario.endswith("restore"):
                    targets.append(("POST", first_id, first_owner))
            targets += [(m, second_id, OWNER) for m in ("GET", "POST")]
            targets += [(m, first_id, first_owner) for m in ("GET", "POST")]
            targets += [(m, second_id, OWNER) for m in ("GET", "POST")]
        assert self.zone_queries == queries, self.zone_queries
        assert self.record_targets == targets, self.record_targets
        assert self.calls == {"zone": len(queries), **({"read": sum(m == "GET" for m, _, _ in targets),
                             "change": sum(m == "POST" for m, _, _ in targets)} if targets else {})}, self.calls
        expected = {
            "ZTEST": {OWNER: (721, [MANUAL]), OTHER: (913, [MANUAL]), self.first: (733, [MANUAL])},
            "ZCHILD": {OWNER: (887, [MANUAL])}, "ZAPEX": {OWNER: (947, [MANUAL])}, "ZNEW": {OWNER: (991, [MANUAL])},
        }
        if self.scenario == "aws-zone-borrowed-refresh":
            expected["ZTEST"][OWNER][1].append('"first_digest-01"')
        assert self.zone_records == expected, self.zone_records


SCENARIOS = [("ali", "multi", "multi"), ("ali", "add-uncertain", "ali-add-uncertain"),
             ("ali", "delete-uncertain", "delete-uncertain")]
SCENARIOS += [("ali", "live-uppercase", "multi")]
SCENARIOS += [("ali", "live-absence-" + scenario, scenario) for scenario in
              ("record-repeat-missing", "record-delete-missing", "record-delete-raced-400", "record-delete-raced-404")]
SCENARIOS += [("ali", "add-" + fault, "ali-add-uncertain") for fault in ("malformed", "missing-id", "code", "nul-id")]
SCENARIOS += [("ali", "delete-" + fault, "delete-failure") for fault in
              ("404", "empty", "malformed", "missing-id", "wrong-id", "code", "nul-id")]
SCENARIOS += [("ali", "flow-committed-" + fault, "flow-committed-" + fault)
              for fault in ("malformed", "missing-id", "empty", "code", "nul-id", "500", "503", "599")]
SCENARIOS += [("ali", scenario, scenario) for scenario in
              ("flow-response-lost", "flow-uncommitted-503", "flow-parse-oom", "flow-uncertain-error-oom", "flow-diagnostic-oom",
               "uncertain-isolation", "capacity", "concurrent", "invalid-input", "delete-parse-oom")]
SCENARIOS += [("ali", scenario, "zone-credential" if scenario in {"zone-forbidden", "zone-code"} else "zone-protocol")
              for scenario in ("zone-forbidden", "zone-malformed", "zone-code", "zone-wrong-shape")]
SCENARIOS += [("ali", scenario, "zone-credential" if scenario == "zone-403-missing" else
               "zone-network" if scenario == "zone-server-503" else "zone-busy" if scenario == "zone-throttle" else
               "zone-memory" if scenario == "zone-memory" else "zone-protocol")
              for scenario in ("zone-403-missing", "zone-generic-404", "zone-server-503", "zone-throttle", "zone-200-missing",
                               "zone-nul-code", "zone-empty", "zone-nonobject", "zone-total-string", "zone-total-negative",
                               "zone-total-float", "zone-wrong-page", "zone-wrong-size", "zone-count-mismatch", "zone-nul-request",
                               "zone-missing-request", "zone-record-shape", "zone-record-domain", "zone-record-id", "zone-duplicate", "zone-memory")]
SCENARIOS += [("ali", scenario, scenario if scenario in {"discovery-cache-capacity", "discovery-specific-zone", "discovery-case-key", "discovery-no-zone"} else "normal")
              for scenario in ("discovery-cache-capacity", "discovery-specific-zone", "discovery-case-key", "discovery-no-zone",
                               "discovery-full-page", "discovery-missing-alias")]
SCENARIOS += [("ali", "discovery-long-owner", "discovery-long-owner")]
SCENARIOS += [("ali", scenario, scenario) for scenario in
              ("alias-uncertain", "alias-uncertain-upper", "alias-owned", "alias-repeat-owned",
               "alias-capacity-owned", "alias-concurrent-same")]
SCENARIOS += [("ali", scenario, scenario) for scenario in
              ("collision-id", "collision-uncommitted", "collision-last-slot", "collision-legacy-slots", "collision-error-oom")]
SCENARIOS += [("ali", scenario, scenario) for scenario in
              ("read-repeat-once", "read-delete-once", "read-repeat-exhausted", "read-flow-exhausted", "read-flow-limit", "read-memory-wrap")]
SCENARIOS += [("ali", "rpc-signing-reset", "rpc-signing-reset")]
SCENARIOS += [("ali", "record-" + operation + "-" + suffix,
               "record-" + operation + "-failure-" + ("permission" if suffix == "403missing" else "network" if suffix == "503missing" else suffix))
              for operation in ("repeat", "delete")
              for suffix in ("malformed", "value", "owner", "id", "line", "type", "status", "generic404",
                             "403missing", "503missing", "200missing", "missing-request", "contradictory", "nul-code")]
SCENARIOS += [("ali", scenario, scenario) for scenario in
              ("record-repeat-missing", "record-delete-missing", "record-repeat-disabled", "record-delete-disabled",
               "record-repeat-parse-oom", "record-delete-parse-oom", "record-delete-raced-400", "record-delete-raced-404")]
SCENARIOS += [("ali", "record-delete-ack-permission", "record-delete-failure-permission")]
SCENARIOS += [("ali", "record-" + operation + "-persistent", "record-" + operation + "-failure-persistent")
              for operation in ("repeat", "delete")]
SCENARIOS += [("ali", "delete-committed-" + suffix, "delete-uncertain" if suffix == "lost" else "delete-failure")
              for suffix in ("lost", "malformed", "503")]
SCENARIOS += [("aws", scenario, "multi" if scenario == "multi" else
               "delete-failure" if scenario == "delete-forbidden" else
               "add-uncertain" if scenario in {"uncertain-add", "uncertain-uncommitted", "ack-empty", "ack-malformed", "ack-committed"} else
               "add-failure" if scenario in {"conflict-limit", "read-malformed", "read-forbidden", "write-forbidden"} else "normal")
              for scenario in ("multi", "conflict", "conflict-limit", "uncertain-add", "uncertain-remove",
                               "uncertain-uncommitted", "read-malformed", "read-forbidden", "write-forbidden",
                               "delete-forbidden", "ack-empty", "ack-malformed", "ack-committed")]
SCENARIOS += [("aws", scenario, "add-uncertain" if scenario.startswith("write-") else "normal")
              for scenario in ("write-committed-503", "write-uncommitted-503", "delete-committed-503", "delete-uncommitted-503")]
SCENARIOS += [("aws", scenario, "borrowed" if scenario in {"existing", "existing-escaped", "conflict-same-value"}
               else "add-uncertain-oom" if scenario == "uncertain-error-oom" else scenario)
              for scenario in ("existing", "existing-escaped", "conflict-same-value", "repeat-owned",
                               "cross-instance", "capacity", "uncertain-error-oom", "uncertain-isolation")]
SCENARIOS += [("aws", scenario, scenario) for scenario in
              ("aws-alias-owned", "aws-alias-repeat-owned", "aws-alias-borrowed", "aws-alias-uncertain",
               "aws-alias-uncertain-upper", "aws-alias-capacity", "aws-alias-concurrent-same")]
SCENARIOS += [("aws", "aws-zone-" + suffix, "aws-zone-" + suffix) for suffix in
              ("parent-child", "owner-apex", "appears-new-pair", "owned-pinned", "owned-restore", "borrowed-refresh",
               "child-forbidden", "child-malformed", "child-ambiguous", "child-truncated", "child-private", "child-replaced", "no-zone", "no-zone-stale-error")]
SCENARIOS += [("aws", "aws-error-" + suffix, "aws-error-rejected" if suffix in
               {"message", "comment", "prefix"} else "aws-error-permission" if suffix in
               {"access-denied", "invalid-token", "signature"} else "aws-error-uncertain") for suffix in AWS_ERROR_BODIES]
SCENARIOS += [("aws", "aws-error-status-mismatch", "aws-error-rejected")]
SCENARIOS += [("aws", f"aws-error-{operation}-{code}-{outcome}",
               f"aws-error-{operation}-limit" if outcome == "limit" else "aws-error-recovered")
              for operation in ("add", "delete") for code in ("throttle", "prior") for outcome in ("once", "three", "limit")]
SCENARIOS += [("aws", "aws-error-delete-" + outcome + "-malformed", "aws-error-recovered")
              for outcome in ("committed", "uncommitted")]


def rest_signature(method, target, headers, body, provider):
    """Independent authentication oracle; derive everything from received bytes."""
    if provider == "cf":
        assert headers["authorization"] == "Bearer test-token"
        assert headers["content-type"] == "application/json"
        return
    algorithm, fields = headers["authorization"].split(" ", 1)
    fields = dict(part.strip().split("=", 1) for part in fields.split(","))
    path, _, query = target.partition("?")
    if provider == "tencent":
        assert algorithm == "TC3-HMAC-SHA256" and target == "/" and method == "POST"
        assert headers["content-type"] == "application/json; charset=utf-8"
        assert headers["x-tc-version"] == "2021-03-23"
        signed = "content-type;host;x-tc-action"
        stamp = headers["x-tc-timestamp"]
        now = datetime.datetime.fromtimestamp(int(stamp), datetime.timezone.utc)
        assert abs(time.time() - int(stamp)) < 300
        date = now.strftime("%Y-%m-%d")
        scope = date + "/dnspod/tc3_request"
        assert fields["Credential"] == "test-id/" + scope
        canonical_headers = (f'content-type:{headers["content-type"]}\nhost:{headers["host"]}\n'
                             f'x-tc-action:{headers["x-tc-action"].lower()}\n')
        canonical = "\n".join([method, path, "", canonical_headers, signed, digest(body)])
        string = "\n".join([algorithm, stamp, scope, digest(canonical.encode())])
        key = mac(mac(mac(b"TC3test-key", date), "dnspod"), "tc3_request")
    else:
        assert algorithm == "SDK-HMAC-SHA256" and fields["Access"] == "test-id"
        assert headers["content-type"] == "application/json"
        signed = "content-type;host;x-sdk-date"
        stamp = headers["x-sdk-date"]
        now = datetime.datetime.strptime(stamp, "%Y%m%dT%H%M%SZ").replace(tzinfo=datetime.timezone.utc)
        assert abs(time.time() - now.timestamp()) < 300
        pairs = urllib.parse.parse_qsl(query, keep_blank_values=True, strict_parsing=True)
        assert len(pairs) == len(dict(pairs))
        quote = lambda value: urllib.parse.quote(value, safe="-_.~")
        canonical_query = "&".join(sorted(f"{quote(k)}={quote(v)}" for k, v in pairs))
        canonical_headers = "".join(f"{name}:{headers[name]}\n" for name in signed.split(";"))
        canonical = "\n".join([method, path.rstrip("/") + "/", canonical_query,
                               canonical_headers, signed, digest(body)])
        string = "\n".join([algorithm, stamp, digest(canonical.encode())])
        key = b"test-key"
    assert fields["SignedHeaders"] == signed
    assert hmac.compare_digest(fields["Signature"], mac(key, string).hex()), "signature differs from independent oracle"


class RestCreateService:
    def __init__(self, provider, scenario):
        self.provider, self.scenario = provider, scenario
        self.calls = collections.Counter()
        self.records = {}
        self.submissions = []
        self.deleted = []

    def acknowledgement(self, record_id, owner, value):
        if self.provider == "cf":
            return {"success": True, "errors": [], "messages": [], "create-probe": True,
                    "result": {"id": record_id, "type": "TXT", "name": owner, "content": value, "ttl": 60}}
        if self.provider == "tencent":
            return {"Response": {"RecordId": int(record_id), "RequestId": "create-probe"}}
        return {"id": record_id, "name": owner + ".", "type": "TXT", "records": ['"' + value + '"'],
                "zone_id": "zone", "ttl": 60, "status": "PENDING_CREATE", "create-probe": True}

    def rejected(self):
        if self.provider == "cf":
            return 403, {"success": False, "errors": [{"code": 10000, "message": "authentication failed"}], "result": None}
        if self.provider == "tencent":
            return 200, {"Response": {"Error": {"Code": "AuthFailure", "Message": "authentication failed"}, "RequestId": "probe"}}
        return 403, {"error_code": "DNS.0401", "error_msg": "authentication failed"}

    def dispatch(self, method, target, headers, body):
        rest_signature(method, target, headers, body, self.provider)
        path, _, query = target.partition("?")
        params = json.loads(body) if body else {}
        if self.provider == "tencent":
            action = headers["x-tc-action"]
            if action == "DescribeDomain":
                self.calls["zone"] += 1
                assert params.keys() == {"Domain"}
                if params["Domain"] == "example.com":
                    return 200, {"Response": {"DomainInfo": {"Domain": "example.com", "DomainId": 101}, "RequestId": "domain-probe"}}
                return 200, {"Response": {"Error": {"Code": "InvalidParameterValue.DomainNotExists", "Message": "no zone"}, "RequestId": "domain-probe"}}
            if action == "DescribeRecord":
                self.calls["read"] += 1
                assert params.keys() == {"Domain", "DomainId", "RecordId"} and params["Domain"] == "example.com" and params["DomainId"] == 101
                record_id = str(params["RecordId"])
                assert record_id in self.records, "read moved to unowned record"
                owner, value = self.records[record_id]
                return 200, {"Response": {"RecordInfo": {"Id": int(record_id), "SubDomain": owner.removesuffix(".example.com"),
                    "RecordType": "TXT", "RecordLine": "默认", "RecordLineId": "0", "Value": value, "Enabled": 1, "DomainId": 101}, "RequestId": "record-probe"}}
            create = action == "CreateRecord"
            assert create or action == "DeleteRecord"
            if create:
                assert params.keys() == {"Domain", "DomainId", "SubDomain", "RecordType", "RecordLine", "Value"} and params["DomainId"] == 101
                assert (params["Domain"], params["SubDomain"], params["RecordType"], params["RecordLine"]) == (
                    "example.com", "_acme-challenge.api", "TXT", "默认")
                owner, value = OWNER, params["Value"]
            else:
                assert params.keys() == {"Domain", "DomainId", "RecordId"} and params["Domain"] == "example.com" and params["DomainId"] == 101
                record_id = str(params["RecordId"])
        else:
            base = "/client/v4/zones" if self.provider == "cf" else "/v2/zones"
            if method == "GET" and path == base:
                self.calls["zone"] += 1
                query = dict(urllib.parse.parse_qsl(query))
                want = "example.com" if self.provider == "cf" else "example.com."
                name = query["name"]
                zones = [{"name": name, "id": "zone"}] if name == want else []
                return 200, {"success": True, "result": zones} if self.provider == "cf" else {"zones": zones}
            records_path = base + "/zone/" + ("dns_records" if self.provider == "cf" else "recordsets")
            create = method == "POST" and path == records_path
            if create:
                assert params["type"] == "TXT" and params["ttl"] == 60
                owner = params["name"].rstrip(".")
                assert owner == OWNER
                if self.provider == "cf":
                    assert params.keys() == {"name", "type", "content", "ttl"}
                    value = params["content"]
                else:
                    assert params.keys() == {"name", "type", "records", "ttl"}
                    assert len(params["records"]) == 1
                    value = json.loads(params["records"][0])
            else:
                if self.provider in {"cf", "huawei"} and method == "GET" and path.startswith(records_path + "/"):
                    self.calls["read"] += 1
                    record_id = path.removeprefix(records_path + "/")
                    if self.provider == "huawei" and record_id not in self.records:
                        assert record_id in self.deleted, "read targeted an unowned ID"
                        return HuaweiRecordService.missing()
                    assert record_id in self.records, "read targeted an unowned ID"
                    ack = self.acknowledgement(record_id, *self.records[record_id])
                    if self.provider == "huawei": ack["status"] = "ACTIVE"; ack["default"] = False
                    return 200, ack
                assert method == "DELETE" and path.startswith(records_path + "/") and not body
                record_id = path.removeprefix(records_path + "/")
        if not create:
            self.calls["delete"] += 1
            assert record_id in self.records, "deletion targeted an unowned ID"
            ack = self.acknowledgement(record_id, *self.records[record_id])
            self.deleted.append(record_id); del self.records[record_id]
            if self.provider == "huawei":
                ack["status"] = "PENDING_DELETE"; ack["default"] = False
                return 202, ack
            return 200, {"success": True, "result": {"id": record_id}} if self.provider == "cf" else (
                {"Response": {"RequestId": "probe"}} if self.provider == "tencent" else {"id": record_id})
        self.calls["create"] += 1
        if self.scenario == "rest-concurrent": time.sleep(0.2)
        number = self.calls["create"]
        self.submissions.append((owner, value))
        record_id = str(number + 10)
        if self.provider == "tencent" and self.scenario in {"rest-operation-denied", "rest-operation-denied-subclass"}:
            if number == 1:
                return 200, {"Response": {"RequestId": "probe", "Error": {
                    "Code": "OperationDenied" if self.scenario == "rest-operation-denied" else "OperationDenied.NoPermissionToOperateDomain",
                    "Message": "verified permission rejection"}}}
            self.records[record_id] = (owner, value)
            return 200, self.acknowledgement(record_id, owner, value)
        if self.scenario == "rest-rejected" and number == 1: return self.rejected()
        self.records[record_id] = (owner, value)
        ack = self.acknowledgement(record_id, owner, value)
        if self.scenario == "rest-normal" or (number > 1 and self.scenario in {"rest-rejected", "rest-isolation", "rest-presend-reset"}): return 200, ack
        if self.scenario == "rest-collision":
            if number == 1: return 200, ack
            return 200, self.acknowledgement("11", owner, value)
        suffix = self.scenario.removeprefix("rest-")
        if suffix in {"lost", "error-oom", "presend-reset"}: return None
        if suffix in {"500", "503"}: return int(suffix), b"upstream failed"
        if suffix == "404-malformed": return 404, b"not json"
        if suffix == "ack-500": return 500, ack
        if suffix == "ack-202" and self.provider == "tencent": return 202, ack
        if suffix == "error-500": return 500, self.rejected()[1]
        if suffix == "contradictory-403": return 403, ack
        if suffix == "empty": return 200, b""
        if suffix in {"malformed", "capacity", "concurrent", "isolation"}: return 200, b"{"
        if suffix == "nul": return 200, json.dumps(ack).encode() + b"\x00unverified-tail"
        result = ack["result"] if self.provider == "cf" else ack["Response"] if self.provider == "tencent" else ack
        id_key = "RecordId" if self.provider == "tencent" else "id"
        if suffix == "duplicate-id":
            encoded = json.dumps(ack, separators=(",", ":"))
            value = json.dumps(result[id_key])
            return 200, encoded.replace(f'"{id_key}":{value}', f'"{id_key}":{value},"{id_key}":{value}').encode()
        if suffix == "missing-id": del result[id_key]
        elif suffix == "unsafe-id": result[id_key] = "../unowned"
        elif suffix == "error-id":
            if self.provider == "cf": ack["errors"] = [{"code": 10000, "message": "contradictory"}]
            elif self.provider == "tencent": result["Error"] = {"Code": "AuthFailure", "Message": "contradictory"}
            else: result["error_code"] = "DNS.0401"; result["error_msg"] = "contradictory"
        elif suffix == "owner": result["name"] = "unowned.example.com."
        elif suffix == "type": result["type"] = "A"
        elif suffix == "value": result["content" if self.provider == "cf" else "records"] = "other" if self.provider == "cf" else ['"other"']
        elif suffix == "id-zero": result[id_key] = 0
        elif suffix == "id-float": result[id_key] = 11.5
        elif suffix == "missing-request": del result["RequestId"]
        elif suffix in {"error-prefix", "internal-error"}:
            del result["RecordId"]
            result["Error"] = {"Code": "AuthFailureSpoof" if suffix == "error-prefix" else "InternalError", "Message": "not a verified rejection"}
        elif suffix == "operation-denied-prefix" and self.provider == "tencent":
            del result["RecordId"]
            result["Error"] = {"Code": "OperationDeniedSpoof", "Message": "unverified error prefix"}
        elif suffix == "zone-id": result["zone_id"] = "unowned-zone"
        elif suffix == "error-message" and self.provider == "huawei": result["error_msg"] = "contradictory response"
        elif suffix == "response-info" and self.provider == "tencent":
            result["RecordInfo"] = {"Id": result.pop("RecordId")}; result["Error"] = {"Code": "AuthFailure", "Message": "contradiction"}
        elif suffix != "parse-oom": raise AssertionError("unknown fixture " + suffix)
        return 200, ack

    def verify(self):
        retriable_rejections = {"rest-rejected", "rest-operation-denied", "rest-operation-denied-subclass"}
        expected_create = 8 if self.scenario == "rest-capacity" else 2 if self.scenario in retriable_rejections | {"rest-isolation", "rest-collision", "rest-presend-reset"} else 1
        expected_delete = 1 if self.scenario in retriable_rejections | {"rest-normal", "rest-isolation", "rest-collision", "rest-presend-reset"} else 0
        assert self.calls["create"] == expected_create, self.calls
        assert self.calls["delete"] == expected_delete, self.calls
        if self.scenario == "rest-isolation": assert self.deleted == ["12"] and "11" in self.records
        if self.scenario == "rest-collision": assert self.deleted == ["11"] and "12" in self.records
        if self.provider == "huawei": assert self.calls["read"] == expected_delete * 2, self.calls
        if self.provider == "tencent": assert self.calls["read"] == expected_delete, self.calls


class RestZoneService:
    """Independent zone discovery with changing delegation and pinned record IDs."""
    def __init__(self, provider, scenario):
        assert provider in {"cf", "huawei"}
        self.provider = provider
        self.fault = scenario.removeprefix("rest-zone-")
        self.calls = collections.Counter()
        self.queries = []
        self.records = {}
        self.created = []
        self.deleted = []
        self.base = "/client/v4/zones" if provider == "cf" else "/v2/zones"
        self.collection = "dns_records" if provider == "cf" else "recordsets"

    def record(self, record_id, zone_id, owner, value, status="ACTIVE"):
        if self.provider == "cf":
            return {"success": True, "errors": [], "result": {
                "id": record_id, "name": owner, "type": "TXT", "content": value, "ttl": 60}}
        return {"id": record_id, "name": owner + ".", "type": "TXT", "zone_id": zone_id,
                "records": [json.dumps(value)], "status": status, "default": False, "ttl": 60}

    def dispatch(self, method, target, headers, body):
        rest_signature(method, target, headers, body, self.provider)
        path, _, query = target.partition("?")
        if method == "GET" and path == self.base:
            self.calls["zone"] += 1
            params = dict(urllib.parse.parse_qsl(query, strict_parsing=True))
            assert set(params) == ({"name", "per_page"} if self.provider == "cf" else {"name", "limit", "search_mode"})
            if self.provider == "huawei": assert params["search_mode"] == "equal" and params["limit"] == "2"
            else: assert params["per_page"] == "5"
            name = params["name"].rstrip(".")
            if self.provider == "huawei": assert params["name"] == name + "."
            self.queries.append(name)
            zones = []
            if not self.fault.startswith("no-zone"):
                if name == OWNER and self.fault == "owner-apex": zone_id = "apex"
                elif name == "example.com": zone_id = "parent"
                elif name == "api.example.com" and (
                        self.fault in {"parent-child", "replacement"} or
                        self.fault == "appears" and self.calls["create"] > 0 or
                        self.fault == "disappears" and self.calls["create"] == 0):
                    zone_id = ("child-old" if self.calls["create"] == 0 else "child-new") if self.fault == "replacement" else "child"
                else: zone_id = None
                if zone_id: zones = [{"id": zone_id, "name": params["name"]}]
                if name == "api.example.com" and self.calls["create"] > 0:
                    if self.fault == "denied":
                        return (403, {"success": False, "errors": [{"code": 10000}], "result": None}) if self.provider == "cf" else (
                            403, {"error_code": "DNS.0401", "error_msg": "query denied"})
                    if self.fault == "malformed": return 200, b"{"
                    if self.fault == "ambiguous": zones = [{"id": name, "name": params["name"]} for name in ("child-a", "child-b")]
            return 200, ({"success": True, "result": zones} if self.provider == "cf" else {"zones": zones})
        match = re.fullmatch(re.escape(self.base) + r"/([^/]+)/" + self.collection + r"(?:/([^/]+))?", path)
        assert match, (method, path)
        zone_id, record_id = match.groups()
        assert not query
        if method == "POST":
            assert record_id is None
            self.calls["create"] += 1
            params = json.loads(body)
            assert set(params) == ({"name", "type", "content", "ttl"} if self.provider == "cf" else {"name", "type", "records", "ttl"})
            assert params["type"] == "TXT" and params["ttl"] == 60
            owner = params["name"].rstrip(".")
            value = params["content"] if self.provider == "cf" else json.loads(params["records"][0])
            if self.provider == "huawei": assert len(params["records"]) == 1
            record_id = str(self.calls["create"] + 10)
            self.created.append((zone_id, record_id, owner, value))
            self.records[record_id] = (zone_id, owner, value)
            return 200, self.record(record_id, zone_id, owner, value)
        assert method in {"GET", "DELETE"} and not body
        if record_id not in self.records:
            assert self.provider == "huawei" and method == "GET" and (zone_id, record_id) in self.deleted
            self.calls["read"] += 1
            return HuaweiRecordService.missing()
        saved_zone, owner, value = self.records[record_id]
        assert zone_id == saved_zone, "cleanup moved an owned record into a different zone"
        if method == "GET":
            self.calls["read"] += 1
            return 200, self.record(record_id, zone_id, owner, value)
        self.calls["delete"] += 1
        self.deleted.append((zone_id, record_id)); del self.records[record_id]
        return (200, {"success": True, "result": {"id": record_id}}) if self.provider == "cf" else (
            202, self.record(record_id, zone_id, owner, value, "PENDING_DELETE"))

    def verify(self):
        first = [OWNER, "api.example.com", "example.com"]
        sibling = ["_acme-challenge.first.example.com", "first.example.com", "example.com"]
        pairs = {"parent-child": ("parent", "child"), "appears": ("parent", "child"),
                 "disappears": ("child", "parent"), "replacement": ("child-old", "child-new")}
        if self.fault.startswith("no-zone"):
            assert self.queries == first and self.calls["create"] == self.calls["delete"] == 0
        elif self.fault == "owner-apex":
            assert self.queries == [OWNER]
            assert self.created == [("apex", "11", OWNER, "first_digest-01")]
        elif self.fault in {"denied", "malformed", "ambiguous"}:
            assert self.queries == sibling + first[:2], self.queries
            assert self.created == [("parent", "11", sibling[0], "first_digest-01")]
        else:
            zone1, zone2 = pairs[self.fault]
            before = sibling if self.fault == "parent-child" else first[:2] if zone1 != "parent" else first
            after = first if zone2 == "parent" else first[:2]
            assert self.queries == before + after, self.queries
            assert self.created == [(zone1, "11", before[0], "first_digest-01"),
                                    (zone2, "12", OWNER, "second_digest-02")], self.created
        assert self.deleted == [(zone, record_id) for zone, record_id, _, _ in self.created]
        assert not self.records
        assert self.calls["read"] == len(self.created) * (2 if self.provider == "huawei" else 1)


class RestZoneInputService(RestZoneService):
    """Zone reads fail without a write/fallback; parsing OOM scans real allocations."""
    def __init__(self, provider, scenario):
        super().__init__(provider, scenario)
        self.fault = scenario.removeprefix("rest-zone-input-")
        self.input_queries = []
        self.input_reads = 0

    def dispatch(self, method, target, headers, body):
        path, _, query = target.partition("?")
        if method != "GET" or path != self.base:
            return super().dispatch(method, target, headers, body)
        rest_signature(method, target, headers, body, self.provider)
        params = dict(urllib.parse.parse_qsl(query, strict_parsing=True))
        assert set(params) == ({"name", "per_page"} if self.provider == "cf" else {"name", "limit", "search_mode"})
        if self.provider == "cf": assert params["per_page"] == "5"
        else: assert params["limit"] == "2" and params["search_mode"] == "equal"
        name = params["name"].rstrip(".")
        if self.provider == "huawei": assert params["name"] == name + "."
        self.input_queries.append(name)
        self.calls["zone"] += 1
        assert name in {OWNER, "api.example.com", "example.com"}
        if name == OWNER:
            self.input_reads += 1
            found = self.fault == "oom-found" or self.fault in {"201", "202", "206", "nul-found", "terminal-nul", "trailing-text"}
            zones = [{"id": "apex", "name": params["name"]}] if found else []
            payload = ({"success": True, "result": zones} if self.provider == "cf" else {"zones": zones})
            if self.fault.startswith("oom-"):
                payload["zone-probe"] = True
                return 200, payload
            if self.input_reads == 1:
                code = self.fault.removeprefix("stale-")
                if code in {"201", "202", "206"}: return int(code), payload
                if code in {"401", "403", "429", "503"}:
                    return int(code), ({"success": False, "result": None, "errors": [{"code": 10000, "message": "query failed"}]} if self.provider == "cf" else
                                       {"error_code": "DNS.0401", "error_msg": "query failed"})
                if self.fault == "stale-malformed": return 200, b"{"
                if self.fault == "stale-shape": return 200, {"success": True, "result": {}, "zones": {}}
                raw = json.dumps(payload, separators=(",", ":")).encode()
                if self.fault in {"nul-empty", "nul-found"}: return 200, raw + b"\0{\"hidden\":true}"
                if self.fault == "terminal-nul": return 200, raw + b"\0"
                if self.fault == "trailing-text": return 200, raw + b"invalid"
                raise AssertionError(self.fault)
            return 200, ({"success": True, "result": []} if self.provider == "cf" else {"zones": []})
        assert not self.fault.startswith("oom-") or self.fault == "oom-empty"
        zones = [{"id": "parent", "name": params["name"]}] if name == "example.com" else []
        return 200, ({"success": True, "result": zones} if self.provider == "cf" else {"zones": zones})

    def verify(self):
        if self.fault == "oom-found":
            assert self.input_reads > 1 and self.input_queries == [OWNER] * self.input_reads
            zone = "apex"
        elif self.fault == "oom-empty":
            assert self.input_reads > 1 and self.input_queries == [OWNER] * self.input_reads + ["api.example.com", "example.com"]
            zone = "parent"
        else:
            assert self.input_reads == 2 and self.input_queries == [OWNER, OWNER, "api.example.com", "example.com"], self.input_queries
            zone = "parent"
        assert self.created == [(zone, "11", OWNER, "first_digest-01")], self.created
        assert self.deleted == [(zone, "11")] and not self.records
        assert self.calls["create"] == self.calls["delete"] == 1
        assert self.calls["read"] == (2 if self.provider == "huawei" else 1)


class RestOwnedService:
    """Owned repeats read saved identities; the service independently tracks mutations."""
    def __init__(self, provider, scenario):
        self.provider = provider
        self.fault = scenario.removeprefix("rest-owned-")
        self.calls = collections.Counter()
        self.queries = []
        self.validations = []
        self.records = {}
        self.created = []
        self.deleted = []
        self.pending = set()
        self.fault_used = False
        self.base = "/client/v4/zones" if provider == "cf" else "/v2/zones"
        self.collection = "dns_records" if provider == "cf" else "recordsets"

    def zone_id(self, zone):
        return {"example.com": "zone", "api.example.com": "child"}[zone]

    @staticmethod
    def domain_id(zone_id):
        return {"zone": 101, "child": 202}[zone_id]

    def record(self, record_id, zone_id, owner, value, status="ACTIVE"):
        if self.provider == "cf":
            return {"success": True, "errors": [], "record-probe": True, "result": {
                "id": record_id, "name": owner, "type": "TXT", "content": value}}
        if self.provider == "huawei":
            return {"id": record_id, "name": owner + ".", "type": "TXT", "zone_id": zone_id,
                    "records": [json.dumps(value)], "status": status, "default": False, "record-probe": True}
        zone = "example.com" if zone_id == "zone" else "api.example.com"
        return {"Response": {"RequestId": "record-probe", "RecordInfo": {
            "Id": int(record_id), "DomainId": self.domain_id(zone_id), "SubDomain": owner.removesuffix("." + zone),
            "RecordType": "TXT", "RecordLine": "默认", "RecordLineId": "0", "Value": value, "Enabled": 1}}}

    def missing(self):
        if self.provider == "cf": return CfRecordService.missing()
        if self.provider == "huawei": return HuaweiRecordService.missing()
        return 200, TencentRecordService.error()

    def rejected(self):
        return RestCreateService(self.provider, "rest-normal").rejected()

    def dispatch(self, method, target, headers, body):
        rest_signature(method, target, headers, body, self.provider)
        params = json.loads(body) if body else {}
        path, _, query = target.partition("?")
        if self.provider == "tencent":
            assert method == "POST" and target == "/"
            action = headers["x-tc-action"]
            if action == "DescribeDomain":
                assert set(params) in ({"Domain"}, {"Domain", "DomainId"})
                zone = params["Domain"]
                validation = "DomainId" in params
                self.calls["domain" if validation else "zone"] += 1
                if validation:
                    assert (zone, params["DomainId"]) == ("example.com", 101)
                    self.validations.append((zone, params["DomainId"]))
                else: self.queries.append(zone)
                available = zone == "example.com" or zone == "api.example.com" and self.calls["create"] > 0 and self.fault == "zone-changed"
                if not available:
                    return 200, TencentRecordService.error("InvalidParameterValue.DomainNotExists", "domain-probe")
                ack = {"Response": {"RequestId": "domain-probe", "DomainInfo": {
                    "Domain": zone, "DomainId": self.domain_id(self.zone_id(zone))}}}
                if validation and self.fault.startswith("domain-") and self.fault != "domain-zero" and not self.fault_used:
                    self.fault_used = True
                    if self.fault == "domain-replaced": ack["Response"]["DomainInfo"]["DomainId"] = 201
                    elif self.fault == "domain-renamed": ack["Response"]["DomainInfo"]["Domain"] = "external.example.org"
                    elif self.fault == "domain-denied": return self.rejected()
                    elif self.fault != "domain-oom": raise AssertionError(self.fault)
                return 200, ack
            assert action in {"CreateRecord", "DescribeRecord", "DeleteRecord"}, action
            zone_id = self.zone_id(params["Domain"])
            assert params["DomainId"] == self.domain_id(zone_id)
            create = action == "CreateRecord"
            if create:
                assert set(params) == {"Domain", "DomainId", "SubDomain", "RecordType", "RecordLine", "Value"}
                assert (params["RecordType"], params["RecordLine"]) == ("TXT", "默认")
                owner = params["SubDomain"] + "." + params["Domain"]
                value = params["Value"]
            else:
                assert set(params) == {"Domain", "DomainId", "RecordId"}
                record_id = str(params["RecordId"])
                method = "GET" if action == "DescribeRecord" else "DELETE"
        else:
            if method == "GET" and path == self.base:
                self.calls["zone"] += 1
                fields = dict(urllib.parse.parse_qsl(query, strict_parsing=True))
                assert set(fields) == ({"name", "per_page"} if self.provider == "cf" else {"name", "limit", "search_mode"})
                if self.provider == "cf": assert fields["per_page"] == "5"
                else: assert fields["limit"] == "2" and fields["search_mode"] == "equal" and fields["name"].endswith(".")
                zone = fields["name"].rstrip(".")
                self.queries.append(zone)
                available = zone == "example.com" or zone == "api.example.com" and self.calls["create"] > 0 and self.fault in {"zone-changed", "missing"}
                zones = [{"id": self.zone_id(zone), "name": fields["name"]}] if available else []
                return 200, ({"success": True, "result": zones} if self.provider == "cf" else {"zones": zones})
            match = re.fullmatch(re.escape(self.base) + r"/([^/]+)/" + self.collection + r"(?:/([^/]+))?", path)
            assert match and not query, (method, target)
            zone_id, record_id = match.groups()
            create = method == "POST"
            if create:
                assert record_id is None and set(params) == ({"name", "type", "content", "ttl"} if self.provider == "cf" else {"name", "type", "records", "ttl"})
                assert params["type"] == "TXT" and params["ttl"] == 60
                owner = params["name"].rstrip(".")
                value = params["content"] if self.provider == "cf" else json.loads(params["records"][0])
                if self.provider == "huawei": assert params["name"] == owner + "." and len(params["records"]) == 1
            else: assert method in {"GET", "DELETE"} and not body
        if create:
            assert owner == OWNER
            self.calls["create"] += 1
            record_id = str(self.calls["create"] + 10)
            self.records[record_id] = (zone_id, owner, value)
            self.created.append((zone_id, record_id, owner, value))
            if self.provider == "tencent": return 200, {"Response": {"RecordId": int(record_id), "RequestId": "create-probe"}}
            response = self.record(record_id, zone_id, owner, value)
            response.pop("record-probe"); response["create-probe"] = True
            return 200, response
        if record_id not in self.records:
            assert self.provider == "huawei" and method == "GET" and (zone_id, record_id) in self.deleted
            self.calls["read"] += 1
            return self.missing()
        saved_zone, owner, value = self.records[record_id]
        assert zone_id == saved_zone, "owned repeat retargeted the original record"
        if method == "GET":
            self.calls["read"] += 1
            read_number = self.calls["read"]
            if self.fault == "read-lost" and read_number <= 3: return None
            if self.fault == "read-lost-once" and read_number == 1: return None
            if not self.fault_used and self.fault == "missing":
                self.fault_used = True
                if self.provider != "tencent":
                    del self.records[record_id]; self.deleted.append((zone_id, record_id))
                return self.missing()
            if record_id in self.pending:
                del self.records[record_id]; self.pending.remove(record_id); self.deleted.append((zone_id, record_id))
                return self.missing()
            ack = self.record(record_id, zone_id, owner, value)
            if not self.fault_used and self.fault.startswith("read-"):
                self.fault_used = True
                fault = self.fault
                item = ack["result"] if self.provider == "cf" else ack["Response"]["RecordInfo"] if self.provider == "tencent" else ack
                if fault == "read-id": item["Id" if self.provider == "tencent" else "id"] = 99 if self.provider == "tencent" else "external-id"
                elif fault == "read-owner": item["SubDomain" if self.provider == "tencent" else "name"] = "external.example.com"
                elif fault == "read-type": item["RecordType" if self.provider == "tencent" else "type"] = "A"
                elif fault == "read-value":
                    item[{"cf": "content", "huawei": "records", "tencent": "Value"}[self.provider]] = ['"external"'] if self.provider == "huawei" else "external"
                elif fault == "read-denied": return self.rejected()
                elif fault == "read-500": return 500, ack
                elif fault == "read-malformed": return 200, b"{"
                elif fault == "read-nul": return 200, json.dumps(ack).encode() + b"\x00tail"
                elif fault.startswith("read-inactive-"): item["status"] = fault.removeprefix("read-inactive-")
                elif fault == "read-disabled": item["Enabled"] = 0
                elif fault.startswith("read-busy-"): item["status"] = fault.removeprefix("read-busy-")
                elif fault == "read-deleting": item["status"] = "PENDING_DELETE"; self.pending.add(record_id)
                elif fault not in {"read-oom", "read-lost", "read-lost-once"}: raise AssertionError(fault)
            return 200, ack
        self.calls["delete"] += 1
        self.deleted.append((zone_id, record_id)); del self.records[record_id]
        if self.provider == "cf": return 200, {"success": True, "result": {"id": record_id}}
        if self.provider == "tencent": return 200, {"Response": {"RecordId": int(record_id), "RequestId": "delete-probe"}}
        return 202, self.record(record_id, zone_id, owner, value, "PENDING_DELETE")

    def verify(self):
        fault = self.fault
        creates = 8 if fault == "capacity" else 2 if fault == "zone-changed" or fault == "missing" and self.provider != "tencent" else 1
        assert self.calls["create"] == creates and not self.records and not self.pending, self.calls
        assert self.deleted == [(z, i) for z, i, _, _ in self.created], (self.deleted, self.created)
        assert [v for _, _, _, v in self.created] == (["first_digest-01", *[f"capacity_{i}" for i in range(1, 8)]] if fault == "capacity" else
               ["first_digest-01", "second_digest-02"] if fault == "zone-changed" else ["first_digest-01"] * creates)
        first = [OWNER, "api.example.com", "example.com"]
        second = first[:-1] if creates == 2 else first
        expected_queries = first * 8 if creates == 8 else first + second if creates == 2 else first
        assert self.queries == expected_queries, self.queries
        assert [z for z, _, _, _ in self.created] == (["zone", "child"] if creates == 2 else ["zone"] * creates)
        repeats = 10 if fault == "repeat-many" else 7 if fault == "concurrent-start" else 2 if (
            fault.startswith(("read-", "domain-")) and fault not in {"read-lost-once", "domain-zero"}) or fault == "missing" and self.provider == "tencent" else 1
        if fault in {"handle-invalid", "domain-zero"}: repeats = 1
        reads = repeats + creates * (2 if self.provider == "huawei" else 1)
        if fault == "read-lost": reads += 2
        if fault == "read-lost-once": reads += 1
        if fault.startswith("domain-") and fault != "domain-zero": reads -= 1
        if fault == "read-deleting": reads = 2
        if fault == "missing" and self.provider != "tencent": reads -= 2 if self.provider == "huawei" else 1
        assert self.calls["read"] == reads, self.calls
        deletes = creates - int(fault == "read-deleting" or fault == "missing" and self.provider != "tencent")
        assert self.calls["delete"] == deletes, self.calls
        if self.provider == "tencent":
            assert self.validations == [("example.com", 101)] * repeats, self.validations


class TencentRecordService(RestCreateService):
    """DNSPod identity, immutable domain IDs and independently delayed indexes."""
    def __init__(self, provider, scenario):
        assert provider == "tencent"
        super().__init__(provider, "rest-normal")
        self.domain_fault = scenario.startswith("tc-domain-")
        self.fault = scenario.removeprefix("tc-domain-").removeprefix("tc-record-")
        self.domain_ids = {}; self.record_zones = {}; self.reads = collections.Counter()
        self.fault_used = False
        self.external = {999: {"RecordId": 999, "Name": "@", "Type": "NS", "Value": "ns.example.test", "Status": "ENABLE"}}
        if self.fault in {"empty-inventory", "late-index-empty"}: self.external.clear()
        if self.fault == "full-inventory":
            self.external.update({i: {"RecordId": i, "Name": "manual-" + str(i), "Type": "A", "Value": "192.0.2.1", "Status": "ENABLE"} for i in range(1000, 3999)})
        self.initial_external = {i: dict(value) for i, value in self.external.items()}

    @staticmethod
    def error(code="InvalidParameter.RecordIdInvalid", request="record-probe"):
        return {"Response": {"Error": {"Code": code, "Message": "fixture error"}, "RequestId": request}}

    def info(self, record_id):
        owner, value = self.records[record_id]
        return {"Id": int(record_id), "SubDomain": owner.removesuffix("." + self.record_zones[record_id]),
                "RecordType": "TXT", "RecordLine": "默认", "RecordLineId": "0", "Value": value,
                "Enabled": 1, "DomainId": self.domain_ids[record_id]}

    def inventory(self):
        items = [dict(item) for item in self.external.values()] + [{"RecordId": int(i), "Name": self.info(i)["SubDomain"], "Type": "TXT",
                    "Value": v[1], "Status": "ENABLE"} for i, v in self.records.items()]
        return {"Response": {"RecordCountInfo": {"TotalCount": len(items), "ListCount": len(items), "SubdomainCount": len(items)},
                             "RecordList": items, "RequestId": "list-probe"}}

    def dispatch(self, method, target, headers, body):
        rest_signature(method, target, headers, body, "tencent")
        assert method == "POST" and target == "/"
        params = json.loads(body); action = headers["x-tc-action"]; fault = self.fault
        if action == "DescribeDomain":
            validation = "DomainId" in params
            self.calls["domain" if validation else "zone"] += 1
            assert params.keys() == ({"Domain", "DomainId"} if validation else {"Domain"})
            zone = params["Domain"]
            if validation: assert params["DomainId"] == 101 and zone == "example.com"
            if zone == OWNER or zone == "api.example.com" and not (fault == "more-specific" and self.calls["create"] != 0):
                return 200, self.error("InvalidParameterValue.DomainNotExists", "domain-probe")
            assert zone in {"example.com", "api.example.com"}
            ack = {"Response": {"DomainInfo": {"Domain": zone, "DomainId": 101 if zone == "example.com" else 202}, "RequestId": "domain-probe"}}
            use_fault = (self.domain_fault or validation and fault.startswith("domain-") and fault != "domain-zero") and not self.fault_used
            if use_fault:
                self.fault_used = True
                suffix = fault if self.domain_fault else fault.removeprefix("domain-")
                info = ack["Response"]["DomainInfo"]
                if suffix in {"id", "id-zero", "id-float", "id-string", "missing-id"}:
                    if suffix == "missing-id": del info["DomainId"]
                    else: info["DomainId"] = {"id": 201, "id-zero": 0, "id-float": 101.5, "id-string": "101"}[suffix]
                elif suffix == "name": info["Domain"] = "external.example.org"
                elif suffix == "missing-request": del ack["Response"]["RequestId"]
                elif suffix == "empty-request": ack["Response"]["RequestId"] = ""
                elif suffix == "denied": return 200, self.error("AuthFailure", "domain-probe")
                elif suffix == "500": return 500, ack
                elif suffix == "malformed": return 200, b"{"
                elif suffix == "nul": return 200, json.dumps(ack).encode() + b"\x00tail"
                elif suffix == "error-info": ack["Response"]["Error"] = {"Code": "AuthFailure", "Message": "contradiction"}
                elif suffix == "wrong-missing": return 200, self.error("InvalidParameter.RecordIdInvalid", "domain-probe")
                elif suffix == "missing-prefix": return 200, self.error("InvalidParameterValue.DomainNotExists.Spoof", "domain-probe")
                elif suffix != "oom": raise AssertionError("unknown domain fault " + suffix)
            return 200, ack
        if action == "CreateRecord":
            self.calls["create"] += 1
            assert params.keys() == {"Domain", "DomainId", "SubDomain", "RecordType", "RecordLine", "Value"}
            zone = params["Domain"]; domain_id = 101 if zone == "example.com" else 202
            assert params["DomainId"] == domain_id and (params["RecordType"], params["RecordLine"]) == ("TXT", "默认")
            assert params["SubDomain"] + "." + zone == OWNER
            record_id = str(self.calls["create"] + 10)
            self.records[record_id] = (OWNER, params["Value"])
            self.domain_ids[record_id] = domain_id; self.record_zones[record_id] = zone
            return 200, self.acknowledgement(record_id, OWNER, params["Value"])
        if action == "DescribeRecordList":
            self.calls["list"] += 1
            assert params == {"Domain": "example.com", "DomainId": 101, "Offset": 0, "Limit": 3000, "ErrorOnEmpty": "no"}
            ack = self.inventory(); result = ack["Response"]; items = result["RecordList"]; counts = result["RecordCountInfo"]
            if (fault == "early-index" and self.calls["list"] == 1) or fault.startswith("late-index-"):
                # A new physical record can still be absent from this index.
                # The production grace-period guard must prevent this query.
                items[:] = [item for item in items if item["RecordId"] != 11]
                counts.update(TotalCount=len(items), ListCount=len(items))
            if fault.startswith("list-") and self.calls["list"] == 1:
                suffix = fault.removeprefix("list-")
                if suffix == "denied": return 200, self.error("AuthFailure", "list-probe")
                if suffix == "500": return 500, ack
                if suffix == "malformed": return 200, b"{"
                if suffix == "nul": return 200, json.dumps(ack).encode() + b"\x00tail"
                if suffix == "error-empty": return 200, self.error("ResourceNotFound.NoDataOfRecord", "list-probe")
                if suffix == "partial": counts["TotalCount"] += 1
                elif suffix == "oversize": counts["TotalCount"] = 3001
                elif suffix == "count-string": counts["TotalCount"] = "1"
                elif suffix == "count-negative": counts["TotalCount"] = -1
                elif suffix == "listed": counts["ListCount"] += 1
                elif suffix == "missing-count": del result["RecordCountInfo"]
                elif suffix == "missing-request": del result["RequestId"]
                elif suffix == "object": result["RecordList"] = {}
                elif suffix == "id-zero": items[0]["RecordId"] = 0
                elif suffix == "id-string": items[0]["RecordId"] = "999"
                elif suffix == "missing-id": del items[0]["RecordId"]
                elif suffix == "name": del items[0]["Name"]
                elif suffix == "type": items[0]["Type"] = 1
                elif suffix == "value": items[0]["Value"] = None
                elif suffix == "name-nul": items[0]["Name"] = "@\x00external"
                elif suffix == "type-nul": items[0]["Type"] = "NS\x00TXT"
                elif suffix == "value-nul": items[0]["Value"] = "external\x00tail"
                elif suffix == "status": items[0]["Status"] = "UNKNOWN"
                elif suffix == "duplicate": items.append(dict(items[0])); counts.update(TotalCount=2, ListCount=2)
                elif suffix == "owned": items.append({"RecordId": 11, "Name": "_acme-challenge.api", "Type": "TXT", "Value": "first_digest-01", "Status": "ENABLE"}); counts.update(TotalCount=2, ListCount=2)
                elif suffix == "contradiction": result["RecordInfo"] = {"Id": 11}
                elif suffix != "oom": raise AssertionError("unknown inventory fault " + suffix)
            return 200, ack
        record_id = str(params["RecordId"])
        assert params.keys() == {"Domain", "DomainId", "RecordId"} and record_id in ({"11", "12"} if fault in {"partial", "more-specific"} else {"11"})
        assert params["DomainId"] == self.domain_ids[record_id] and params["Domain"] == self.record_zones[record_id]
        if action == "DescribeRecord":
            self.calls["read"] += 1; self.reads[record_id] += 1
            n = self.reads[record_id]
            if fault.startswith("late-index-") and (n == 2 if fault == "late-index-delete" else n <= (2 if fault == "late-index-repeat" else 1)):
                return 200, self.error()
            if fault == "partial" and record_id == "12" and n == 1: return 200, self.error("AuthFailure")
            if fault == "read-lost" and n <= 3: return None
            if fault == "read-lost-once" and n == 1: return None
            if n == 1 and (fault in {"missing", "early-missing", "clock-back", "empty-inventory", "full-inventory"} or fault.startswith(("list-", "domain-")) and fault != "domain-zero"): del self.records[record_id]
            if self.calls["delete"] == 1 and n == 2 and fault.startswith("reconcile-"):
                if fault == "reconcile-denied": return 200, self.error("AuthFailure")
                if fault == "reconcile-malformed": return 200, b"{"
            if record_id not in self.records: return 200, self.error()
            if fault == "early-index" and n == 1: return 200, self.error()
            ack = {"Response": {"RecordInfo": self.info(record_id), "RequestId": "record-probe"}}
            if n == 1 and fault.startswith("read-"):
                suffix = fault.removeprefix("read-"); info = ack["Response"]["RecordInfo"]
                if suffix == "id": info["Id"] = 99
                elif suffix == "domain-id": info["DomainId"] = 201
                elif suffix == "id-string": info["Id"] = "11"
                elif suffix == "owner": info["SubDomain"] = "external"
                elif suffix == "value": info["Value"] = "external"
                elif suffix == "type": info["RecordType"] = "A"
                elif suffix == "line": info["RecordLine"] = "电信"
                elif suffix == "line-id": info["RecordLineId"] = "10=0"
                elif suffix == "enabled": info["Enabled"] = 2
                elif suffix == "enabled-string": info["Enabled"] = "1"
                elif suffix == "disabled": info["Enabled"] = 0
                elif suffix == "missing-id": del info["Id"]
                elif suffix == "missing-request": del ack["Response"]["RequestId"]
                elif suffix == "error-info": ack["Response"]["Error"] = {"Code": "InvalidParameter.RecordIdInvalid", "Message": "contradiction"}
                elif suffix == "denied": return 200, self.error("AuthFailure")
                elif suffix == "500": return 500, ack
                elif suffix == "malformed": return 200, b"{"
                elif suffix == "nul": return 200, json.dumps(ack).encode() + b"\x00tail"
                elif suffix == "generic-404": return 404, b"not found"
                elif suffix == "missing-prefix": return 200, self.error("InvalidParameter.RecordIdInvalid.Spoof")
                elif suffix == "list-error": return 200, self.error("ResourceNotFound.NoDataOfRecord")
                elif suffix != "oom": raise AssertionError("unknown record fault " + suffix)
            return 200, ack
        assert action == "DeleteRecord" and record_id in self.records
        self.calls["delete"] += 1; first = self.calls["delete"] == 1
        if fault == "late-index-delete" and first: return None
        committed = not first or not fault.startswith("uncommitted-")
        if committed: del self.records[record_id]; self.deleted.append(record_id)
        ack = {"Response": {"RequestId": "delete-probe"}}
        if self.domain_fault or not first or fault in {"normal", "partial", "more-specific", "delete-presend", "early-index", "domain-zero"} or fault.startswith(("read-", "handle-", "late-index-")): return 200, ack
        suffix = fault.removeprefix("committed-").removeprefix("uncommitted-")
        if suffix in {"lost", "error-oom"} or fault.startswith("reconcile-"): return None
        if suffix == "500": return 500, ack
        if suffix == "http-202": return 202, ack
        if suffix == "empty": return 200, b""
        if suffix == "malformed": return 200, b"{"
        if suffix == "nul": return 200, json.dumps(ack).encode() + b"\x00tail"
        if suffix == "denied": return 200, self.error("AuthFailure", "delete-probe")
        if suffix == "operation-denied": return 200, self.error("OperationDenied.NoPermissionToOperateDomain", "delete-probe")
        if suffix == "operation-denied-prefix": return 200, self.error("OperationDeniedSpoof", "delete-probe")
        if suffix == "missing": return 200, self.error(request="delete-probe")
        if suffix == "missing-request": del ack["Response"]["RequestId"]
        elif suffix == "empty-request": ack["Response"]["RequestId"] = ""
        elif suffix == "id": ack["Response"]["RecordId"] = 99
        elif suffix == "id-string": ack["Response"]["RecordId"] = "11"
        elif suffix == "error-info": ack["Response"] = {"RequestId": "delete-probe", "RecordInfo": {"Id": 11}, "Error": {"Code": "AuthFailure", "Message": "contradiction"}}
        elif suffix != "oom": raise AssertionError("unknown delete fault " + suffix)
        return 200, ack

    def verify(self):
        fault = self.fault
        assert not self.records and self.external == self.initial_external
        if fault.startswith("late-index-"):
            assert self.calls["create"] == 1 and self.deleted == ["11"], self.calls
            assert self.calls["delete"] == (2 if fault == "late-index-delete" else 1), self.calls
            assert self.calls["read"] == (3 if fault in {"late-index-delete", "late-index-repeat"} else 2), self.calls
            assert self.calls["list"] == (2 if fault == "late-index-repeat" else 1), self.calls
            return
        if self.domain_fault:
            assert self.calls["create"] == self.calls["delete"] == self.calls["read"] == 1 and self.deleted == ["11"], self.calls
            return
        if fault in {"partial", "more-specific"}:
            assert self.calls["create"] == self.calls["delete"] == 2 and self.calls["read"] == (3 if fault == "partial" else 2), self.calls
            assert self.deleted == ["11", "12"]; return
        no_delete = fault in {"missing", "early-missing", "clock-back", "empty-inventory", "full-inventory"} or fault.startswith(("list-", "domain-")) and fault != "domain-zero"
        wanted_delete = 0 if no_delete else 2 if fault.startswith("uncommitted-") else 1
        assert self.calls["create"] == 1 and self.calls["delete"] == wanted_delete, self.calls
        if no_delete: assert not self.deleted
        else: assert self.deleted == ["11"]
        wanted_read = 1
        if fault in {"missing", "empty-inventory", "full-inventory", "early-index", "early-missing", "clock-back", "delete-presend", "committed-oom"} or fault.startswith(("list-", "domain-")): wanted_read = 2
        if fault == "domain-zero": wanted_read = 1
        if fault.startswith("read-"): wanted_read = 4 if fault == "read-lost" else 2 if fault != "read-disabled" else 1
        if fault.startswith("committed-"): wanted_read = 2 if fault == "committed-oom" else 3
        if fault.startswith("uncommitted-"): wanted_read = 2 if fault in {"uncommitted-denied", "uncommitted-operation-denied", "uncommitted-oom", "uncommitted-error-oom"} else 3
        if fault.startswith("reconcile-"): wanted_read = 3
        assert self.calls["read"] == wanted_read, self.calls
        if no_delete or fault.startswith(("committed-", "reconcile-")):
            expected_lists = 1 if fault in {"early-missing", "clock-back", "committed-oom"} or fault.startswith(("domain-", "reconcile-")) else 2
            assert self.calls["list"] == expected_lists, self.calls
        if not no_delete and not fault.startswith(("committed-", "reconcile-")): assert self.calls["list"] == 0, self.calls


class TencentZoneService:
    """Full-owner discovery, apex host records and immutable cleanup identities."""
    def __init__(self, provider, scenario):
        assert provider == "tencent"
        self.fault = scenario.removeprefix("tc-zone-")
        self.calls = collections.Counter()
        self.queries = []
        self.validations = []
        self.records = {}
        self.created = []
        self.deleted = []
        self.fault_used = False
        self.owner = ("_acme-challenge." + ".".join(["a" * 63] * 3 + ["a" * 33, "example", "com"])) if self.fault == "owner-long" else OWNER
        assert self.fault != "owner-long" or len(self.owner) == 253

    def hosted_id(self, zone):
        fault = self.fault
        if fault.startswith("no-zone"): return None
        if zone == self.owner and (fault.startswith("apex-read-") or fault in {"owner-apex", "owner-case", "owner-long"}): return 303
        if zone == "example.com": return 101
        if zone == "api.example.com" and (fault in {"parent-child", "replacement"} or
                fault == "appears" and self.calls["create"] != 0 or fault == "disappears" and self.calls["create"] == 0):
            return 302 if fault == "replacement" and self.calls["create"] != 0 else 202
        return None

    def dispatch(self, method, target, headers, body):
        rest_signature(method, target, headers, body, "tencent")
        assert method == "POST" and target == "/"
        params = json.loads(body)
        action = headers["x-tc-action"]
        if action == "DescribeDomain":
            zone = params["Domain"]
            if "DomainId" in params:
                assert params.keys() == {"Domain", "DomainId"}
                self.calls["domain"] += 1
                self.validations.append((zone, params["DomainId"]))
                assert any((z, i) == (zone, params["DomainId"]) for z, i, _, _, _ in self.created)
                domain_id = params["DomainId"]
            else:
                assert params.keys() == {"Domain"}
                self.calls["zone"] += 1
                self.queries.append(zone)
                assert zone in {self.owner, "api.example.com", "example.com", "_acme-challenge.first.example.com", "first.example.com"}, "unexpected discovery candidate: " + zone
                if zone == self.owner and not self.fault_used and self.fault in {"denied", "503", "malformed", "nul", "wrong-domain", "missing-request"}:
                    self.fault_used = True
                    if self.fault == "denied": return 200, TencentRecordService.error("OperationDenied.NoPermissionToOperateDomain", "domain-probe")
                    if self.fault == "503": return 503, TencentRecordService.error("InternalError", "domain-probe")
                    if self.fault == "malformed": return 200, b"{"
                    ack = {"Response": {"DomainInfo": {"Domain": zone, "DomainId": 303}, "RequestId": "domain-probe"}}
                    if self.fault == "nul": return 200, json.dumps(ack).encode() + b"\0tail"
                    if self.fault == "wrong-domain": ack["Response"]["DomainInfo"]["Domain"] = "external.example.org"
                    else: del ack["Response"]["RequestId"]
                    return 200, ack
                domain_id = self.hosted_id(zone)
                if domain_id is None:
                    code = "InvalidParameter.DomainInvalid" if zone == self.owner and self.fault == "invalid-owner" else "InvalidParameterValue.DomainNotExists"
                    return 200, TencentRecordService.error(code, "domain-probe")
            return 200, {"Response": {"DomainInfo": {"Domain": zone, "DomainId": domain_id}, "RequestId": "domain-probe"}}
        if action == "CreateRecord":
            assert params.keys() == {"Domain", "DomainId", "SubDomain", "RecordType", "RecordLine", "Value"}
            zone, domain_id, sub = params["Domain"], params["DomainId"], params["SubDomain"]
            assert domain_id == self.hosted_id(zone)
            assert (params["RecordType"], params["RecordLine"]) == ("TXT", "默认")
            owner = zone if sub == "@" else sub + "." + zone
            assert owner in {self.owner, "_acme-challenge.first.example.com"}
            self.calls["create"] += 1
            record_id = str(self.calls["create"] + 10)
            self.records[record_id] = (zone, domain_id, sub, params["Value"])
            self.created.append((zone, domain_id, record_id, owner, params["Value"]))
            return 200, {"Response": {"RecordId": int(record_id), "RequestId": "create-probe"}}
        assert action in {"DescribeRecord", "DeleteRecord"}
        assert params.keys() == {"Domain", "DomainId", "RecordId"}
        record_id = str(params["RecordId"])
        assert record_id in self.records, "operation targeted an unowned record"
        zone, domain_id, sub, value = self.records[record_id]
        assert (params["Domain"], params["DomainId"]) == (zone, domain_id), "cleanup moved the original identity"
        if action == "DescribeRecord":
            self.calls["read"] += 1
            info = {"Id": int(record_id), "DomainId": domain_id, "SubDomain": sub, "Value": value,
                    "RecordType": "TXT", "RecordLine": "默认", "RecordLineId": "0", "Enabled": 1}
            if self.fault.startswith("apex-read-") and not self.fault_used:
                self.fault_used = True
                if self.fault == "apex-read-owner": info["SubDomain"] = self.owner
                elif self.fault == "apex-read-domain-id": info["DomainId"] += 1
                elif self.fault == "apex-read-value": info["Value"] = "external-value"
            return 200, {"Response": {"RecordInfo": info, "RequestId": "record-probe"}}
        self.calls["delete"] += 1
        self.deleted.append((zone, domain_id, record_id)); del self.records[record_id]
        return 200, {"Response": {"RequestId": "delete-probe"}}

    def verify(self):
        fault = self.fault
        first = [self.owner, "api.example.com", "example.com"]
        sibling = ["_acme-challenge.first.example.com", "first.example.com", "example.com"]
        if fault.startswith("no-zone"):
            assert self.queries == first and not self.created and not self.deleted
            assert self.calls == {"zone": 3}, self.calls
            return
        if fault in {"parent-child", "appears", "disappears", "replacement"}:
            expected = {"parent-child": (sibling, first[:2], [("example.com", 101), ("api.example.com", 202)]),
                        "appears": (first, first[:2], [("example.com", 101), ("api.example.com", 202)]),
                        "disappears": (first[:2], first, [("api.example.com", 202), ("example.com", 101)]),
                        "replacement": (first[:2], first[:2], [("api.example.com", 202), ("api.example.com", 302)])}[fault]
            before, after, zones = expected
            assert self.queries == before + after, self.queries
            assert [(z, i) for z, i, _, _, _ in self.created] == zones
            assert self.calls["create"] == self.calls["delete"] == self.calls["read"] == 2
            assert self.calls["domain"] == 0
        else:
            bad_discovery = fault in {"denied", "503", "malformed", "nul", "wrong-domain", "missing-request"}
            apex = fault.startswith("apex-read-") or fault in {"owner-apex", "owner-case", "owner-long"}
            assert self.queries == ([self.owner] if apex else ([self.owner] + first if bad_discovery else first)), self.queries
            assert self.created == [(self.owner if apex else "example.com", 303 if apex else 101, "11", self.owner, "first_digest-01")]
            assert self.calls["create"] == self.calls["delete"] == 1
            assert self.calls["read"] == (2 if fault.startswith("apex-read-") or not bad_discovery else 1)
            assert self.calls["domain"] == (0 if bad_discovery or fault.startswith("apex-read-") else 1)
        assert self.deleted == [(z, i, record_id) for z, i, record_id, _, _ in self.created]
        assert not self.records


class CfRecordService(RestCreateService):
    """Independent record state, identity reads and delete outcome faults."""
    def __init__(self, provider, scenario):
        assert provider == "cf"
        super().__init__(provider, "rest-normal")
        self.fault = scenario.removeprefix("cf-record-")

    @staticmethod
    def missing():
        return 404, {"success": False, "errors": [{"code": 81044, "message": "Record does not exist."}], "result": None}

    def dispatch(self, method, target, headers, body):
        prefix = "/client/v4/zones/zone/dns_records/"
        if not target.startswith(prefix): return super().dispatch(method, target, headers, body)
        rest_signature(method, target, headers, body, "cf")
        assert not body and method in {"GET", "DELETE"}
        record_id = target.removeprefix(prefix)
        assert record_id in ({"11", "12"} if self.fault == "partial" else {"11"}), "operation moved to another record"
        fault = self.fault
        if method == "GET":
            self.calls["read"] += 1
            n = self.calls["read"]
            if fault == "partial" and n == 2: return self.rejected()
            if fault == "read-lost" and n <= 3: return None
            if fault == "read-lost-once" and n == 1: return None
            if fault == "missing" and n == 1: del self.records[record_id]
            if record_id not in self.records:
                if fault == "reconcile-denied" and n == 2: return self.rejected()
                if fault == "reconcile-malformed" and n == 2: return 200, b"{"
                status, error = self.missing()
                if fault == "reconcile-oom": error["record-probe"] = True
                return status, error
            ack = self.acknowledgement(record_id, *self.records[record_id])
            ack.pop("create-probe"); ack["record-probe"] = True
            result = ack["result"]
            if n == 1 and fault.startswith("read-"):
                suffix = fault.removeprefix("read-")
                if suffix == "id": result["id"] = "external-id"
                elif suffix == "owner": result["name"] = "external.example.com"
                elif suffix == "type": result["type"] = "A"
                elif suffix == "value": result["content"] = "external-value"
                elif suffix == "missing-id": del result["id"]
                elif suffix == "missing-errors": del ack["errors"]
                elif suffix == "success-string": ack["success"] = "true"
                elif suffix == "success-false": ack["success"] = False
                elif suffix == "error-id": ack["errors"] = [{"code": 10000, "message": "contradiction"}]
                elif suffix == "denied": return self.rejected()
                elif suffix == "500": return 500, ack
                elif suffix == "malformed": return 200, b"{"
                elif suffix == "nul": return 200, json.dumps(ack).encode() + b"\x00tail"
                elif suffix in {"404", "404-empty"}: return 404, self.rejected()[1] if suffix == "404" else b""
                elif suffix == "404-result":
                    status, error = self.missing(); error["result"] = result; return status, error
                elif suffix == "404-mixed":
                    status, error = self.missing(); error["errors"].append({"code": 10000, "message": "denied"}); return status, error
                elif suffix == "404-code-string":
                    status, error = self.missing(); error["errors"][0]["code"] = "81044"; return status, error
                elif suffix == "missing-200": return 200, self.missing()[1]
                elif suffix == "404-success":
                    status, error = self.missing(); error["success"] = True; return status, error
                elif suffix == "404-no-message":
                    status, error = self.missing(); del error["errors"][0]["message"]; return status, error
                elif suffix == "duplicate-id":
                    encoded = json.dumps(ack, separators=(",", ":"))
                    return 200, encoded.replace('"id":"11"', '"id":"11","id":"11"').encode()
                elif suffix != "oom": raise AssertionError("unknown read fault " + suffix)
            return 200, ack
        self.calls["delete"] += 1
        assert record_id in self.records and self.records[record_id] == (OWNER, "first_digest-01")
        first = self.calls["delete"] == 1
        committed = not first or fault in {"normal", "ack-minimal", "partial"} or fault.startswith("committed-") or fault.startswith("reconcile-") or fault.startswith("read-")
        if committed:
            del self.records[record_id]; self.deleted.append(record_id)
        ack = {"success": True, "errors": [], "result": {"id": record_id}, "delete-probe": True}
        if not first or fault in {"normal", "missing", "partial"} or fault.startswith("read-"): return 200, ack
        if fault == "ack-minimal": return 200, {"result": {"id": record_id}}
        suffix = fault.removeprefix("committed-").removeprefix("uncommitted-")
        if suffix == "lost" or fault.startswith("reconcile-"): return None
        if suffix == "500": return 500, ack
        if suffix == "empty": return 200, b""
        if suffix == "malformed": return 200, b"{"
        if suffix == "nul": return 200, json.dumps(ack).encode() + b"\x00tail"
        if suffix == "404": return self.missing()
        if suffix == "404-generic": return 404, b"not found"
        if suffix == "id": ack["result"]["id"] = "external-id"
        elif suffix == "missing-id": del ack["result"]["id"]
        elif suffix == "success-string": ack["success"] = "true"
        elif suffix == "success-false": ack["success"] = False
        elif suffix == "errors-object": ack["errors"] = {}
        elif suffix == "error-id": ack["errors"] = [{"code": 10000, "message": "contradiction"}]
        elif suffix == "denied": return self.rejected()
        elif suffix == "oom": pass
        elif suffix != "error-oom": raise AssertionError("unknown delete fault " + suffix)
        if suffix == "error-oom": return None
        return 200, ack

    def verify(self):
        fault = self.fault
        if fault == "partial":
            assert self.calls["create"] == 2 and self.calls["read"] == 3 and self.calls["delete"] == 2, self.calls
            assert self.deleted == ["11", "12"] and not self.records, self.records
            return
        assert self.calls["create"] == 1 and not self.records, self.calls
        assert self.deleted == ([] if fault == "missing" else ["11"]), self.deleted
        expected_delete = 0 if fault == "missing" else 2 if fault.startswith("uncommitted-") else 1
        expected_read = 1 if fault in {"normal", "ack-minimal", "missing"} else 3 if fault.startswith("uncommitted-") else 2
        if fault in {"uncommitted-denied", "uncommitted-oom", "uncommitted-error-oom"}: expected_read = 2
        if fault.startswith("reconcile-"): expected_read = 3
        if fault == "read-lost": expected_read = 4
        assert self.calls["read"] == expected_read and self.calls["delete"] == expected_delete, self.calls


class HuaweiRecordService(RestCreateService):
    """Actual v2 recordsets, including delayed asynchronous deletion."""
    def __init__(self, provider, scenario):
        assert provider == "huawei"
        super().__init__(provider, "rest-normal")
        self.fault = scenario.removeprefix("hw-record-")
        self.pending = set()
        self.accepted = []
        self.reads = collections.Counter()
        self.pending_reads = collections.Counter()

    @staticmethod
    def missing():
        return 404, {"error_code": "DNS.0313", "error_msg": "This record set does not exist."}

    def record(self, record_id, status="ACTIVE", marker="record-probe"):
        owner, value = self.records[record_id]
        return {"id": record_id, "name": owner + ".", "type": "TXT", "zone_id": "zone",
                "records": ['"' + value + '"'], "ttl": 60, "default": False, "status": status, marker: True}

    def dispatch(self, method, target, headers, body):
        prefix = "/v2/zones/zone/recordsets/"
        if not target.startswith(prefix): return super().dispatch(method, target, headers, body)
        rest_signature(method, target, headers, body, "huawei")
        assert not body and method in {"GET", "DELETE"}
        record_id = target.removeprefix(prefix)
        assert record_id in ({"11", "12"} if self.fault == "partial" else {"11"}), "operation moved to another recordset"
        fault = self.fault
        if method == "GET":
            self.calls["read"] += 1; self.reads[record_id] += 1
            n = self.reads[record_id]
            if fault == "partial" and record_id == "12" and n == 1: return self.rejected()
            if fault == "read-lost" and n <= 3: return None
            if fault == "read-lost-once" and n == 1: return None
            if fault == "missing" and n == 1: del self.records[record_id]
            if fault == "initial-pending" and n == 1: self.pending.add(record_id)
            if record_id not in self.records: return self.missing()
            if record_id in self.pending:
                if n == 2 and fault in {"reconcile-denied", "accepted-read-denied"}: return self.rejected()
                if n == 2 and fault == "reconcile-malformed": return 200, b"{"
                self.pending_reads[record_id] += 1
                pn = self.pending_reads[record_id]
                status = "PENDING_DELETE"
                if fault == "delayed-active" and pn == 1: status = "ACTIVE"
                if fault == "pending-active" and pn <= 5: status = "ACTIVE"
                keep = pn <= 5 if fault in {"pending-exhausted", "pending-active"} else pn <= 2 if fault == "delayed-active" else pn <= 1
                if not keep:
                    del self.records[record_id]; self.pending.remove(record_id); self.deleted.append(record_id)
                    return self.missing()
                ack = self.record(record_id, status)
                if n == 2 and fault == "accepted-read-identity": ack["records"].append('"external"')
                return 200, ack
            ack = self.record(record_id)
            if n == 1 and fault.startswith("read-"):
                suffix = fault.removeprefix("read-")
                if suffix == "id": ack["id"] = "external-id"
                elif suffix == "owner": ack["name"] = "external.example.com."
                elif suffix == "type": ack["type"] = "A"
                elif suffix == "zone": ack["zone_id"] = "external-zone"
                elif suffix == "value": ack["records"] = ['"external"']
                elif suffix == "multi-value": ack["records"].append('"external"')
                elif suffix == "default": ack["default"] = True
                elif suffix == "default-string": ack["default"] = "false"
                elif suffix == "missing-default": del ack["default"]
                elif suffix == "missing-status": del ack["status"]
                elif suffix == "status-unknown": ack["status"] = "ACTIVE-SPOOF"
                elif suffix.startswith("busy-"): ack["status"] = "PENDING_" + suffix.removeprefix("busy-").upper()
                elif suffix == "error-message": ack["error_msg"] = "contradiction"
                elif suffix == "denied": return self.rejected()
                elif suffix == "500": return 500, ack
                elif suffix == "malformed": return 200, b"{"
                elif suffix == "nul": return 200, json.dumps(ack).encode() + b"\x00tail"
                elif suffix == "404-generic": return 404, b"not found"
                elif suffix == "404-zone": return 404, {"error_code": "DNS.0302", "error_msg": "zone not found"}
                elif suffix == "404-prefix": return 404, {"error_code": "DNS.0313.Spoof", "error_msg": "not found"}
                elif suffix == "404-id":
                    status, error = self.missing(); error["id"] = record_id; return status, error
                elif suffix == "404-no-message": return 404, {"error_code": "DNS.0313"}
                elif suffix == "missing-200": return 200, self.missing()[1]
                elif suffix in {"404-status", "404-default"}:
                    status, error = self.missing(); error["status" if suffix == "404-status" else "default"] = "ACTIVE" if suffix == "404-status" else False; return status, error
                elif suffix == "duplicate-id":
                    encoded = json.dumps(ack, separators=(",", ":"))
                    return 200, encoded.replace('"id":"11"', '"id":"11","id":"11"').encode()
                elif suffix in {"disabled", "frozen", "illegal", "police", "error"}:
                    ack["status"] = {"disabled": "DISABLE", "frozen": "FREEZE"}.get(suffix, suffix.upper())
                elif suffix != "oom": raise AssertionError("unknown Huawei read fault " + suffix)
            return 200, ack
        self.calls["delete"] += 1
        assert record_id in self.records and self.records[record_id] == (OWNER, "first_digest-01")
        if record_id in self.pending:
            return 409, {"error_code": "DNS.0314", "error_msg": "The record set is not in a steady state."}
        first = self.calls["delete"] == 1
        committed = not first or not fault.startswith("uncommitted-")
        if committed:
            self.pending.add(record_id); self.accepted.append(record_id)
        ack = self.record(record_id, "PENDING_DELETE", "delete-probe")
        if not first or fault in {"normal", "delayed-active", "pending-exhausted", "pending-active", "partial", "delete-presend"} or fault.startswith("read-") or fault.startswith("accepted-read-"):
            return 202, ack
        if fault == "ack-no-records": del ack["records"]; return 202, ack
        suffix = fault.removeprefix("committed-").removeprefix("uncommitted-")
        if suffix in {"lost", "error-oom"} or fault.startswith("reconcile-"): return None
        if suffix == "500": return 500, ack
        if suffix == "http-200": return 200, ack
        if suffix == "http-204": return 204, b""
        if suffix in {"error-status", "error-default"}:
            status, error = self.rejected(); error["status" if suffix == "error-status" else "default"] = "PENDING_DELETE" if suffix == "error-status" else False; return status, error
        if suffix == "empty": return 202, b""
        if suffix == "malformed": return 202, b"{"
        if suffix == "nul": return 202, json.dumps(ack).encode() + b"\x00tail"
        if suffix == "404": return self.missing()
        if suffix == "404-generic": return 404, b"not found"
        if suffix == "id": ack["id"] = "external-id"
        elif suffix == "owner": ack["name"] = "external.example.com."
        elif suffix == "type": ack["type"] = "A"
        elif suffix == "zone": ack["zone_id"] = "external-zone"
        elif suffix == "status-active": ack["status"] = "ACTIVE"
        elif suffix == "missing-status": del ack["status"]
        elif suffix == "default": ack["default"] = True
        elif suffix == "default-string": ack["default"] = "false"
        elif suffix == "error-id": ack["error_code"] = "DNS.0401"; ack["error_msg"] = "contradiction"
        elif suffix == "records-multi": ack["records"].append('"external"')
        elif suffix == "value": ack["records"] = ['"external"']
        elif suffix == "denied": return self.rejected()
        elif suffix != "oom": raise AssertionError("unknown Huawei delete fault " + suffix)
        return 202, ack

    def verify(self):
        fault = self.fault
        assert not self.records and not self.pending, self.records
        if fault == "partial":
            assert self.calls["create"] == 2 and self.calls["read"] == 7 and self.calls["delete"] == 2, self.calls
            assert self.accepted == self.deleted == ["11", "12"]
            return
        assert self.calls["create"] == 1, self.calls
        expected_delete = 0 if fault in {"missing", "initial-pending"} else 2 if fault.startswith("uncommitted-") else 1
        expected_read = 3
        if fault == "missing": expected_read = 1
        elif fault == "initial-pending": expected_read = 2
        elif fault in {"pending-exhausted", "pending-active"}: expected_read = 7
        elif fault == "delayed-active": expected_read = 4
        elif fault.startswith("read-"): expected_read = 6 if fault == "read-lost" else 4
        elif fault.startswith("uncommitted-"): expected_read = 4 if fault in {"uncommitted-oom", "uncommitted-denied", "uncommitted-error-oom"} else 5
        elif fault.startswith("reconcile-") or fault.startswith("accepted-read-"): expected_read = 4
        if fault in {"reconcile-oom", "accepted-read-oom", "accepted-read-identity"}: expected_read = 3
        if fault in {"read-disabled", "read-frozen", "read-illegal", "read-police", "read-error"}: expected_read = 3
        if fault == "delete-presend": expected_read = 4
        assert self.calls["read"] == expected_read and self.calls["delete"] == expected_delete, self.calls
        assert self.accepted == ([] if fault in {"missing", "initial-pending"} else ["11"]), self.accepted
        assert self.deleted == ([] if fault == "missing" else ["11"]), self.deleted


for _provider in ("cf", "tencent", "huawei"):
    SCENARIOS += [(_provider, "rest-" + suffix, "rest-" + suffix if suffix in
                   {"normal", "rejected", "capacity", "concurrent", "isolation", "collision", "parse-oom", "error-oom", "presend-reset"} else "rest-unknown")
                  for suffix in ("normal", "rejected", "capacity", "concurrent", "isolation", "collision", "parse-oom", "error-oom",
                                 "lost", "500", "503", "404-malformed", "empty", "malformed", "nul", "missing-id", "unsafe-id", "error-id",
                                 "presend-reset", "ack-500", "error-500", "contradictory-403", "duplicate-id")]
    SCENARIOS += [(_provider, "rest-" + suffix, "rest-unknown") for suffix in
                  (("id-zero", "id-float", "missing-request") if _provider == "tencent" else ("owner", "type", "value"))]
SCENARIOS += [("tencent", "rest-" + suffix, "rest-unknown") for suffix in ("error-prefix", "internal-error")]
SCENARIOS += [("huawei", "rest-zone-id", "rest-unknown")]
SCENARIOS += [("huawei", "rest-error-message", "rest-unknown")]
SCENARIOS += [("tencent", "rest-" + name, "rest-unknown") for name in ("ack-202", "response-info")]
SCENARIOS += [("tencent", "rest-" + name, "rest-rejected") for name in ("operation-denied", "operation-denied-subclass")]
SCENARIOS += [("tencent", "rest-operation-denied-prefix", "rest-unknown")]
SCENARIOS += [("tencent", "tc-record-" + name, "rest-tc-record-" + name) for name in
              ("uncommitted-operation-denied", "uncommitted-operation-denied-prefix")]
SCENARIOS += [("tencent", "tc-record-late-index-" + name, "rest-tc-late-index-" + name) for name in
              ("nonempty", "empty", "repeat", "delete")]
SCENARIOS += [("tencent", "tc-domain-" + name, "rest-tc-domain-" + name) for name in
              ("id-zero", "id-float", "id-string", "missing-id", "name", "missing-request", "empty-request", "denied", "500", "malformed", "nul", "error-info", "wrong-missing", "missing-prefix", "oom")]
SCENARIOS += [("tencent", "tc-record-" + name, "rest-tc-record-" + name) for name in
              ("normal", "missing", "empty-inventory", "full-inventory", "early-index", "early-missing", "clock-back", "partial", "more-specific", "delete-presend", "handle-zero", "handle-overflow", "handle-injection", "domain-zero",
               *("read-" + name for name in ("id", "domain-id", "id-string", "owner", "value", "type", "line", "line-id", "enabled", "enabled-string", "disabled", "missing-id", "missing-request", "error-info", "denied", "500", "malformed", "nul", "generic-404", "missing-prefix", "list-error", "oom", "lost", "lost-once")),
               *("list-" + name for name in ("denied", "500", "malformed", "nul", "error-empty", "partial", "oversize", "count-string", "count-negative", "listed", "missing-count", "missing-request", "object", "id-zero", "id-string", "missing-id", "name", "type", "value", "name-nul", "type-nul", "value-nul", "status", "duplicate", "owned", "contradiction", "oom")),
               *("domain-" + name for name in ("id", "name", "denied", "oom")),
               *(outcome + "-" + name for outcome in ("committed", "uncommitted") for name in ("lost", "500", "http-202", "empty", "malformed", "nul", "missing", "missing-request", "empty-request", "id", "id-string", "error-info", "oom")),
               "uncommitted-denied", "uncommitted-error-oom", "reconcile-denied", "reconcile-malformed", "reconcile-oom")]
SCENARIOS += [("cf", "cf-record-" + suffix, "rest-cf-record-" + suffix) for suffix in
              ("normal", "ack-minimal", "missing", "partial",
               *("read-" + name for name in ("id", "owner", "type", "value", "missing-id", "missing-errors", "success-string",
                   "success-false", "error-id", "denied", "500", "malformed", "nul", "404", "404-empty", "404-result", "404-mixed", "404-code-string", "404-success", "404-no-message", "missing-200", "duplicate-id", "oom", "lost", "lost-once")),
               *(outcome + "-" + name for outcome in ("committed", "uncommitted") for name in
                 ("lost", "500", "empty", "malformed", "nul", "404", "404-generic", "id", "missing-id", "success-string", "success-false", "errors-object", "error-id", "oom")),
               "uncommitted-denied", "uncommitted-error-oom", "reconcile-denied", "reconcile-malformed", "reconcile-oom")]
SCENARIOS += [(provider, "rest-zone-" + fault, "rest-zone-" + fault) for provider in ("cf", "huawei")
              for fault in REST_ZONE_FAULTS]
SCENARIOS += [(provider, "rest-zone-input-" + fault, "rest-zone-input-" + fault) for provider in ("cf", "huawei")
              for fault in REST_ZONE_INPUT_FAULTS]
SCENARIOS += [(provider, "rest-owned-" + fault, "rest-owned-" + fault) for provider in ("cf", "tencent", "huawei")
              for fault in (*REST_OWNED_FAULTS, *( ("domain-replaced", "domain-renamed", "domain-denied", "domain-oom", "domain-zero", "read-disabled") if provider == "tencent" else
                 ("read-busy-PENDING_CREATE", "read-busy-PENDING_UPDATE", "read-deleting",
                  *("read-inactive-" + status for status in ("DISABLE", "FREEZE", "ILLEGAL", "POLICE", "ERROR"))) if provider == "huawei" else ()))]
SCENARIOS += [("huawei", "hw-record-" + suffix, "rest-hw-record-" + suffix) for suffix in
              ("normal", "ack-no-records", "missing", "initial-pending", "delayed-active", "pending-exhausted", "pending-active", "partial", "delete-presend",
               *("read-" + name for name in ("id", "owner", "type", "zone", "value", "multi-value", "default", "default-string",
                   "missing-default", "missing-status", "status-unknown", "busy-create", "busy-update", "busy-freeze", "busy-disable",
                   "error-message", "denied", "500", "malformed", "nul", "404-generic", "404-zone", "404-prefix", "404-id", "404-no-message",
                   "404-status", "404-default", "missing-200", "duplicate-id", "oom", "lost", "lost-once", "disabled", "frozen", "illegal", "police", "error")),
               *(outcome + "-" + name for outcome in ("committed", "uncommitted") for name in
                 ("lost", "500", "empty", "malformed", "nul", "404", "404-generic", "id", "owner", "type", "zone", "status-active", "missing-status",
                  "default", "default-string", "error-id", "records-multi", "value", "oom", "http-200", "http-204", "error-status", "error-default")),
               "uncommitted-denied", "uncommitted-error-oom", "reconcile-denied", "reconcile-malformed", "reconcile-oom",
               "accepted-read-denied", "accepted-read-oom", "accepted-read-identity")]


SCENARIOS += [("tencent", "tc-zone-" + fault, "rest-tc-zone-" + fault) for fault in TC_ZONE_FAULTS]


def case(client: Path, ca: Path, context: ssl.SSLContext, provider: str, scenario: str, mode: str):
    service = (AwsZoneService(provider, scenario) if scenario.startswith("aws-zone-") else
               AwsErrorService(provider, scenario) if scenario.startswith("aws-error-") else
               CfRecordService(provider, scenario) if scenario.startswith("cf-record-") else
               HuaweiRecordService(provider, scenario) if scenario.startswith("hw-record-") else
               TencentRecordService(provider, scenario) if scenario.startswith(("tc-record-", "tc-domain-")) else
               TencentZoneService(provider, scenario) if scenario.startswith("tc-zone-") else
               RestOwnedService(provider, scenario) if scenario.startswith("rest-owned-") else
               RestZoneInputService(provider, scenario) if scenario.startswith("rest-zone-input-") else
               RestZoneService(provider, scenario) if scenario in REST_ZONE_SCENARIOS else
               RestCreateService(provider, scenario) if scenario.startswith("rest-") else Service(provider, scenario))
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0)); listener.listen(16)
    listeners = [listener]
    # localhost can resolve to either family. An unserved ::1 adds a slow
    # refused-connect fallback on Windows to every otherwise valid RPC.
    if socket.has_ipv6:
        ipv6 = None
        try:
            ipv6 = socket.socket(socket.AF_INET6)
            ipv6.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
            ipv6.bind(("::1", listener.getsockname()[1])); ipv6.listen(16)
            listeners.append(ipv6)
        except OSError:
            if ipv6 is not None: ipv6.close()  # IPv6 may be disabled at socket creation too.
    selector = selectors.DefaultSelector()
    for peer_listener in listeners:
        peer_listener.setblocking(False)
        selector.register(peer_listener, selectors.EVENT_READ)
    endpoint = f"localhost:{listener.getsockname()[1]}"
    stop = threading.Event()
    errors = []

    def serve():
        while not stop.is_set():
            for ready, _ in selector.select(timeout=0.1):
                try:
                    raw, _ = ready.fileobj.accept()
                except BlockingIOError:
                    continue
                try:
                    raw.settimeout(8)
                    with context.wrap_socket(raw, server_side=True) as peer:
                        method, target, headers, body = read_request(peer)
                        assert headers["host"] == endpoint
                        if provider == "aws" and method == "POST":
                            assert headers["content-type"] == "application/xml"
                        response = service.dispatch(method, target, headers, body)
                        if response is None:
                            continue  # Ragged TLS close: the request has already reached the service.
                        status, payload = response
                        if isinstance(payload, dict):
                            payload = json.dumps(payload, separators=(",", ":")).encode()
                        head = f"HTTP/1.1 {status} Fixture\r\nContent-Length: {len(payload)}\r\nConnection: close\r\n\r\n".encode()
                        peer.sendall(head + payload)
                except Exception as exc:
                    raw.close(); errors.append(repr(exc)); return

    worker = threading.Thread(target=serve, daemon=True); worker.start()
    try:
        result = subprocess.run([str(client), endpoint, str(ca), provider, mode],
                                capture_output=True, text=True,
                                timeout=180 if mode in {"capacity", "concurrent", "discovery-cache-capacity",
                                                      "alias-capacity-owned", "alias-concurrent-same", "collision-last-slot",
                                                      "aws-alias-capacity", "aws-alias-concurrent-same"} else 60)
    finally:
        stop.set(); worker.join(timeout=10); selector.close()
        for peer_listener in listeners:
            peer_listener.close()
    if worker.is_alive() or errors or result.returncode != 0:
        raise AssertionError(f"{provider}/{scenario}: rc={result.returncode} server={errors} calls={dict(service.calls)}\n{result.stderr}")
    service.verify()
    if scenario.startswith("rest-zone-input-oom-"):
        print(result.stdout.strip(), flush=True)
    print(f"  {provider}/{scenario}: passed ({dict(service.calls)})", flush=True)


def run_cases(client: Path, ca: Path, leaf: Path, key: Path, selected=None):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(leaf, key)
    def server_name(peer, name, ctx):
        if name != "localhost":
            raise ssl.SSLError("provider sent wrong SNI")
    context.set_servername_callback(server_name)
    failures = []
    for provider, scenario, mode in SCENARIOS:
        if selected and provider + "/" + scenario not in selected:
            continue
        try:
            case(client, ca, context, provider, scenario, mode)
        except Exception as exc:
            failures.append(str(exc)); print(str(exc), flush=True)
    if failures:
        raise AssertionError("provider wire failures:\n" + "\n".join(failures))
    print("ACME independent provider TLS wire tests passed", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--coverage-dir", type=Path)
    parser.add_argument("--case", action="append", choices=[p + "/" + s for p, s, _ in SCENARIOS])
    parser.add_argument("--client", type=Path, help="use an already compiled probe")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="xacme-provider-wire-") as temp:
        root = Path(temp)
        build_root = args.coverage_dir.resolve() if args.coverage_dir else root
        build_root.mkdir(parents=True, exist_ok=True)
        client = args.client.resolve() if args.client else build_client(build_root, args.compiler, args.sanitize, bool(args.coverage_dir))
        ca, _, leaf, _, key = certificates(root)
        run_cases(client, ca, leaf, key, args.case)


if __name__ == "__main__":
    main()
