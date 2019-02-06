if (process.platform !== "win32") {
    throw new Error("Only Windows OS is supported by this test!");
}

// Require Third-party dependencies
const test = require("ava");
const is = require("@slimio/is");

// Require package
const winservices = require("../index");

// REGEX
const ReFailedOpenService = /Failed\sto\sopen\sService\shandle/;

// Test method getLogicalDrives
test("Verify constants object", function verifyConstant(assert) {
    assert.is(Reflect.has(winservices, "constants"), true);
    assert.is(Reflect.has(winservices.constants, "States"), true);
    assert.is(Object.isFrozen(winservices.constants), true);

    const States = winservices.constants.States;
    assert.is(States.Active, 0);
    assert.is(States.Inactive, 1);
    assert.is(States.All, 2);
});

// Check Service payload
function checkService(assert, service, strict = true) {
    assert.is(is.plainObject(service), true);
    assert.is(is.string(service.name), true);
    assert.is(is.string(service.displayName), true);
    assert.is(is.plainObject(service.process), true);
    const process = service.process;
    if (strict) {
        assert.is(is.number(process.id), true);
        assert.is(is.string(process.name), true);
        assert.is(is.number(process.serviceFlags), true);
        assert.is(is.number(process.checkPoint), true);
    }
    assert.is(is.number(process.currentState), true);
    assert.is(is.number(process.serviceType), true);
    assert.is(is.number(process.controlsAccepted), true);
    assert.is(is.number(process.serviceSpecificExitCode), true);
    assert.is(is.number(process.waitHint), true);
    assert.is(is.number(process.win32ExitCode), true);
}

// Test method enumServicesStatus(States.All)
test("enumServicesStatus(All)", async function enumServicesStatus(assert) {
    assert.is(Reflect.has(winservices, "enumServicesStatus"), true);

    const allServices = await winservices.enumServicesStatus();
    assert.is(is.array(allServices), true);
    assert.is(allServices.length > 0, true);

    for (const service of allServices) {
        checkService(assert, service);
    }
});

// enumServicesStatus - desiredState
test("enumServicesStatus - desiredState should be typeof number", async function enumServicesStatus(assert) {
    assert.is(Reflect.has(winservices, "enumServicesStatus"), true);

    await assert.throwsAsync(winservices.enumServicesStatus("yop"), {
        instanceOf: Error,
        message: "Argument desiredState should be typeof number!"
    });
});

// Test method enumServicesStatus(States.Active)
test("enumServicesStatus(Active)", async function enumServicesStatus(assert) {
    assert.is(Reflect.has(winservices, "enumServicesStatus"), true);

    const allServices = await winservices.enumServicesStatus(winservices.constants.States.Active);
    assert.is(is.array(allServices), true);
    assert.is(allServices.length > 0, true);

    for (const service of allServices) {
        checkService(assert, service);
    }
});

// Test method enumServicesStatus(States.Inactive)
test("enumServicesStatus(Inactive)", async function enumServicesStatus(assert) {
    assert.is(Reflect.has(winservices, "enumServicesStatus"), true);

    const allServices = await winservices.enumServicesStatus(winservices.constants.States.Inactive);
    assert.is(is.array(allServices), true);
    assert.is(allServices.length > 0, true);

    for (const service of allServices) {
        checkService(assert, service);
    }
});

// getServiceConfiguration - serviceName
test("getServiceConfiguration - serviceName should be typeof string", async function enumServicesStatus(assert) {
    assert.is(Reflect.has(winservices, "getServiceConfiguration"), true);

    await assert.throwsAsync(winservices.getServiceConfiguration(10), {
        instanceOf: Error,
        message: "argument serviceName should be typeof string!"
    });
});

// Test method getServiceConfiguration
test("getServiceConfiguration", async function getServiceConfiguration(assert) {
    assert.is(Reflect.has(winservices, "enumServicesStatus"), true);
    assert.is(Reflect.has(winservices, "getServiceConfiguration"), true);

    const allServices = await winservices.enumServicesStatus();
    assert.is(is.array(allServices), true);
    assert.is(allServices.length > 0, true);

    for (const service of allServices) {
        assert.is(is.string(service.name), true);
        try {
            const config = await winservices.getServiceConfiguration(service.name);
            assert.is(is.plainObject(config), true);
            assert.is(is.string(config.type), true);
            assert.is(is.string(config.errorControl), true);
            assert.is(is.string(config.startType), true);
            assert.is(is.string(config.binaryPath), true);
            assert.is(is.string(config.account), true);
            if (!is.nullOrUndefined(config.loadOrderGroup)) {
                assert.is(is.string(config.loadOrderGroup), true);
            }
            if (!is.nullOrUndefined(config.dependencies)) {
                assert.is(is.string(config.dependencies), true);
            }
            if (!is.nullOrUndefined(config.tagId)) {
                assert.is(is.number(config.tagId), true);
            }
            if (!is.nullOrUndefined(config.description)) {
                assert.is(is.string(config.description), true);
            }
        }
        catch (err) {
            console.log(`service ${service.name} failed !`);
            // do nothing
        }
    }
});

// getServiceTriggers - serviceName
test("getServiceTriggers - serviceName should be typeof string", async function enumServicesStatus(assert) {
    assert.is(Reflect.has(winservices, "getServiceTriggers"), true);

    await assert.throwsAsync(winservices.getServiceTriggers(10), {
        instanceOf: Error,
        message: "argument serviceName should be typeof string!"
    });
});

// Test method getServiceTriggers
test("getServiceTriggers", async function getServiceTriggers(assert) {
    assert.is(Reflect.has(winservices, "enumServicesStatus"), true);
    assert.is(Reflect.has(winservices, "getServiceTriggers"), true);

    const allServices = await winservices.enumServicesStatus();
    assert.is(is.array(allServices), true);
    assert.is(allServices.length > 0, true);

    for (const service of allServices) {
        assert.is(is.string(service.name), true);
        const triggers = await winservices.getServiceTriggers(service.name);
        assert.is(is.array(triggers), true);
        for (const trigger of triggers) {
            assert.is(is.plainObject(trigger), true);
            assert.is(is.number(trigger.type), true);
            assert.is(is.number(trigger.action), true);
            assert.is(is.string(trigger.guid), true);
            assert.is(is.array(trigger.dataItems), true);
            for (const item of trigger.dataItems) {
                assert.is(is.number(item.dataType), true);
            }
        }
    }
});

// enumDependentServices - serviceName
test("enumDependentServices - serviceName should be typeof string", async function enumServicesStatus(assert) {
    assert.is(Reflect.has(winservices, "enumDependentServices"), true);

    await assert.throwsAsync(winservices.enumDependentServices(10), {
        instanceOf: Error,
        message: "argument serviceName should be typeof string!"
    });
});


// Test method enumDependentServices
test("enumDependentServices", async function enumDependentServices(assert) {
    assert.is(Reflect.has(winservices, "enumServicesStatus"), true);
    assert.is(Reflect.has(winservices, "enumDependentServices"), true);

    const allServices = await winservices.enumServicesStatus();
    assert.is(is.array(allServices), true);
    assert.is(allServices.length > 0, true);

    for (const service of allServices) {
        assert.is(is.string(service.name), true);
        try {
            const dependentServices = await winservices.enumDependentServices(service.name);
            assert.is(is.plainObject(dependentServices), true);
            for (const [serviceName, depService] of Object.entries(dependentServices)) {
                assert.is(is.string(serviceName), true);
                checkService(assert, depService, false);
            }
        }
        catch (error) {
            assert.is(ReFailedOpenService.test(error), true);
        }
    }
});

