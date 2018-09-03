#include <windows.h>
#include <iostream>
#include <unordered_map>
#include <tchar.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
#include "napi.h"
using namespace std;
using namespace Napi;

typedef std::unordered_map<DWORD, std::string> PROCESSESMAP;

/**
 * Retrieve Process Name and ID
 */ 
BOOL GetProcessNameAndId(PROCESSESMAP* procMap) {
    PROCESSENTRY32 pe32;
 
    // Take a snapshot of all processes in the system.
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return false;
    }
 
    // Set the size of the structure before using it.
    pe32.dwSize = sizeof(PROCESSENTRY32);
 
    // Retrieve information about the first process,
    // and exit if unsuccessful
    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle( hProcessSnap ); // clean the snapshot object
        return false;
    }
 
    do {
        procMap->insert(make_pair(pe32.th32ProcessID, pe32.szExeFile));
    } while (Process32Next(hProcessSnap, &pe32));
 
    CloseHandle(hProcessSnap);
    return true;
}
 

/*
 * Enumerate Windows Services
 */
Value enumServicesStatus(const CallbackInfo& info) {
    Env env = info.Env();

    // Get a handle to the SCM database. 
    SC_HANDLE SCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (NULL == SCManager) {
        Error::New(env, "Open SCManager failed").ThrowAsJavaScriptException();
        printf("OpenSCManager failed (%d)\n", GetLastError());

        return env.Null();
    }
    
    // Declare required variables
    DWORD bytesNeeded = 0;
    DWORD servicesNum;
    BOOL status;

    status = EnumServicesStatusExA(
        SCManager,
        SC_ENUM_PROCESS_INFO,
        SERVICE_WIN32,
        SERVICE_STATE_ALL,
        NULL,
        0,
        &bytesNeeded,
        &servicesNum,
        NULL,
        NULL
    );
 
    PBYTE lpBytes = (PBYTE) malloc(bytesNeeded);
    status = EnumServicesStatusExA(
        SCManager,
        SC_ENUM_PROCESS_INFO,
        SERVICE_WIN32,
        SERVICE_STATE_ALL,
        lpBytes,
        bytesNeeded,
        &bytesNeeded,
        &servicesNum,
        NULL,
        NULL
    );
    if (!status) {
        Error::New(env, "Execution of EnumServicesStatusEx failed").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    // Retrieve processes
    PROCESSESMAP* processesMap = new PROCESSESMAP;
    bool gPnresult = GetProcessNameAndId(processesMap);

    ENUM_SERVICE_STATUS_PROCESS* lpServiceStatus = (ENUM_SERVICE_STATUS_PROCESS*) lpBytes;
    Array ret = Array::New(env); 

    for (DWORD i = 0; i < servicesNum; i++) {
        ENUM_SERVICE_STATUS_PROCESS service = lpServiceStatus[i];
        DWORD processId = service.ServiceStatusProcess.dwProcessId;

        // Create and push JavaScript Object
        Object JSObject = Object::New(env);
        ret[i] = JSObject;

        Object statusProcess = Object::New(env);
        statusProcess.Set("id", processId);
        statusProcess.Set("name", gPnresult ? processesMap->find(processId)->second.c_str() : "");
        statusProcess.Set("currentState", service.ServiceStatusProcess.dwCurrentState);
        statusProcess.Set("serviceType", service.ServiceStatusProcess.dwServiceType);
        statusProcess.Set("checkPoint", service.ServiceStatusProcess.dwCheckPoint);
        statusProcess.Set("controlsAccepted", service.ServiceStatusProcess.dwControlsAccepted);
        statusProcess.Set("serviceFlags", service.ServiceStatusProcess.dwServiceFlags);
        statusProcess.Set("serviceSpecificExitCode", service.ServiceStatusProcess.dwServiceSpecificExitCode);
        statusProcess.Set("waitHint", service.ServiceStatusProcess.dwWaitHint);
        statusProcess.Set("win32ExitCode", service.ServiceStatusProcess.dwWin32ExitCode);

        JSObject.Set("name", service.lpServiceName);
        JSObject.Set("displayName", service.lpDisplayName);
        JSObject.Set("process", statusProcess);
    }
 
    free(lpBytes);
    CloseServiceHandle(SCManager);

    return ret;
}

/*
 * Retrieve Service configuration
 */
Value getServiceConfiguration(const CallbackInfo& info) {
    Env env = info.Env();

        // Check argument length!
    if (info.Length() < 1) {
        Error::New(env, "Wrong number of argument provided!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // DriveName should be typeof string
    if (!info[0].IsString()) {
        Error::New(env, "argument serviceName should be typeof string!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Retrieve service Name
    string serviceName = info[0].As<String>().Utf8Value();
    Object ret = Object::New(env);

    LPQUERY_SERVICE_CONFIG lpsc; 
    LPSERVICE_DESCRIPTION lpsd;
    DWORD dwBytesNeeded, cbBufSize, dwError; 

    // Get a handle to the SCM database. 
    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);  
    if (NULL == schSCManager) {
        printf("OpenSCManager failed (%d)\n", GetLastError());

        return env.Null();
    }

    // Get a handle to the service.
    SC_HANDLE schService = OpenServiceA(schSCManager, serviceName.c_str(), SERVICE_QUERY_CONFIG); 
    if (schService == NULL) { 
        printf("OpenService failed (%d)\n", GetLastError()); 
        CloseServiceHandle(schSCManager);

        return env.Null();
    }

    // Get the configuration information.
    if(!QueryServiceConfig(schService, NULL, 0, &dwBytesNeeded)) {
        dwError = GetLastError();
        if(ERROR_INSUFFICIENT_BUFFER == dwError) {
            cbBufSize = dwBytesNeeded;
            lpsc = (LPQUERY_SERVICE_CONFIG) LocalAlloc(LMEM_FIXED, cbBufSize);
        }
        else {
            printf("QueryServiceConfig failed (%d)", dwError);
            goto cleanup; 
        }
    }
  
    if(!QueryServiceConfig(schService, lpsc, cbBufSize, &dwBytesNeeded) ) {
        printf("QueryServiceConfig failed (%d)", GetLastError());
        goto cleanup;
    }

    if(!QueryServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &dwBytesNeeded)) {
        dwError = GetLastError();
        if(ERROR_INSUFFICIENT_BUFFER == dwError) {
            cbBufSize = dwBytesNeeded;
            lpsd = (LPSERVICE_DESCRIPTION) LocalAlloc(LMEM_FIXED, cbBufSize);
        }
        else {
            printf("QueryServiceConfig2 failed (%d)", dwError);
            goto cleanup; 
        }
    }
 
    if (!QueryServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, (LPBYTE) lpsd, cbBufSize, &dwBytesNeeded)) {
        printf("QueryServiceConfig2 failed (%d)", GetLastError());
        goto cleanup;
    }
 
    // Print the configuration information.
    ret.Set("type", lpsc->dwServiceType);
    ret.Set("startType", lpsc->dwStartType);
    ret.Set("errorControl", lpsc->dwErrorControl);
    ret.Set("binaryPath", lpsc->lpBinaryPathName);
    ret.Set("account", lpsc->lpServiceStartName);

    if (lpsd->lpDescription != NULL && lstrcmp(lpsd->lpDescription, TEXT("")) != 0) {
        ret.Set("description", lpsd->lpDescription);
    }
    if (lpsc->lpLoadOrderGroup != NULL && lstrcmp(lpsc->lpLoadOrderGroup, TEXT("")) != 0) {
        ret.Set("loadOrderGroup", lpsc->lpLoadOrderGroup);
    }
    if (lpsc->dwTagId != 0) {
        ret.Set("tagId", lpsc->dwTagId);
    }
    if (lpsc->lpDependencies != NULL && lstrcmp(lpsc->lpDependencies, TEXT("")) != 0) {
        ret.Set("dependencies", lpsc->lpDependencies);
    }
 
    LocalFree(lpsc); 
    LocalFree(lpsd);

    cleanup:
        CloseServiceHandle(schService); 
        CloseServiceHandle(schSCManager);

    return ret;
}

/*
 * Convert GUID to std::string
 */
string GuidToString(GUID guid) {
	char guid_cstr[39];
	snprintf(guid_cstr, sizeof(guid_cstr),
	         "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
	         guid.Data1, guid.Data2, guid.Data3,
	         guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
	         guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

	return string(guid_cstr);
}

/*
 * Retrieve Service triggers
 */
Value getServiceTriggers(const CallbackInfo& info) {
    Env env = info.Env();

        // Check argument length!
    if (info.Length() < 1) {
        Error::New(env, "Wrong number of argument provided!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // DriveName should be typeof string
    if (!info[0].IsString()) {
        Error::New(env, "argument serviceName should be typeof string!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Retrieve service Name
    string serviceName = info[0].As<String>().Utf8Value();
    DWORD dwBytesNeeded, cbBufSize, dwError;

    // Get a handle to the SCM database. 
    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);  
    if (NULL == schSCManager) {
        printf("OpenSCManager failed (%d)\n", GetLastError());

        return env.Null();
    }

    // Get a handle to the service.
    SC_HANDLE schService = OpenServiceA(schSCManager, serviceName.c_str(), SERVICE_QUERY_CONFIG); 
    if (schService == NULL) { 
        printf("OpenService failed (%d)\n", GetLastError()); 
        CloseServiceHandle(schSCManager);

        return env.Null();
    }

    DWORD bytesNeeded = 0;
    SERVICE_TRIGGER_INFO triggers;
    Array ret = Array::New(env);

    bool result = QueryServiceConfig2A(schService, SERVICE_CONFIG_TRIGGER_INFO, NULL, 0, &dwBytesNeeded);
    if (!result) {
        dwError = GetLastError();
        if(ERROR_INSUFFICIENT_BUFFER == dwError) {
            cbBufSize = dwBytesNeeded;
        }
        else {
            Error::New(env, "QueryServiceConfig2 (1) failed").ThrowAsJavaScriptException();
            printf("QueryServiceConfig2 (1) failed (%d)\n", dwError);
            goto cleanup; 
        }
    }

    if (!QueryServiceConfig2(schService, SERVICE_CONFIG_TRIGGER_INFO, (LPBYTE) &triggers, cbBufSize, &dwBytesNeeded)) {
        Error::New(env, "QueryServiceConfig2 (2) failed").ThrowAsJavaScriptException();
        printf("QueryServiceConfig2 (2) failed (%d)\n", GetLastError());
        goto cleanup;
    }

    if (triggers.cTriggers == 0) {
        goto cleanup;
    }

    for (DWORD i = 0; i < triggers.cTriggers; i++) {
        SERVICE_TRIGGER serviceTrigger = triggers.pTriggers[i];
        std::string guid = GuidToString(*serviceTrigger.pTriggerSubtype);

        Object trigger = Object::New(env);
        Array dataItems = Array::New(env);

        trigger.Set("type", serviceTrigger.dwTriggerType);
        trigger.Set("action", serviceTrigger.dwAction);
        trigger.Set("dataItems", dataItems);
        trigger.Set("guid", guid.c_str());

        ret[i] = trigger;

        // for (DWORD j = 0; j < serviceTrigger.cDataItems; j++) {
        //     SERVICE_TRIGGER_SPECIFIC_DATA_ITEM pServiceTrigger = serviceTrigger.pDataItems[j];
        //     Object specificDataItems = Object::New(env);
        //     dataItems[j] = specificDataItems;

        //     specificDataItems.Set("dataType", pServiceTrigger.dwDataType);
        // }
    }

    cleanup:
        CloseServiceHandle(schService); 
        CloseServiceHandle(schSCManager);

    return ret;
}

// Initialize Native Addon
Object Init(Env env, Object exports) {

    // Setup methods
    // TODO: Launch with AsyncWorker to avoid event loop starvation
    exports.Set("enumServicesStatus", Function::New(env, enumServicesStatus));
    exports.Set("getServiceConfiguration", Function::New(env, getServiceConfiguration));
    exports.Set("getServiceTriggers", Function::New(env, getServiceTriggers));

    return exports;
}

// Export
NODE_API_MODULE(Winservices, Init)
