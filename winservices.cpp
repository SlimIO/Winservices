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
 * Function to Convert GUID to std::string
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
 * Translate the int32_t desiredState to a DWORD valid ENUMERATION
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
 * Asynchronous Worker to Enumerate Windows Services
 * 
 * @header: windows.h
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/ns-winsvc-_enum_service_status_processa
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/ns-winsvc-_service_status_process
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
        DWORD errorCode = GetLastError();
        stringstream error;
        error << e.what() << " - code (" << errorCode << ") - " << getLastErrorMessage();
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
    DWORD serviceState = SERVICE_STATE_ALL;

    // Check if there is less than two argument, if then throw a JavaScript exception
    if (info.Length() < 2) {
        Error::New(env, "Wrong number of argument provided!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // The first argument (desiredState) should be typeof Napi::Number
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
    int32_t desiredState = info[0].As<Number>().Int32Value();
    serviceState = getServiceState(desiredState);
    Function cb = info[1].As<Function>();
    (new EnumServicesWorker(cb, serviceState))->Queue();

    return env.Undefined();
}

/*
 * Asynchronous Worker to retrieve the configuration of a given Service (name)
 * 
 * @header: windows.h
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/services/querying-a-service-s-configuration
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/ns-winsvc-_query_service_configa
 */
class ConfigServiceWorker : public AsyncWorker {
    public:
        ConfigServiceWorker(Function& callback, string serviceName) : AsyncWorker(callback), serviceName(serviceName) {}
        ~ConfigServiceWorker() {}

    private:
        string serviceName;
        LPQUERY_SERVICE_CONFIG config;

    // This code will be executed on the worker thread
    void Execute() {
        SCManager manager;
        BOOL success;

        // Open Manager!
        success = manager.Open(SC_MANAGER_ENUMERATE_SERVICE);
        if (!success) {
            return SetError("Open SCManager failed");
        }

        // Open Service!
        success = manager.DeclareService(serviceName, SERVICE_QUERY_CONFIG);
        if (!success) {
            return SetError("Failed to Open service!");
        }

        // Retrieve Service Config!
        ServiceConfigResponse serviceConfig = manager.ServiceConfig();
        if (!serviceConfig.status) {
            return SetError("Failed to Query Service Config!");
        }
        config = serviceConfig.lpsc;

        manager.Close();
    }

    void OnError(const Error& e) {
        DWORD errorCode = GetLastError();
        stringstream error;
        error << e.what() << " - code (" << errorCode << ") - " << getLastErrorMessage();
        Error::New(Env(), error.str()).ThrowAsJavaScriptException();
    }

    void OnOK() {
        HandleScope scope(Env());
        Object ret = Object::New(Env());

        // Setup properties
        ret.Set("type", config->dwServiceType);
        ret.Set("startType", config->dwStartType);
        ret.Set("errorControl", config->dwErrorControl);
        ret.Set("binaryPath", config->lpBinaryPathName);
        ret.Set("account", config->lpServiceStartName);

        if (config->lpLoadOrderGroup != NULL && lstrcmp(config->lpLoadOrderGroup, TEXT("")) != 0) {
            ret.Set("loadOrderGroup", config->lpLoadOrderGroup);
        }
        if (config->dwTagId != 0) {
            ret.Set("tagId", config->dwTagId);
        }
        if (config->lpDependencies != NULL && lstrcmp(config->lpDependencies, TEXT("")) != 0) {
            ret.Set("dependencies", config->lpDependencies);
        }

        Callback().Call({Env().Null(), ret});
    }

};

/*
 * Retrieve Service configuration
 */
Value getServiceConfiguration(const CallbackInfo& info) {
    Env env = info.Env();

    // Check if there is less than two argument, if then throw a JavaScript exception
    if (info.Length() < 2) {
        Error::New(env, "Wrong number of argument provided!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // The first argument (serviceName) should be typeof Napi::String
    if (!info[0].IsString()) {
        Error::New(env, "argument serviceName should be typeof string!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // callback should be a Napi::Function
    if (!info[1].IsFunction()) {
        Error::New(env, "Argument callback should be a Function!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Retrieve service Name and execute worker!
    string serviceName = info[0].As<String>().Utf8Value();
    Function cb = info[1].As<Function>();
    (new ConfigServiceWorker(cb, serviceName))->Queue();

    return env.Undefined();
}

/*
 * Asynchronous Worker to retrieve all triggers of a given Service (name).
 * 
 * @header: windows.h
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/taskschd/c-c-code-example-retrieving-trigger-strings
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/ns-winsvc-_service_trigger_info
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/ns-winsvc-_service_trigger_specific_data_item
 */
class ServiceTriggersWorker : public AsyncWorker {
    public:
        ServiceTriggersWorker(Function& callback, string serviceName) : AsyncWorker(callback), serviceName(serviceName) {}
        ~ServiceTriggersWorker() {}

    private:
        string serviceName;
        SERVICE_TRIGGER_INFO *triggers = {0};

    // This code will be executed on the worker thread
    void Execute() {
        SCManager manager;
        BOOL success;

        // Instanciate Variables
        void* buf = NULL;
        DWORD dwBytesNeeded, cbBufSize;

        // Open Manager!
        success = manager.Open(SC_MANAGER_ENUMERATE_SERVICE);
        if (!success) {
            return SetError("Open SCManager failed");
        }

        // Open Service!
        success = manager.DeclareService(serviceName, SERVICE_QUERY_CONFIG);
        if (!success) {
            return SetError("Failed to Open service!");
        }

        // Query for TRIGGER INFO
        success = QueryServiceConfig2A(manager.service, SERVICE_CONFIG_TRIGGER_INFO, NULL, 0, &dwBytesNeeded);
        if (!success) {
            if(ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
                manager.Close();
                return SetError("QueryServiceConfig2 (1) failed");
            }
            cbBufSize = dwBytesNeeded;
            free(buf);
            buf = malloc(cbBufSize);
        }

        if (!QueryServiceConfig2(manager.service, SERVICE_CONFIG_TRIGGER_INFO, (LPBYTE) buf, cbBufSize, &dwBytesNeeded)) {
            manager.Close();
            return SetError("QueryServiceConfig2 (2) failed");
        }

        triggers = (SERVICE_TRIGGER_INFO*) buf;
        manager.Close();
    }

    void OnError(const Error& e) {
        DWORD errorCode = GetLastError();
        stringstream error;
        error << e.what() << " - code (" << errorCode << ") - " << getLastErrorMessage();
        Error::New(Env(), error.str()).ThrowAsJavaScriptException();
    }

    void OnOK() {
        HandleScope scope(Env());

        DWORD triggersLen = triggers->cTriggers;
        Array ret = Array::New(Env(), triggersLen);
        PSERVICE_TRIGGER currTrigger = triggers->pTriggers;

        for (unsigned i = 0; i < triggersLen; ++i, currTrigger++) {
            Object trigger = Object::New(Env());
            Array dataItems = Array::New(Env());

            trigger.Set("type", currTrigger->dwTriggerType);
            trigger.Set("action", currTrigger->dwAction);
            trigger.Set("dataItems", dataItems);

            string guid = guidToString(*currTrigger->pTriggerSubtype);
            trigger.Set("guid", guid.c_str());

            ret[i] = trigger;
            for (DWORD j = 0; j < currTrigger->cDataItems; j++) {
                SERVICE_TRIGGER_SPECIFIC_DATA_ITEM pServiceTrigger = currTrigger->pDataItems[j];
                Object specificDataItems = Object::New(Env());
                dataItems[j] = specificDataItems;

                specificDataItems.Set("dataType", pServiceTrigger.dwDataType);
                if (pServiceTrigger.dwDataType == SERVICE_TRIGGER_DATA_TYPE_BINARY) {
                    specificDataItems.Set("data", byteSeqToString(pServiceTrigger.pData, pServiceTrigger.cbData));
                }
                else if(pServiceTrigger.dwDataType == SERVICE_TRIGGER_DATA_TYPE_STRING) {
                    specificDataItems.Set("data", byteSeqToString(pServiceTrigger.pData, pServiceTrigger.cbData));
                }
            }
        }

        Callback().Call({Env().Null(), ret});
    }

};

/*
 * Retrieve Service triggers (interface binding).
 */
Value getServiceTriggers(const CallbackInfo& info) {
    Env env = info.Env();

    // Check if there is less than two argument, if then throw a JavaScript exception
    if (info.Length() < 2) {
        Error::New(env, "Wrong number of argument provided!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // The first argument (serviceName) should be typeof Napi::String
    if (!info[0].IsString()) {
        Error::New(env, "argument serviceName should be typeof string!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // callback should be a Napi::Function
    if (!info[1].IsFunction()) {
        Error::New(env, "Argument callback should be a Function!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Retrieve service Name and execute worker
    string serviceName = info[0].As<String>().Utf8Value();
    Function cb = info[1].As<Function>();
    (new ServiceTriggersWorker(cb, serviceName))->Queue();

    return env.Undefined();
}


/*
 * Asynchronous Worker to enumerate depending service of a given service!
 * 
 * @header: windows.h
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/nf-winsvc-enumdependentservicesa
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/Services/stopping-a-service
 */
class DependentServiceWorker : public AsyncWorker {
    public:
        DependentServiceWorker(Function& callback, string serviceName, DWORD serviceState) :
        AsyncWorker(callback), serviceName(serviceName), serviceState(serviceState) {}
        ~DependentServiceWorker() {}

    private:
        string serviceName;
        LPENUM_SERVICE_STATUSA lpDependencies = NULL;
        DWORD serviceState;
        DWORD nbReturned;

    // This code will be executed on the worker thread
    void Execute() {
        SCManager manager;
        BOOL success;
        DWORD dwBytesNeeded;

        // Open Manager!
        success = manager.Open(SC_MANAGER_ENUMERATE_SERVICE);
        if (!success) {
            return SetError("Failed to Open SCManager");
        }

        // Open Service!
        success = manager.DeclareService(serviceName, SERVICE_ENUMERATE_DEPENDENTS);
        if (!success) {
            return SetError("Failed to Open service");
        }

        success = EnumDependentServicesA(manager.service, serviceState, lpDependencies, 0, &dwBytesNeeded, &nbReturned);
        if (!success) {
            DWORD errorCode = GetLastError();
            if(ERROR_MORE_DATA != errorCode) {
                manager.Close();
                return SetError("EnumDependentServicesA (1) failed");
            }

            lpDependencies = (LPENUM_SERVICE_STATUSA) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwBytesNeeded);
            if (!lpDependencies) {
                manager.Close();
                return SetError("ERROR while re-allocating memory");
            }
        }

        if (!EnumDependentServicesA(manager.service, serviceState, lpDependencies, dwBytesNeeded, &dwBytesNeeded, &nbReturned)) {
            manager.Close();
            return SetError("EnumDependentServicesA (2) failed");
        }

        manager.Close();
    }

    void OnError(const Error& e) {
        DWORD errorCode = GetLastError();
        stringstream error;
        error << e.what() << " for service " << serviceName << " - code (" << errorCode << ") - " << getLastErrorMessage();

        Callback().Call({String::New(Env(), error.str()), Env().Null()});
    }

    void OnOK() {
        HandleScope scope(Env());
        Object ret = Object::New(Env());

        ENUM_SERVICE_STATUSA service;
        for (DWORD i = 0; i < nbReturned; ++i) {
            SecureZeroMemory(&service, sizeof(service));
            service = *(lpDependencies + i);
            Object JSObject = Object::New(Env());

            JSObject.Set("name", service.lpServiceName);
            JSObject.Set("displayName", service.lpDisplayName);
            Object statusProcess = Object::New(Env());
            statusProcess.Set("currentState", service.ServiceStatus.dwCurrentState);
            statusProcess.Set("checkpoint", service.ServiceStatus.dwCheckPoint);
            statusProcess.Set("controlsAccepted", service.ServiceStatus.dwControlsAccepted);
            statusProcess.Set("serviceSpecificExitCode", service.ServiceStatus.dwServiceSpecificExitCode);
            statusProcess.Set("serviceType", service.ServiceStatus.dwServiceType);
            statusProcess.Set("waitHint", service.ServiceStatus.dwWaitHint);
            statusProcess.Set("win32ExitCode", service.ServiceStatus.dwWin32ExitCode);
            
            JSObject.Set("process", statusProcess);
            ret.Set(service.lpServiceName, JSObject);
        }

        Callback().Call({Env().Null(), ret});
    }

};

/*
 * Enumerate dependent Services!
 * 
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/Services/stopping-a-service
 */
Value enumDependentServices(const CallbackInfo& info) {
    Env env = info.Env();
    DWORD serviceState = SERVICE_STATE_ALL;

    // Check if there is less than two argument, if then throw a JavaScript exception
    if (info.Length() < 3) {
        Error::New(env, "Wrong number of argument provided!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // The first argument (serviceName) should be typeof Napi::String
    if (!info[0].IsString()) {
        Error::New(env, "argument serviceName should be typeof string!").ThrowAsJavaScriptException();
        return env.Null();
    }

        // The first argument (desiredState) should be typeof Napi::Number
    if (!info[1].IsNumber()) {
        Error::New(env, "Argument desiredState should be typeof number!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // callback should be a Napi::Function
    if (!info[2].IsFunction()) {
        Error::New(env, "Argument callback should be a Function!").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Retrieve service Name
    string serviceName = info[0].As<String>().Utf8Value();
    int32_t desiredState = info[1].As<Number>().Int32Value();
    serviceState = getServiceState(desiredState);
    Function cb = info[2].As<Function>();
    (new DependentServiceWorker(cb, serviceName, serviceState))->Queue();

    return env.Undefined();
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
