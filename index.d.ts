declare namespace Winservices {
    export interface ServiceHost {
        host: string;
    }

    export interface ServiceStates {
        Active: 0;
        Inactive: 1;
        All: 2;
    }

    export interface TriggerActions {
        start: 1;
        stop: 2;
    }

    export interface TriggerTypes {
        custom: 20;
        deviceInterfaceArrival: 1;
        addressAvailability: 2;
        domainJoin: 3;
        fireWallPortEvent: 4;
        groupPolicy: 5;
        networkEndpoint: 6;
    }

    export interface DependentServices {
        [serviceName: string]: Service;
    }

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

    export function enumServicesStatus(desiredState?: keyof ServiceStates, options?: ServiceHost): Promise<Service[]>;
    export function getServiceConfiguration(serviceName: string): Promise<ServiceInformation>;
    export function getServiceTriggers(serviceName: string): Promise<ServiceTrigger[]>;
    export function enumDependentServices(serviceName: string, desiredState?: keyof ServiceStates): Promise<DependentServices>;

    export declare namespace constants {
        export const States: ServiceStates

        export const Trigger: {
            type: TriggerTypes;
            action: TriggerActions;
        }
    }
}

export as namespace Winservices;
export = Winservices;
