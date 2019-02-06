# Winservices
[![Maintenance](https://img.shields.io/badge/Maintained%3F-yes-green.svg)](https://github.com/SlimIO/Winservices/commit-activity)
[![GitHub license](https://img.shields.io/github/license/Naereen/StrapDown.js.svg)](https://github.com/SlimIO/Nixmem/blob/master/LICENSE)
![V1.2](https://img.shields.io/badge/version-1.2.0-blue.svg)
![N-API](https://img.shields.io/badge/N--API-experimental-orange.svg)
[![Build Status](https://travis-ci.com/SlimIO/Winservices.svg?branch=master)](https://travis-ci.com/SlimIO/Winservices)

SlimIO Windows Services is a NodeJS Binding which expose low-level Microsoft APIs on Services.

This binding expose the following methods/struct:

- [ENUM_SERVICE_STATUS_PROCESS](https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/ns-winsvc-_enum_service_status_processa)
- [SERVICE_CONFIG](https://docs.microsoft.com/en-us/windows/desktop/services/querying-a-service-s-configuration)
- [SERVICE_TRIGGER_INFO](https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/ns-winsvc-_service_trigger_info)
- [EnumDependentServices](https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/nf-winsvc-enumdependentservicesa)

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

## How to build the project

Before building the project, be sure to get the following npm package installed:

- Install (or upgrade to) NodeJS v10+ and npm v6+
- [Windows build tools](https://www.npmjs.com/package/windows-build-tools)

Then, just run normal npm install command:

```bash
$ npm install
```

## Available commands

All projects commands are described here:

| command | description |
| --- | --- |
| npm run prebuild | Generate addon prebuild |
| npm run doc | Generate JSDoc .HTML documentation (in the /docs root directory) |
| npm run coverage | Generate coverage of tests |
| npm run report | Generate .HTML report of tests coverage |

> the report command have to be triggered after the coverage command.
