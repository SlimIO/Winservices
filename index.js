/** @type {Winservices} */
const winservices = require("./build/Release/Winservices.node");

console.time("enumServicesStatus");
const services = winservices.enumServicesStatus();
console.timeEnd("enumServicesStatus");

for (const service of services) {
    if (service.name === "Appinfo") {
        continue;
    }
    console.log("\n");
    console.log(`service name: ${service.name}`);

    console.time("getServiceTriggers");
    const info = winservices.getServiceTriggers(service.name);
    console.timeEnd("getServiceTriggers");

    console.log(info);
}
