// Require Third-party dependencies
const test = require("ava");
const is = require("@sindresorhus/is");

// Require package
const winservices = require("../index");

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

// Test method getServiceConfiguration
test("getServiceConfiguration", async function getServiceConfiguration(assert) {
    assert.is(Reflect.has(winservices, "enumServicesStatus"), true);
    assert.is(Reflect.has(winservices, "getServiceConfiguration"), true);

    const allServices = await winservices.enumServicesStatus();
    assert.is(is.array(allServices), true);
    assert.is(allServices.length > 0, true);

    for (const service of allServices) {
        assert.is(is.string(service.name), true);
        const config = await winservices.getServiceConfiguration(service.name);
        assert.is(is.number(config.type), true);
        assert.is(is.number(config.errorControl), true);
        assert.is(is.number(config.startType), true);
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
    }
});

// Test method enumDependentServices
test("enumDependentServices", async function enumServicesStatus(assert) {
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
            // console.log(error);
        }
    }
});

