# Winservices

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

> WIP

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
