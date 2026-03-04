#pragma once

#include <QString>

#ifdef _WIN32

class FilterRegistrar {
public:
    enum class Status {
        Registered,       // DLL registered at correct path
        RegisteredStale,  // DLL registered but path doesn't match current location
        NotRegistered     // No registry entry found
    };

    // Check current registration status by reading the registry directly.
    // Fast and non-invasive (no COM, no CoCreateInstance).
    static Status checkStatus();

    // Returns the DLL path currently recorded in the registry, or empty string.
    static QString registeredDllPath();

    // Returns the expected DLL path (adjacent to the running exe).
    static QString expectedDllPath();

    // Register the filter via elevated regsvr32 (triggers UAC prompt).
    // Returns true if regsvr32 exited with code 0.
    static bool registerFilter();

    // Unregister the filter via elevated regsvr32 /u (triggers UAC prompt).
    // Returns true if regsvr32 exited with code 0.
    static bool unregisterFilter();

    // Human-readable status string for UI display.
    static QString statusText(Status s);
};

#endif // _WIN32
