"use strict";

/**
 * @namespace winservices
 * @description Windows Services - Node.JS low level binding
 */
const winservices = require("node-gyp-build")(__dirname);

// CONSTANTS
const kServiceStates = Object.freeze({ Active: 0, Inactive: 1, All: 2 });
const kTriggerActions = Object.freeze({ start: 1, stop: 2 });
const kTriggerTypes = Object.freeze({
    custom: 20,
    deviceInterfaceArrival: 1,
    addressAvailability: 2,
    domainJoin: 3,
    fireWallPortEvent: 4,
    groupPolicy: 5,
    networkEndpoint: 6
});

/**
 * @async
 * @function enumServicesStatus
 * @memberof winservices#
 * @description Enumerate Windows Service by a given state
 * @param {!number} [desiredState=2] desired service State (Active, Inactive, All).
 * @param {any} [options]
 * @returns {Promise<Winservices.Service[]>}
 *
 * @version 1.0.0
 * @example
 * const { enumServicesStatus, constants: { States } } = require("@slimio/winservices");
 *
 * async function main() {
 *     const services = enumServicesStatus(States.Active);
 *     for (const service of services) {
 *         console.log(`service name => ${service.name}`);
 *     }
 * }
 * main().catch(console.error)
 */
function enumServicesStatus(desiredState = kServiceStates.All, options = { host: "" }) {
    return new Promise((resolve, reject) => {
        if (!Reflect.has(options, "host")) {
            options.host = "";
        }
        winservices.enumServicesStatus(desiredState, options, (error, services) => {
            if (error) {
                return reject(error);
            }

            return resolve(services);
        });
    });
}

/**
 * @async
 * @function enumDependentServices
 * @memberof winservices#
 * @description Enumerate Dependent Service of a given Service
 * @param {!string} serviceName service name on which we have to seek for dependent services!
 * @param {!number} desiredState desired state of dependent services.
 * @returns {Promise<Winservices.DependentServices>}
 *
 * @version 1.0.0
 * @example
 * const {
 *     enumServicesStatus,
 *     enumDependentServices,
 *     constants: { States }
 * } = require("@slimio/winservices");
 *
 * async function main() {
 *     const services = enumServicesStatus(States.Active);
 *     for (const service of services) {
 *         console.log(`service name => ${service.name}`);
 *         const dependentServices = await enumDependentServices(service.name);
 *         console.log(`Number of dependent Services: ${Object.keys(dependentServices).length}`);
 *         console.log(JSON.stringify(dependentServices, null, 4));
 *     }
 * }
 * main().catch(console.error)
 */
function enumDependentServices(serviceName, desiredState = kServiceStates.All) {
    return new Promise((resolve, reject) => {
        winservices.enumDependentServices(serviceName, desiredState, (error, dependentServices) => {
            if (error) {
                return reject(error);
            }

            return resolve(dependentServices);
        });
    });
}

/**
 * @async
 * @function getServiceConfiguration
 * @memberof winservices#
 * @description Get a given service configuration
 * @param {!string} serviceName service name
 * @returns {Promise<Winservices.ServiceInformation>}
 *
 * @version 1.0.0
 * @example
 * const {
 *     enumServicesStatus,
 *     getServiceConfiguration,
 *     constants: { States }
 * } = require("@slimio/winservices");
 *
 * async function main() {
 *     const services = enumServicesStatus(States.Active);
 *     for (const service of services) {
 *         console.log(`\nservice name => ${service.name}`);
 *         const serviceConfig = await getServiceConfiguration(service.name);
 *         console.log("Service configuration: ");
 *         console.log(JSON.stringify(serviceConfig, null, 4));
 *         console.log("--------------------------------");
 *     }
 * }
 * main().catch(console.error)
 */
function getServiceConfiguration(serviceName) {
    return new Promise((resolve, reject) => {
        winservices.getServiceConfiguration(serviceName, (error, serviceConfig) => {
            if (error) {
                return reject(error);
            }

            return resolve(serviceConfig);
        });
    });
}

/**
 * @async
 * @function getServiceTriggers
 * @memberof winservices#
 * @description Retrieve all triggers of a given service!
 * @param {!string} serviceName service name
 * @returns {Promise<Winservices.ServiceTrigger[]>}
 *
 * @version 1.0.0
 * @example
 * const {
 *     enumServicesStatus,
 *     getServiceTriggers,
 *     constants: { States }
 * } = require("@slimio/winservices");
 *
 * async function main() {
 *     const services = enumServicesStatus(States.Active);
 *     for (const service of services) {
 *         console.log(`\nservice name => ${service.name}`);
 *         const serviceTriggers = await getServiceTriggers(service.name);
 *         if (serviceTriggers.length === 0) {
 *             continue;
 *         }
 *         console.log("Service triggers: ");
 *         console.log(JSON.stringify(serviceTriggers, null, 4));
 *         console.log("--------------------------------");
 *     }
 * }
 * main().catch(console.error)
 */
function getServiceTriggers(serviceName) {
    return new Promise((resolve, reject) => {
        winservices.getServiceTriggers(serviceName, (error, serviceTriggers) => {
            if (error) {
                return reject(error);
            }

            return resolve(serviceTriggers);
        });
    });
}

// Exports Methods
module.exports = {
    enumServicesStatus,
    enumDependentServices,
    getServiceConfiguration,
    getServiceTriggers,
    constants: Object.freeze({
        States: kServiceStates,
        Trigger: Object.freeze({
            action: kTriggerActions,
            type: kTriggerTypes
        })
    })
};
