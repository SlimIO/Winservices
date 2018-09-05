#include <windows.h>
#include <tlhelp32.h>
#include "SCManager.h"
#include "napi.h"
#include <unordered_map>
#include <string>

using namespace std;
using namespace Napi;

typedef unordered_map<DWORD, string> PROCESSESMAP;

/*
 * Convert GUID to std::string
 */
string guidToString(GUID guid) {
	char guid_cstr[39];
	snprintf(guid_cstr, sizeof(guid_cstr),
	         "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
	         guid.Data1, guid.Data2, guid.Data3,
	         guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
	         guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

	return string(guid_cstr);
}

/**
 * Retrieve Process Name and ID
 * 
 * @header: tlhelp32.h
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/tlhelp32/nf-tlhelp32-createtoolhelp32snapshot
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/tlhelp32/nf-tlhelp32-process32first
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/tlhelp32/nf-tlhelp32-process32next
 */ 
BOOL getProcessNameAndId(PROCESSESMAP* procMap) {
    PROCESSENTRY32 pe32;
    HANDLE hProcessSnap;
 
    // Take a snapshot of all processes in the system.
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return false;
    }
 
    // Set the size of the structure before using it.
    pe32.dwSize = sizeof(PROCESSENTRY32);
 
    // Retrieve information about the first process,
    // and exit if unsuccessful
    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap); // clean the snapshot object
        return false;
    }
    
    // Insert rows in the processes map
    do {
        wstring wSzExeFile((wchar_t*) pe32.szExeFile);
        procMap->insert(make_pair(pe32.th32ProcessID, string(wSzExeFile.begin(), wSzExeFile.end())));
    } while (Process32Next(hProcessSnap, &pe32));
 
    CloseHandle(hProcessSnap);
    return true;
}

/*
 * Get the desired DWORD service state!
 */
DWORD getServiceState(int32_t desiredState) {
    DWORD state;
    switch(desiredState) {
        case 0:
            state = SERVICE_ACTIVE;
            break;
        case 1:
            state = SERVICE_INACTIVE;
            break;
        case 2:
            state = SERVICE_STATE_ALL;
            break;
    }

    return state;
}
 
/*
 * Enumerate Windows Services
 * 
 * @header: windows.h
 */
Value enumServicesStatus(const CallbackInfo& info) {
    Env env = info.Env();

    // Instanciate Variables
    BOOL success;
    SCManager manager;
    DWORD servicesNum;
    DWORD serviceState = SERVICE_STATE_ALL;

    // Check if there is less than one argument, if then throw a JavaScript exception
    if (info.Length() == 1) {
        if (!info[0].IsNumber()) {
            Error::New(env, "argument desiredState should be typeof number!").ThrowAsJavaScriptException();
            return env.Null();
        }

        // Retrieve service Name
        int32_t desiredState = info[0].As<Number>().Int32Value();
        serviceState = getServiceState(desiredState);
    }

    // Open SC-Manager
    success = manager.Open(SC_MANAGER_ENUMERATE_SERVICE);
    if (!success) {
        Error::New(env, "Open SCManager failed").ThrowAsJavaScriptException();
        // printf("OpenSCManager failed (%d)\n", GetLastError());
        return env.Null();
    }

    // Enumerate services
    EnumServicesResponse enumRet = manager.EnumServicesStatus(serviceState, &servicesNum);
    if (!enumRet.status) {
        Error::New(env, "Execution of EnumServicesStatusEx failed").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Retrieve processes name and id!
    PROCESSESMAP* processesMap = new PROCESSESMAP;
    success = getProcessNameAndId(processesMap);

    ENUM_SERVICE_STATUS_PROCESS* lpServiceStatus = (ENUM_SERVICE_STATUS_PROCESS*) enumRet.lpBytes;
    Array ret = Array::New(env); 
    for (DWORD i = 0; i < servicesNum; i++) {
        ENUM_SERVICE_STATUS_PROCESS service = lpServiceStatus[i];
        DWORD processId = service.ServiceStatusProcess.dwProcessId;

        // Create and push JavaScript Object
        Object JSObject = Object::New(env);
        ret[i] = JSObject;

        Object statusProcess = Object::New(env);
        statusProcess.Set("id", processId);
        statusProcess.Set("name", success ? processesMap->find(processId)->second.c_str() : "");
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
    
    // Close manager
    manager.Close();
    free(enumRet.lpBytes);

    return ret;
}

/*
 * Retrieve Service configuration
 * 
 * @header: windows.h
 */
Value getServiceConfiguration(const CallbackInfo& info) {
    Env env = info.Env();

    // Instanciate Variables
    SCManager manager;
    BOOL success;

    // Check if there is less than one argument, if then throw a JavaScript exception
    if (info.Length() < 1) {
        Error::New(env, "Wrong number of argument provided!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // The first argument (serviceName) should be typeof Napi::String
    if (!info[0].IsString()) {
        Error::New(env, "argument serviceName should be typeof string!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Retrieve service Name
    string serviceName = info[0].As<String>().Utf8Value();
    Object ret = Object::New(env);

    // Open Manager!
    success = manager.Open(SC_MANAGER_ENUMERATE_SERVICE);
    if (!success) {
        Error::New(env, "Open SCManager failed").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Open Service!
    success = manager.DeclareService(serviceName);
    if (!success) {
        Error::New(env, "Failed to Open service!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Retrieve Service Config!
    ServiceConfigResponse serviceConfig = manager.ServiceConfig();
    if (!serviceConfig.status) {
        Error::New(env, "Failed to Query Service Config!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Service description
    // LPSERVICE_DESCRIPTIONA lpsd = manager.ServiceConfig2<LPSERVICE_DESCRIPTIONA>(SERVICE_CONFIG_DESCRIPTION);

    // Setup properties
    ret.Set("type", serviceConfig.lpsc->dwServiceType);
    ret.Set("startType", serviceConfig.lpsc->dwStartType);
    ret.Set("errorControl", serviceConfig.lpsc->dwErrorControl);
    ret.Set("binaryPath", serviceConfig.lpsc->lpBinaryPathName);
    ret.Set("account", serviceConfig.lpsc->lpServiceStartName);

    // if (lpsd != NULL && lpsd->lpDescription != NULL) {
    //     ret.Set("description", lpsd->lpDescription);
    // }
    if (serviceConfig.lpsc->lpLoadOrderGroup != NULL && lstrcmp(serviceConfig.lpsc->lpLoadOrderGroup, TEXT("")) != 0) {
        ret.Set("loadOrderGroup", serviceConfig.lpsc->lpLoadOrderGroup);
    }
    if (serviceConfig.lpsc->dwTagId != 0) {
        ret.Set("tagId", serviceConfig.lpsc->dwTagId);
    }
    if (serviceConfig.lpsc->lpDependencies != NULL && lstrcmp(serviceConfig.lpsc->lpDependencies, TEXT("")) != 0) {
        ret.Set("dependencies", serviceConfig.lpsc->lpDependencies);
    }
    
    cleanup:
    // Free handle/memory
    LocalFree(serviceConfig.lpsc); 
    // if (lpsd != NULL) {
    //     LocalFree(lpsd);
    // }
    manager.Close();

    return ret;
}

/*
 * Retrieve Service triggers
 * 
 * @header: windows.h
 */
Value getServiceTriggers(const CallbackInfo& info) {
    Env env = info.Env();

    // Instanciate Variables
    SCManager manager;
    BOOL success;
    DWORD dwBytesNeeded, cbBufSize, dwError;

    // Check if there is less than one argument, if then throw a JavaScript exception
    if (info.Length() < 1) {
        Error::New(env, "Wrong number of argument provided!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // The first argument (serviceName) should be typeof Napi::String
    if (!info[0].IsString()) {
        Error::New(env, "argument serviceName should be typeof string!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Retrieve service Name
    string serviceName = info[0].As<String>().Utf8Value();

    // Open Manager!
    success = manager.Open(SC_MANAGER_ENUMERATE_SERVICE);
    if (!success) {
        Error::New(env, "Open SCManager failed").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Open Service!
    success = manager.DeclareService(serviceName);
    if (!success) {
        Error::New(env, "Failed to Open service!").ThrowAsJavaScriptException();
        return env.Null();
    }

    DWORD bytesNeeded = 0;
    SERVICE_TRIGGER_INFO triggers;
    Array ret = Array::New(env);

    success = QueryServiceConfig2A(manager.service, SERVICE_CONFIG_TRIGGER_INFO, NULL, 0, &dwBytesNeeded);
    if (!success) {
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

    if (!QueryServiceConfig2(manager.service, SERVICE_CONFIG_TRIGGER_INFO, (LPBYTE) &triggers, cbBufSize, &dwBytesNeeded)) {
        Error::New(env, "QueryServiceConfig2 (2) failed").ThrowAsJavaScriptException();
        printf("QueryServiceConfig2 (2) failed (%d)\n", GetLastError());
        goto cleanup;
    }

    if (triggers.cTriggers == 0) {
        goto cleanup;
    }

    for (DWORD i = 0; i < triggers.cTriggers; i++) {
        SERVICE_TRIGGER serviceTrigger = triggers.pTriggers[i];
        std::string guid = guidToString(*serviceTrigger.pTriggerSubtype);

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
        manager.Close();

    return ret;
}

/*
 * Initialize Node.JS Binding exports
 * 
 * @header: napi.h
 */
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
