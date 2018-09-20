#ifndef SCMANAGER_H
#define SCMANAGER_H

#include <windows.h>
#include <string>

using namespace std;

struct EnumServicesResponse {
    bool status = true;
    ENUM_SERVICE_STATUS_PROCESS *lpBytes;
};

struct ServiceConfigResponse {
    bool status;
    LPQUERY_SERVICE_CONFIG lpsc;
};

class SCManager {
    public:
        SCManager();
        bool Open(DWORD dwDesiredAccess);
        bool Close();
        bool DeclareService(string serviceName, DWORD dwAccessRight);
        EnumServicesResponse EnumServicesStatus(DWORD serviceState, DWORD* servicesNum);
        ServiceConfigResponse ServiceConfig();
        template<typename T> T ServiceConfig2(DWORD dwInfoLevel, bool allocate);
        bool isOpen;
        SC_HANDLE manager;
        SC_HANDLE service;
};

// SCManager Constructor
SCManager::SCManager() {
    isOpen = false;
    manager = NULL;
    service = NULL;
}

/*
 * Open SCManager
 * 
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/nf-winsvc-openscmanagera
 */
bool SCManager::Open(DWORD dwDesiredAccess) {
    if (dwDesiredAccess == NULL) {
        dwDesiredAccess = SC_MANAGER_ENUMERATE_SERVICE;
    }
    manager = OpenSCManager(NULL, NULL, dwDesiredAccess);
    if (NULL == manager) {
        return false;
    }

    isOpen = true;
    return true;
}

/*
 * Close SCManager (and service handle is there is one!)
 * 
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/nf-winsvc-closeservicehandle
 */
bool SCManager::Close() {
    if (!isOpen) {
        return false;
    }
    if (service != NULL) {
        CloseServiceHandle(service);
    }
    CloseServiceHandle(manager);
    isOpen = false;

    return true;
}

/*
 * Open a new Service handle
 * 
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/nf-winsvc-openservicea
 * @note: OpenService "A" is used to support const char*
 */
bool SCManager::DeclareService(string serviceName, DWORD dwAccessRight) {
    service = OpenServiceA(manager, serviceName.c_str(), dwAccessRight); 
    if (service == NULL) { 
        Close();
        return false;
    }

    return true;
}

/*
 * Enumate Services Status
 * 
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/nf-winsvc-enumservicesstatusexa
 */
EnumServicesResponse SCManager::EnumServicesStatus(DWORD serviceState, DWORD* servicesNum) {
    void* buf = NULL;
    DWORD bufSize = 0;
    DWORD bytesNeeded = 0;
    EnumServicesResponse ret;

    for(;;) {
        ret.status = EnumServicesStatusExA(
            manager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, serviceState, (LPBYTE) buf, bufSize, &bytesNeeded, servicesNum, NULL, NULL
        );

        // If status is true, then return the response
        if (ret.status) {
            ret.lpBytes = (ENUM_SERVICE_STATUS_PROCESS*) buf;
            return ret;
        }

        // If last error is different of ERROR_MORE_DATA (insufficient buffer size), then return false & close.
        if (ERROR_MORE_DATA != GetLastError()) {
            free(buf);
            Close();
            return ret;
        }

        // Re-allocate buffer as much is needed!
        bufSize += bytesNeeded;
        free(buf);
        buf = malloc(bufSize);
    }
}

/**
 * Retrieve Service Configuration
 * 
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/nf-winsvc-queryserviceconfiga
 */
ServiceConfigResponse SCManager::ServiceConfig() {
    DWORD dwBytesNeeded, cbBufSize;
    ServiceConfigResponse ret;
    ret.status = false;

    if(!QueryServiceConfig(service, NULL, 0, &dwBytesNeeded)) {
        if(ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
            Close();
            return ret;
        }

        // Re-allocation buffer size
        cbBufSize = dwBytesNeeded;
        ret.lpsc = (LPQUERY_SERVICE_CONFIG) LocalAlloc(LMEM_FIXED, cbBufSize);
    }
  
    if(!QueryServiceConfig(service, ret.lpsc, cbBufSize, &dwBytesNeeded) ) {
        Close();
        return ret;
    }

    ret.status = true;
    return ret;
}

#endif // SCMANAGER_H
