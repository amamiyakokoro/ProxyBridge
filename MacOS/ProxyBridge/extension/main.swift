//
//  main.swift
//  extension
//
//  Created by sourav kalal on 13/11/25.
//

import Foundation
import NetworkExtension
import Darwin

// launchd can start the extension with a low soft descriptor limit. Keep the
// inherited hard limit and never lower an already larger soft limit.
var descriptorLimit = rlimit()
if getrlimit(RLIMIT_NOFILE, &descriptorLimit) == 0 {
    let target = min(rlim_t(10240), descriptorLimit.rlim_max)
    if descriptorLimit.rlim_cur < target {
        descriptorLimit.rlim_cur = target
        if setrlimit(RLIMIT_NOFILE, &descriptorLimit) != 0 {
            NSLog("Unable to raise file descriptor limit: errno=%d", errno)
        }
    }
}

autoreleasepool {
    NEProvider.startSystemExtensionMode()
}

dispatchMain()
