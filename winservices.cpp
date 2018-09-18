#include <windows.h>
#include <tlhelp32.h>
#include "SCManager.h"
#include "napi.h"
#include <iostream>
#include <iomanip>
#include <sstream>
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

/*
 * Retrieve last message error with code of GetLastError()
 */
string getLastErrorMessage() {
    char err[256];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), err, 255, NULL);
    return string(err);
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
class EnumServicesWorker : public AsyncWorker {
    public:
        EnumServicesWorker(Function& callback, DWORD serviceState) : AsyncWorker(callback), serviceState(serviceState) {}
        ~EnumServicesWorker() {}

    private:
        DWORD serviceState;
        ENUM_SERVICE_STATUS_PROCESS *lpServiceStatus;
        PROCESSESMAP *processesMap;
        DWORD servicesNum;

    // This code will be executed on the worker thread
    void Execute() {
        SCManager manager;
        BOOL success;

        // Retrieve processes name and id!
        processesMap = new PROCESSESMAP;
        success = getProcessNameAndId(processesMap);
        if (!success) {
            return SetError("Failed to get process names and ids");
        }

        // Open SC-Manager
        success = manager.Open(SC_MANAGER_ENUMERATE_SERVICE);
        if (!success) {
            return SetError("Open SCManager failed");
        }

        // Enumerate services
        EnumServicesResponse enumRet = manager.EnumServicesStatus(serviceState, &servicesNum);
        if (!enumRet.status) {
            return SetError("Failed to Enumerate Services");
        }

        lpServiceStatus = enumRet.lpBytes;
        manager.Close();
    }

    void OnError(const Error& e) {
        stringstream error;
        error << e.what() << getLastErrorMessage();
        Error::New(Env(), error.str()).ThrowAsJavaScriptException();
    }

    void OnOK() {
        HandleScope scope(Env());
        Array ret = Array::New(Env());
        unsigned arrIndex = 0;

        ENUM_SERVICE_STATUS_PROCESS service;
        DWORD processId;
        for (DWORD i = 0; i<servicesNum; ++i) {
            SecureZeroMemory(&service, sizeof(service));
            SecureZeroMemory(&processId, sizeof(processId));
            service = *lpServiceStatus;
            processId = service.ServiceStatusProcess.dwProcessId;
        
            // Create and push JavaScript Object
            Object JSObject = Object::New(Env());

            Object statusProcess = Object::New(Env());
            statusProcess.Set("id", processId);
            statusProcess.Set("name", processesMap->find(processId)->second.c_str());
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
            ret[arrIndex] = JSObject;

            lpServiceStatus++;
            arrIndex++;
        }

        Callback().Call({Env().Null(), ret});
    }

};
 
/*
 * Enumerate Windows Services (binding interface).
 */
Value enumServicesStatus(const CallbackInfo& info) {
    Env env = info.Env();
    int32_t desiredState;
    DWORD serviceState = SERVICE_STATE_ALL;

    // Check argument length!
    if (info.Length() < 2) {
        Error::New(env, "Wrong number of argument provided!").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[0].IsNumber()) {
        Error::New(env, "Argument desiredState should be typeof number!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // callback should be a Napi::Function
    if (!info[1].IsFunction()) {
        Error::New(env, "Argument callback should be a Function!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Execute worker
    desiredState = info[0].As<Number>().Int32Value();
    serviceState = getServiceState(desiredState);
    Function cb = info[1].As<Function>();
    (new EnumServicesWorker(cb, serviceState))->Queue();

    return env.Undefined();
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
    success = manager.DeclareService(serviceName, SERVICE_QUERY_CONFIG);
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

    // Setup properties
    ret.Set("type", serviceConfig.lpsc->dwServiceType);
    ret.Set("startType", serviceConfig.lpsc->dwStartType);
    ret.Set("errorControl", serviceConfig.lpsc->dwErrorControl);
    ret.Set("binaryPath", serviceConfig.lpsc->lpBinaryPathName);
    ret.Set("account", serviceConfig.lpsc->lpServiceStartName);

    if (serviceConfig.lpsc->lpLoadOrderGroup != NULL && lstrcmp(serviceConfig.lpsc->lpLoadOrderGroup, TEXT("")) != 0) {
        ret.Set("loadOrderGroup", serviceConfig.lpsc->lpLoadOrderGroup);
    }
    if (serviceConfig.lpsc->dwTagId != 0) {
        ret.Set("tagId", serviceConfig.lpsc->dwTagId);
    }
    if (serviceConfig.lpsc->lpDependencies != NULL && lstrcmp(serviceConfig.lpsc->lpDependencies, TEXT("")) != 0) {
        ret.Set("dependencies", serviceConfig.lpsc->lpDependencies);
    }
    
    // Free handle/memory
    LocalFree(serviceConfig.lpsc);
    manager.Close();

    return ret;
}

/*
 * Byte sequence to std::string
 */
string byteSeqToString(const unsigned char bytes[], size_t n) {
    ostringstream stm;
    stm << hex << uppercase;

    for(size_t i = 0; i < n; ++i) {
        stm << setw(2) << setfill('0') << unsigned(bytes[i]);
    }

    return stm.str();
}

/*
 * Retrieve Service triggers
 * 
 * @header: windows.h
 */
Value getServiceTriggers(const CallbackInfo& info) {
    Env env = info.Env();

    // Instanciate Variables
    void* buf = NULL;
    SCManager manager;
    BOOL success;
    SERVICE_TRIGGER_INFO *triggers = {0};
    DWORD dwBytesNeeded, cbBufSize;

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
    success = manager.DeclareService(serviceName, SERVICE_QUERY_CONFIG);
    if (!success) {
        Error::New(env, "Failed to Open service!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Query for TRIGGER INFO
    success = QueryServiceConfig2A(manager.service, SERVICE_CONFIG_TRIGGER_INFO, NULL, 0, &dwBytesNeeded);
    if (!success) {
        if(ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
            Error::New(env, "QueryServiceConfig2 (1) failed").ThrowAsJavaScriptException();
            manager.Close();

            return env.Null();
        }
        cbBufSize = dwBytesNeeded;
        free(buf);
        buf = malloc(cbBufSize);
    }

    if (!QueryServiceConfig2(manager.service, SERVICE_CONFIG_TRIGGER_INFO, (LPBYTE) buf, cbBufSize, &dwBytesNeeded)) {
        Error::New(env, "QueryServiceConfig2 (2) failed").ThrowAsJavaScriptException();
        manager.Close();

        return env.Null();
    }

    triggers = (SERVICE_TRIGGER_INFO*) buf;
    DWORD triggersLen = triggers->cTriggers;
    Array ret = Array::New(env, triggersLen);

    PSERVICE_TRIGGER currTrigger = triggers->pTriggers;
    for (unsigned i = 0; i < triggersLen; ++i, currTrigger++) {
        Object trigger = Object::New(info.Env());
        Array dataItems = Array::New(info.Env());

        trigger.Set("type", currTrigger->dwTriggerType);
        trigger.Set("action", currTrigger->dwAction);
        trigger.Set("dataItems", dataItems);

        string guid = guidToString(*currTrigger->pTriggerSubtype);
        trigger.Set("guid", guid.c_str());

        ret[i] = trigger;
        for (DWORD j = 0; j < currTrigger->cDataItems; j++) {
            SERVICE_TRIGGER_SPECIFIC_DATA_ITEM pServiceTrigger = currTrigger->pDataItems[j];
            Object specificDataItems = Object::New(env);
            dataItems[j] = specificDataItems;

            specificDataItems.Set("dataType", pServiceTrigger.dwDataType);
            if (pServiceTrigger.dwDataType == SERVICE_TRIGGER_DATA_TYPE_BINARY) {
                specificDataItems.Set("data", byteSeqToString(pServiceTrigger.pData, pServiceTrigger.cbData));
            }
            // else if(pServiceTrigger.dwDataType == SERVICE_TRIGGER_DATA_TYPE_STRING) {
            //     specificDataItems.Set("data", byteSeqToString(pServiceTrigger.pData, pServiceTrigger.cbData));
            // }
        }
    }

    // Cleanup ressources!
    manager.Close();

    free(buf);

    return ret;
}

/*
 * Enumerate dependent Services!
 * 
 * @header: windows.h
 */
Value enumDependentServices(const CallbackInfo& info) {
    Env env = info.Env();

    // Instanciate Variables
    void* buf = NULL;
    SCManager manager;
    BOOL success;
    LPENUM_SERVICE_STATUSA *lpServices = {0};
    DWORD dwBytesNeeded, cbBufSize, nbReturned;

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
    success = manager.DeclareService(serviceName, SERVICE_ENUMERATE_DEPENDENTS);
    if (!success) {
        Error::New(env, "Failed to Open service!").ThrowAsJavaScriptException();
        return env.Null();
    }

    success = EnumDependentServicesA(manager.service, SERVICE_STATE_ALL, NULL, 0, &dwBytesNeeded, &nbReturned);
    if (!success) {
        if(ERROR_MORE_DATA != GetLastError()) {
            Error::New(env, "EnumDependentServicesA (1) failed").ThrowAsJavaScriptException();
            manager.Close();

            return env.Null();
        }
        cbBufSize = dwBytesNeeded;
        free(buf);
        buf = malloc(cbBufSize);
    }

    if (!EnumDependentServicesA(manager.service, SERVICE_STATE_ALL, (LPENUM_SERVICE_STATUSA) buf, cbBufSize, &dwBytesNeeded, &nbReturned)) {
        Error::New(env, "EnumDependentServicesA (2) failed").ThrowAsJavaScriptException();
        manager.Close();

        return env.Null();
    }

    lpServices = (LPENUM_SERVICE_STATUSA*) buf;

    Array ret = Array::New(env);
    ENUM_SERVICE_STATUSA *service;
    for (DWORD i = 0; i < nbReturned; ++i) {
        SecureZeroMemory(&service, sizeof(service));
        service = *lpServices;
        Object JSObject = Object::New(env);
        ret[i] = JSObject;

        cout << "name: " << service->lpServiceName << endl;
        cout << "displayName: " << service->lpDisplayName << endl;

        JSObject.Set("name", service->lpServiceName);
        JSObject.Set("displayName", service->lpDisplayName);
        // Object statusProcess = Object::New(env);
        // statusProcess.Set("currentState", service.ServiceStatus.dwCurrentState);
        // statusProcess.Set("checkpoint", service.ServiceStatus.dwCheckPoint);
        // statusProcess.Set("controlsAccepted", service.ServiceStatus.dwControlsAccepted);
        // statusProcess.Set("serviceSpecificExitCode", service.ServiceStatus.dwServiceSpecificExitCode);
        // statusProcess.Set("serviceType", service.ServiceStatus.dwServiceType);
        // statusProcess.Set("waitHint", service.ServiceStatus.dwWaitHint);
        // statusProcess.Set("win32ExitCode", service.ServiceStatus.dwWin32ExitCode);
        
        // JSObject.Set("process", statusProcess);
        lpServices++;
    }

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
    exports.Set("enumDependentServices", Function::New(env, enumDependentServices));
    exports.Set("getServiceConfiguration", Function::New(env, getServiceConfiguration));
    exports.Set("getServiceTriggers", Function::New(env, getServiceTriggers));

    return exports;
}

// Export
NODE_API_MODULE(Winservices, Init)
