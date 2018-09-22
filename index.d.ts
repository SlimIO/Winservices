declare namespace Winservices {

    export interface ServiceStates {
        Active: 0,
        Inactive: 1,
        All: 2
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
        type: number;
        startType: number;
        errorControl: number;
        binaryPath: string;
        account: string;
        loadOrderGroup?: string;
        tagId?: number;
        dependencies?: string;
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

    export function enumServicesStatus(desiredState?: keyof ServiceStates): Promise<Service[]>;
    export function getServiceConfiguration(serviceName: string): Promise<ServiceInformation>;
    export function getServiceTriggers(serviceName: string): Promise<ServiceTrigger[]>;
    export function enumDependentServices(serviceName: string, desiredState?: keyof ServiceStates): Promise<DependentServices>;
    export const constants: {
        readonly States: ServiceStates
    };
}

export as namespace Winservices;
export = Winservices;
