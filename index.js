/** @type {Winservices} */
const winservices = require("./build/Release/Winservices.node");

console.time("enumServicesStatus");
const services = winservices.enumServicesStatus();
console.timeEnd("enumServicesStatus");
console.log(services.length);

for (const service of services) {
    if (service.process.serviceType === 48) {
        continue;
    }
    console.log(service);
    // console.time("getServiceConfiguration");
    // const config = winservices.getServiceConfiguration(service.name);
    // console.timeEnd("getServiceConfiguration");
    // console.log(config);
    // console.log("\n");

    console.time("getServiceTriggers");
    const perf = winservices.getServiceTriggers(service.name);
    console.timeEnd("getServiceTriggers");

    console.log(perf);
    console.log("\n\n");
}
