/** @type {Winservices} */
const winservices = require("bindings")("winservices.node");

winservices.enumServicesStatus(0, function(err, data) {
    if (err) {
        throw new Error(err);
    }
    console.log(data.length);
    for (const service of data) {
        const triggers = winservices.getServiceTriggers(service.name);
        if (triggers.length > 0) {
            console.log(JSON.stringify(triggers, null, 2));
        }
    }
    console.log("done!!!");
});
