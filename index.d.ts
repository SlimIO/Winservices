declare namespace Winservices {

    export interface Service {
        name: string,
        displayName: string;
        process: {
            id: number;
            name: string;
            currentState: number;
            serviceType: number;
            checkPoint: number;
            controlsAccepted: number;
            serviceFlags: number;
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
        description?: string;
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
        data: any;
    }

    export function enumServicesStatus(): Service[];
    export function getServiceConfiguration(serviceName: string): ServiceInformation;
    export function getServiceTriggers(serviceName: string): ServiceTrigger[];
}

export as namespace Winservices;
export = Winservices;
