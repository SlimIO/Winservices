/** @type {Winservices} */
const winservices = require("bindings")("winservices.node");

winservices.enumServicesStatus(0, function(err, data) {
    if (err) {
        throw new Error(err);
    }
    console.log(data.length);
    for (const service of data) {
        const dep = winservices.enumDependentServices(service.name);
        console.log(dep);
    }
    console.log("done!!!");
});
