/** @type {Winservices} */
const winservices = require("bindings")("winservices.node");

const services = winservices.enumServicesStatus();
for (const service of services) {
    console.log(`service name => ${service.name}`);
    const perf = winservices.getServiceTriggers(service.name);

    console.log(perf);
    console.log("\n\n");
}
