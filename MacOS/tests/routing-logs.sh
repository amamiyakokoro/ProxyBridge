#!/bin/sh
set -eu
routing_source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
routing_test_dir=$(mktemp -d)
trap 'rm -rf "$routing_test_dir"' EXIT
cat "$routing_source_dir/ProxyBridge/extension/AppProxyProvider.swift" "$routing_source_dir/tests/routing-logs.swift" > "$routing_test_dir/main.swift"
swiftc -suppress-warnings -module-cache-path "$routing_test_dir/cache" "$routing_test_dir/main.swift" -o "$routing_test_dir/routing-logs"
"$routing_test_dir/routing-logs"
