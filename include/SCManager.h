#ifndef SCMANAGER_H
#define SCMANAGER_H

#include <windows.h>
#include <string>

using namespace std;

struct EnumServicesResponse {
    bool status;
    PBYTE lpBytes;
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
        // printf("OpenSCManager failed (%d)\n", GetLastError());

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
 * @note: OpenService "A" is used to support char*
 */
bool SCManager::DeclareService(string serviceName, DWORD dwAccessRight) {
    // SERVICE_QUERY_CONFIG
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
    // Instanciate Variables
    DWORD bytesNeeded = 0;
    EnumServicesResponse ret;

    // Made a first call to get bytesNeeded & servicesNum
    EnumServicesStatusExA(
        manager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, serviceState, NULL, 0, &bytesNeeded, servicesNum, NULL, NULL
    );
    
    // Allocate Memory for lpBytes
    ret.lpBytes = (PBYTE) malloc(bytesNeeded);
    ret.status = EnumServicesStatusExA(
        manager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, serviceState, ret.lpBytes, bytesNeeded, &bytesNeeded, servicesNum, NULL, NULL
    );

    if (!ret.status) {
        Close();
    }

    return ret;
}

// Query Service Config
ServiceConfigResponse SCManager::ServiceConfig() {
    DWORD dwBytesNeeded, cbBufSize;
    ServiceConfigResponse ret;
    ret.status = false;

    if(!QueryServiceConfig(service, NULL, 0, &dwBytesNeeded)) {
        if(ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
            Close();
            return ret;
        }
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

template<typename T>
T SCManager::ServiceConfig2(DWORD dwInfoLevel, bool allocate) {
    T lpsd;
    DWORD dwBytesNeeded, cbBufSize;

    if(!QueryServiceConfig2A(service, dwInfoLevel, NULL, 0, &dwBytesNeeded)) {
        if(ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
            Close();
            return NULL;
        }
        cbBufSize = dwBytesNeeded;
        if (allocate) {
            lpsd = (T) LocalAlloc(LMEM_FIXED, cbBufSize);
        }
    }
 
    if (!QueryServiceConfig2A(service, dwInfoLevel, (LPBYTE) &lpsd, cbBufSize, &dwBytesNeeded)) {
        Close();
        return NULL;
    }

    return lpsd;
}


#endif // SCMANAGER_H
