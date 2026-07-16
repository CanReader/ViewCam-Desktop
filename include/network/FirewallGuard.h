#pragma once

#include <QString>

#ifdef _WIN32

// Checks (and can fix) whether Windows Firewall allows the inbound UDP
// discovery beacon this app listens for. Windows blocks inbound traffic to
// unrecognized apps by default on Private/Domain networks; the installer
// creates an exception at install time (elevated), but a dev build, a
// firewall reset, or a manually deleted rule can leave discovery silently
// broken with no obvious symptom other than "phone never shows up".
class FirewallGuard {
public:
    enum class Status {
        Allowed,  // an enabled inbound rule exists for this exe, this port, this network profile
        Blocked,  // rule missing / disabled / doesn't cover the current profile
        Unknown   // couldn't determine (COM failure)
    };

    // Read-only check via the Windows Firewall COM API. Fast, no elevation
    // required — safe to call on the UI thread at startup.
    static Status checkDiscoveryStatus();

    // Elevates (triggers a UAC consent prompt) and adds the inbound UDP
    // discovery rule via netsh, matching what the installer creates. Blocks
    // until the elevated command finishes or the user cancels — call this
    // off the UI thread. Returns true only if the rule was actually added.
    static bool requestApproval();

    static QString statusText(Status s);
};

#endif // _WIN32
