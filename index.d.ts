declare namespace Winservices {

    export enum ServiceState {
        ACTIVE = 0,
        INACTIVE = 1,
        ALL = 2
    }

    export interface Service {
        name: string,
        displayName: string;
        process: {
            id?: number;
            name?: string;
            currentState: number;
            serviceType: number;
            checkPoint: number;
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

    export function enumServicesStatus(desiredState?: ServiceState): Service[];
    export function getServiceConfiguration(serviceName: string): ServiceInformation;
    export function getServiceTriggers(serviceName: string): ServiceTrigger[];
    export function enumDependentServices(serviceName: string): Service[];
}

export as namespace Winservices;
export = Winservices;
