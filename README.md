# Winservices
![V1.2](https://img.shields.io/badge/version-1.2.0-blue.svg)
![N-API](https://img.shields.io/badge/N--API-v3-green.svg)
[![Maintenance](https://img.shields.io/badge/Maintained%3F-yes-green.svg)](https://github.com/SlimIO/Winservices/commit-activity)
[![GitHub license](https://img.shields.io/github/license/Naereen/StrapDown.js.svg)](https://github.com/SlimIO/Nixmem/blob/master/LICENSE)
![2DEP](https://img.shields.io/badge/Dependencies-2-yellow.svg)
[![Known Vulnerabilities](https://snyk.io/test/github/SlimIO/Winservices/badge.svg?targetFile=package.json)](https://snyk.io/test/github/SlimIO/Winservices?targetFile=package.json)
[![Build Status](https://travis-ci.com/SlimIO/Winservices.svg?branch=master)](https://travis-ci.com/SlimIO/Winservices) [![Greenkeeper badge](https://badges.greenkeeper.io/SlimIO/Winservices.svg)](https://greenkeeper.io/)

SlimIO Windows Services is a NodeJS Binding which expose low-level Microsoft APIs on Services.

This binding expose the following methods/struct:

- [ENUM_SERVICE_STATUS_PROCESS](https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/ns-winsvc-_enum_service_status_processa)
- [SERVICE_CONFIG](https://docs.microsoft.com/en-us/windows/desktop/services/querying-a-service-s-configuration)
- [SERVICE_TRIGGER_INFO](https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/ns-winsvc-_service_trigger_info)
- [EnumDependentServices](https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/nf-winsvc-enumdependentservicesa)

## Requirements
- Node.js v10 or higher 

## Getting Started

This package is available in the Node Package Repository and can be easily installed with [npm](https://docs.npmjs.com/getting-started/what-is-npm) or [yarn](https://yarnpkg.com).

```bash
$ npm i @slimio/winservices
# or
$ yarn add @slimio/winservices
```

## Usage example

Get all active windows services and retrieve advanced informations for each of them in series.

```js
const {
    enumServicesStatus,
    getServiceConfiguration,
    constants: { States }
} = require("@slimio/winservices");

async function main() {
    const activeServices = await enumServicesStatus(States.Active);

    for (const service of activeServices) {
        console.log(`service name: ${service.name}`);
        const serviceConfig = await getServiceConfiguration(service.name);
        console.log(JSON.stringify(serviceConfig, null, 4));
        console.log("------------------------------\n");
    }
}
main().catch(console.error);
```

## API

### enumServicesStatus(desiredState: number): Promise< Service[] >

Enumerate Windows Services by the desirate state (Default equal to `State.All`). State can be retrieved with the constants **State**.

```ts
export interface ServiceStates {
    Active: 0,
    Inactive: 1,
    All: 2
}
```

The returned value is a Promise that contain an Array of Service.

```ts
export interface Service {
    name: string,
    displayName: string;
    process: {
        id?: number;
        name?: string;
        currentState: number;
        serviceType: number;
        checkPoint?: number;
        controlsAccepted: number;
        serviceFlags?: number;
        serviceSpecificExitCode: number;
        waitHint: number;
        win32ExitCode: number;
    };
}
```

### enumDependentServices(serviceName: string, desiredState?: number): Promise< DependentServices >
Enumerate dependent Windows Services of a given Service name. The returned value is a Promise of Object DependentServices.

Default value for desiredState is `State.All`.

```ts
export interface DependentServices {
    [serviceName: string]: Service;
}
```

> **Warning**: Each Service are a reducted version of the TypeScript interface `Service` (optionals are not in the payload).

### getServiceConfiguration(serviceName: string): Promise< ServiceInformation >
Get a given Windows Service configuration. The returned value is a Promise of Object ServiceInformation.

```ts
export interface ServiceInformation {
    type: string;
    startType: string;
    errorControl: string;
    binaryPath: string;
    account: string;
    loadOrderGroup?: string;
    tagId?: number;
    dependencies?: string;
    description?: string;
}
```

### getServiceTriggers(serviceName: string): Promise< ServiceTrigger[] >
Get all Service triggers for a given Service name. The returned value is a Promise that contain an Array of ServiceTrigger.

```ts
export interface ServiceTrigger {
    type: number;
    action: number;
    guid: string;
    dataItems: ServiceTriggerSpecificDataItem[]
}

export interface ServiceTriggerSpecificDataItem {
    dataType: number;
    data?: string;
}
```

## Contribution Guidelines
To contribute to the project, please read the [code of conduct](https://github.com/SlimIO/Governance/blob/master/COC_POLICY.md) and the guide for [N-API compilation](https://github.com/SlimIO/Governance/blob/master/docs/native_addons.md).

## License
MIT
