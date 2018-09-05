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
        bool DeclareService(string serviceName);
        EnumServicesResponse EnumServicesStatus(DWORD serviceState, DWORD* servicesNum);
        ServiceConfigResponse ServiceConfig();
        template<typename T> T ServiceConfig2(DWORD dwInfoLevel);
        bool isOpen;
        SC_HANDLE manager;
        SC_HANDLE service;
};

#endif // SCMANAGER_H
