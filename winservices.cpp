#include <windows.h>
#include <tlhelp32.h>
#include "SCManager.h"
#include "napi.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <string>

const char FAILED_MANAGER[] = "Failed to open SCManager handle";
const char FAILED_SERVICE[] = "Failed to open Service handle";

typedef std::unordered_map<DWORD, std::string> PROCESSESMAP;

/*
 * Function to Convert GUID to std::string
 */
std::string guidToString(GUID guid) {
	char guid_cstr[39];
	snprintf(guid_cstr, sizeof(guid_cstr),
	         "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
	         guid.Data1, guid.Data2, guid.Data3,
	         guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
	         guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

	return std::string(guid_cstr);
}

/*
 * Retrieve last message error with code of GetLastError()
 */
std::string getLastErrorMessage() {
    char err[256];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), err, 255, NULL);
    return std::string(err);
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
        std::wstring wSzExeFile((wchar_t*) pe32.szExeFile);
        procMap->insert(std::make_pair(pe32.th32ProcessID, std::string(wSzExeFile.begin(), wSzExeFile.end())));
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return true;
}

/*
 * Translate the int32_t desiredState to a valid DWORD ENUMERATION
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
std::string byteSeqToString(const unsigned char bytes[], size_t n) {
    std::ostringstream stm;
    stm << std::hex << std::uppercase;

    for(size_t i = 0; i < n; ++i) {
        stm << std::setw(2) << std::setfill('0') << unsigned(bytes[i]);
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
class EnumServicesWorker : public Napi::AsyncWorker {
    public:
        EnumServicesWorker(Napi::Function& callback, DWORD serviceState, std::string host) : AsyncWorker(callback), serviceState(serviceState) {}
        ~EnumServicesWorker() {}

    private:
        std::string host;
        DWORD serviceState;
        ENUM_SERVICE_STATUS_PROCESS *lpServiceStatus;
        PROCESSESMAP *processesMap;
        DWORD servicesNum;

    // This code will be executed on the worker thread
    void Execute() {
        SCManager manager;
        BOOL success;

        processesMap = new PROCESSESMAP;
        success = getProcessNameAndId(processesMap);
        if (!success) {
            return SetError("Failed to retrieve process names and ids");
        }

        success = manager.Open(SC_MANAGER_ENUMERATE_SERVICE, host);
        if (!success) {
            return SetError(FAILED_MANAGER);
        }

        EnumServicesResponse enumRet = manager.EnumServicesStatus(serviceState, &servicesNum);
        if (!enumRet.status) {
            std::stringstream error;
            error << "Failed to enumerate Windows Services by state (" << serviceState << ")";
            return SetError(error.str());
        }

        lpServiceStatus = enumRet.lpBytes;
        manager.Close();
    }

    void OnError(const Napi::Error& e) {
        DWORD errorCode = GetLastError();
        std::stringstream error;
        error << e.what();
        if (errorCode != 0) {
            error << " - code (" << errorCode << ") - " << getLastErrorMessage();
        }

        Callback().Call({Napi::String::New(Env(), error.str()), Env().Null()});
    }

    void OnOK() {
        Napi::HandleScope scope(Env());
        Napi::Array ret = Napi::Array::New(Env());
        unsigned arrIndex = 0;

        ENUM_SERVICE_STATUS_PROCESS service;
        DWORD processId;
        for (DWORD i = 0; i<servicesNum; ++i) {
            SecureZeroMemory(&service, sizeof(service));
            SecureZeroMemory(&processId, sizeof(processId));
            service = *lpServiceStatus;
            processId = service.ServiceStatusProcess.dwProcessId;

            Napi::Object JSObject = Napi::Object::New(Env());
            Napi::Object statusProcess = Napi::Object::New(Env());

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
 * Asynchronous Worker to retrieve the configuration of a given Service (name)
 *
 * @header: windows.h
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/services/querying-a-service-s-configuration
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/ns-winsvc-_query_service_configa
 */
class ConfigServiceWorker : public Napi::AsyncWorker {
    public:
        ConfigServiceWorker(Napi::Function& callback, std::string serviceName) : AsyncWorker(callback), serviceName(serviceName) {}
        ~ConfigServiceWorker() {}

    private:
        std::string serviceName;
        LPQUERY_SERVICE_CONFIG config;
        LPSERVICE_DESCRIPTION description;

    // This code will be executed on the worker thread
    void Execute() {
        SCManager manager;
        BOOL success;
        DWORD dwBytesNeeded;

        // Open Manager!
        success = manager.Open(SC_MANAGER_ENUMERATE_SERVICE, "");
        if (!success) {
            return SetError(FAILED_MANAGER);
        }

        // Open Service!
        success = manager.DeclareService(serviceName, SERVICE_QUERY_CONFIG);
        if (!success) {
            return SetError(FAILED_SERVICE);
        }

        // Retrieve Service Config!
        ServiceConfigResponse serviceConfig = manager.ServiceConfig();
        if (!serviceConfig.status) {
            return SetError("Failed to Query Service Config!");
        }
        config = serviceConfig.lpsc;

        if(!QueryServiceConfig2(manager.service, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &dwBytesNeeded)) {
            if(ERROR_INSUFFICIENT_BUFFER != GetLastError()){
                manager.Close();
                return SetError("QueryServiceConfig2 failed (1)");
            }
            description = (LPSERVICE_DESCRIPTION) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwBytesNeeded);
        }

        if (!QueryServiceConfig2(manager.service, SERVICE_CONFIG_DESCRIPTION, (LPBYTE) description, dwBytesNeeded, &dwBytesNeeded))  {
            manager.Close();
            return SetError("QueryServiceConfig2 failed (2)");
        }

        manager.Close();
    }

    void OnError(const Napi::Error& e) {
        DWORD errorCode = GetLastError();
        std::stringstream error;
        error << e.what() << " for " << serviceName;
        if (errorCode != 0) {
            error << " - code (" << errorCode << ") - " << getLastErrorMessage();
        }

        Callback().Call({Napi::String::New(Env(), error.str()), Env().Null()});
    }

    void OnOK() {
        Napi::HandleScope scope(Env());
        Napi::Object ret = Napi::Object::New(Env());

        // Setup properties
        switch (config->dwServiceType) {
            case SERVICE_FILE_SYSTEM_DRIVER:
                ret.Set("type", "FILE_SYSTEM_DRIVER");
                break;
            case SERVICE_KERNEL_DRIVER:
                ret.Set("type", "KERNEL_DRIVER");
                break;
            case SERVICE_WIN32_OWN_PROCESS:
                ret.Set("type", "WIN32_OWN_PROCESS");
                break;
            case SERVICE_WIN32_SHARE_PROCESS:
                ret.Set("type", "WIN32_SHARE_PROCESS");
                break;
            case SERVICE_INTERACTIVE_PROCESS:
                ret.Set("type", "INTERACTIVE_PROCESS");
                break;
            default:
                ret.Set("type", "UNKNOWN");
                break;
        }

        switch (config->dwStartType) {
            case SERVICE_AUTO_START:
                ret.Set("startType", "AUTO_START");
                break;
            case SERVICE_BOOT_START:
                ret.Set("startType", "BOOT_START");
                break;
            case SERVICE_DEMAND_START:
                ret.Set("startType", "DEMAND_START");
                break;
            case SERVICE_DISABLED:
                ret.Set("startType", "DISABLED");
                break;
            case SERVICE_SYSTEM_START:
                ret.Set("startType", "SYSTEM_START");
                break;
        }

        switch (config->dwErrorControl) {
            case SERVICE_ERROR_CRITICAL:
                ret.Set("errorControl", "CRITICAL");
                break;
            case SERVICE_ERROR_IGNORE:
                ret.Set("errorControl", "IGNORE");
                break;
            case SERVICE_ERROR_NORMAL:
                ret.Set("errorControl", "NORMAL");
                break;
            case SERVICE_ERROR_SEVERE:
                ret.Set("errorControl", "SEVERE");
                break;
        }
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
        if (description->lpDescription != NULL) {
            ret.Set("description", description->lpDescription);
        }

        Callback().Call({Env().Null(), ret});
    }

};

/*
 * Asynchronous Worker to retrieve all triggers of a given Service (name).
 *
 * @header: windows.h
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/taskschd/c-c-code-example-retrieving-trigger-strings
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/ns-winsvc-_service_trigger_info
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/ns-winsvc-_service_trigger_specific_data_item
 */
class ServiceTriggersWorker : public Napi::AsyncWorker {
    public:
        ServiceTriggersWorker(Napi::Function& callback, std::string serviceName) : AsyncWorker(callback), serviceName(serviceName) {}
        ~ServiceTriggersWorker() {}

    private:
        std::string serviceName;
        SERVICE_TRIGGER_INFO *triggers = {0};

    // This code will be executed on the worker thread
    void Execute() {
        SCManager manager;
        BOOL success;

        // Instanciate Variables
        void* buf = NULL;
        DWORD dwBytesNeeded, cbBufSize;

        success = manager.Open(SC_MANAGER_ENUMERATE_SERVICE, "");
        if (!success) {
            return SetError(FAILED_MANAGER);
        }

        success = manager.DeclareService(serviceName, SERVICE_QUERY_CONFIG);
        if (!success) {
            return SetError(FAILED_SERVICE);
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

    void OnError(const Napi::Error& e) {
        DWORD errorCode = GetLastError();
        std::stringstream error;
        error << e.what() << " for " << serviceName;
        if (errorCode != 0) {
            error << " - code (" << errorCode << ") - " << getLastErrorMessage();
        }

        Callback().Call({Napi::String::New(Env(), error.str()), Env().Null()});
    }

    void OnOK() {
        Napi::HandleScope scope(Env());

        DWORD triggersLen = triggers->cTriggers;
        Napi::Array ret = Napi::Array::New(Env(), triggersLen);
        PSERVICE_TRIGGER currTrigger = triggers->pTriggers;

        for (unsigned i = 0; i < triggersLen; ++i, currTrigger++) {
            Napi::Object trigger = Napi::Object::New(Env());
            Napi::Array dataItems = Napi::Array::New(Env());

            trigger.Set("type", currTrigger->dwTriggerType);
            trigger.Set("action", currTrigger->dwAction);
            trigger.Set("dataItems", dataItems);

            std::string guid = guidToString(*currTrigger->pTriggerSubtype);
            trigger.Set("guid", guid.c_str());

            ret[i] = trigger;
            for (DWORD j = 0; j < currTrigger->cDataItems; j++) {
                SERVICE_TRIGGER_SPECIFIC_DATA_ITEM pServiceTrigger = currTrigger->pDataItems[j];
                Napi::Object specificDataItems = Napi::Object::New(Env());
                dataItems[j] = specificDataItems;

                specificDataItems.Set("dataType", pServiceTrigger.dwDataType);
                if (pServiceTrigger.dwDataType == SERVICE_TRIGGER_DATA_TYPE_BINARY) {
                    specificDataItems.Set("data", byteSeqToString(pServiceTrigger.pData, pServiceTrigger.cbData));
                }
                else if(pServiceTrigger.dwDataType == SERVICE_TRIGGER_DATA_TYPE_STRING) {
                    std::stringstream data;
                    for(size_t i = 0; i < pServiceTrigger.cbData; ++i) {
                        data << (char*) pServiceTrigger.pData;
                        pServiceTrigger.pData++;
                    }
                    specificDataItems.Set("data", data.str());
                }
            }
        }

        Callback().Call({Env().Null(), ret});
    }

};


/*
 * Asynchronous Worker to enumerate depending service of a given service!
 *
 * @header: windows.h
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/nf-winsvc-enumdependentservicesa
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/Services/stopping-a-service
 */
class DependentServiceWorker : public Napi::AsyncWorker {
    public:
        DependentServiceWorker(Napi::Function& callback, std::string serviceName, DWORD serviceState) :
        AsyncWorker(callback), serviceName(serviceName), serviceState(serviceState) {}
        ~DependentServiceWorker() {}

    private:
        std::string serviceName;
        LPENUM_SERVICE_STATUSA lpDependencies = NULL;
        DWORD serviceState;
        DWORD nbReturned;

    // This code will be executed on the worker thread
    void Execute() {
        SCManager manager;
        BOOL success;
        DWORD dwBytesNeeded;

        // Open Manager!
        success = manager.Open(SC_MANAGER_ENUMERATE_SERVICE, "");
        if (!success) {
            return SetError(FAILED_MANAGER);
        }

        // Open Service!
        success = manager.DeclareService(serviceName, SERVICE_ENUMERATE_DEPENDENTS);
        if (!success) {
            return SetError(FAILED_SERVICE);
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

    void OnError(const Napi::Error& e) {
        DWORD errorCode = GetLastError();
        std::stringstream error;
        error << e.what() << " for " << serviceName;
        if (errorCode != 0) {
            error << " - code (" << errorCode << ") - " << getLastErrorMessage();
        }

        Callback().Call({Napi::String::New(Env(), error.str()), Env().Null()});
    }

    void OnOK() {
        Napi::HandleScope scope(Env());
        Napi::Object ret = Napi::Object::New(Env());

        ENUM_SERVICE_STATUSA service;
        for (DWORD i = 0; i < nbReturned; ++i) {
            SecureZeroMemory(&service, sizeof(service));
            service = *(lpDependencies + i);

            Napi::Object JSObject = Napi::Object::New(Env());
            Napi::Object statusProcess = Napi::Object::New(Env());

            JSObject.Set("name", service.lpServiceName);
            JSObject.Set("displayName", service.lpDisplayName);

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
 * Enumerate Windows Services (binding interface).
 */
Napi::Value enumServicesStatus(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    int32_t desiredState;
    DWORD serviceState = SERVICE_STATE_ALL;
    Napi::Function cb;

    if (info.Length() < 2) {
        Napi::Error::New(env, "Wrong number of argument provided!").ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[0].IsNumber()) {
        Napi::Error::New(env, "Argument desiredState should be typeof number!").ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[1].IsObject()) {
        Napi::Error::New(env, "Argument options should be instanceof Object!").ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[2].IsFunction()) {
        Napi::Error::New(env, "Argument callback should be a Function!").ThrowAsJavaScriptException();
        return env.Null();
    }

    desiredState = info[0].As<Napi::Number>().Int32Value();
    Napi::Object options = info[1].As<Napi::Object>();
    cb = info[2].As<Napi::Function>();

    std::string host = options.Get("host").As<Napi::String>().Utf8Value();
    std::cout << "hostname: " << host << "\n";

    serviceState = getServiceState(desiredState);
    (new EnumServicesWorker(cb, serviceState, host))->Queue();

    return env.Undefined();
}

/*
 * Retrieve a given Service configuration (binding interface)
 */
Napi::Value getServiceConfiguration(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    std::string serviceName;
    Napi::Function cb;

    if (info.Length() < 2) {
        Napi::Error::New(env, "Wrong number of argument provided!").ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[0].IsString()) {
        Napi::Error::New(env, "argument serviceName should be typeof string!").ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[1].IsFunction()) {
        Napi::Error::New(env, "Argument callback should be a Function!").ThrowAsJavaScriptException();
        return env.Null();
    }

    serviceName = info[0].As<Napi::String>().Utf8Value();
    cb = info[1].As<Napi::Function>();

    (new ConfigServiceWorker(cb, serviceName))->Queue();

    return env.Undefined();
}

/*
 * Retrieve a given service triggers (interface binding).
 */
Napi::Value getServiceTriggers(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    std::string serviceName;
    Napi::Function cb;

    if (info.Length() < 2) {
        Napi::Error::New(env, "Wrong number of argument provided!").ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[0].IsString()) {
        Napi::Error::New(env, "argument serviceName should be typeof string!").ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[1].IsFunction()) {
        Napi::Error::New(env, "Argument callback should be a Function!").ThrowAsJavaScriptException();
        return env.Null();
    }

    serviceName = info[0].As<Napi::String>().Utf8Value();
    cb = info[1].As<Napi::Function>();

    (new ServiceTriggersWorker(cb, serviceName))->Queue();

    return env.Undefined();
}

/*
 * Enumerate dependent Services!
 *
 * @doc: https://docs.microsoft.com/en-us/windows/desktop/Services/stopping-a-service
 */
Napi::Value enumDependentServices(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    DWORD serviceState = SERVICE_STATE_ALL;
    int32_t desiredState;
    std::string serviceName;
    Napi::Function cb;

    if (info.Length() < 3) {
        Napi::Error::New(env, "Wrong number of argument provided!").ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[0].IsString()) {
        Napi::Error::New(env, "argument serviceName should be typeof string!").ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[1].IsNumber()) {
        Napi::Error::New(env, "Argument desiredState should be typeof number!").ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[2].IsFunction()) {
        Napi::Error::New(env, "Argument callback should be a Function!").ThrowAsJavaScriptException();
        return env.Null();
    }

    serviceName = info[0].As<Napi::String>().Utf8Value();
    desiredState = info[1].As<Napi::Number>().Int32Value();
    cb = info[2].As<Napi::Function>();

    serviceState = getServiceState(desiredState);
    (new DependentServiceWorker(cb, serviceName, serviceState))->Queue();

    return env.Undefined();
}

/*
 * Initialize Node.js N-API binding exports
 *
 * @header: napi.h
 */
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("enumServicesStatus", Napi::Function::New(env, enumServicesStatus));
    exports.Set("enumDependentServices", Napi::Function::New(env, enumDependentServices));
    exports.Set("getServiceConfiguration", Napi::Function::New(env, getServiceConfiguration));
    exports.Set("getServiceTriggers", Napi::Function::New(env, getServiceTriggers));

    return exports;
}

// Export
NODE_API_MODULE(Winservices, Init)
