#!/bin/sh
set -eu
provider_source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
provider_test_dir=$(mktemp -d)
trap 'rm -rf "$provider_test_dir"' EXIT
for regression in udp-lifetime proxy-replies; do
    cat "$provider_source_dir/tests/provider-standin.swift" > "$provider_test_dir/main.swift"
    sed 's/^class AppProxyProvider: NETransparentProxyProvider {/class AppProxyProvider: RegressionProxyProvider {/' \
        "$provider_source_dir/ProxyBridge/extension/AppProxyProvider.swift" >> "$provider_test_dir/main.swift"
    cat "$provider_source_dir/tests/$regression.swift" >> "$provider_test_dir/main.swift"
    swiftc -O -suppress-warnings -module-cache-path "$provider_test_dir/cache" \
        "$provider_test_dir/main.swift" \
        "$provider_source_dir/ProxyBridge/extension/FlowFailure.swift" \
        "$provider_source_dir/ProxyBridge/extension/ProviderDiagnostics.swift" -o "$provider_test_dir/$regression"
    "$provider_test_dir/$regression"
done
