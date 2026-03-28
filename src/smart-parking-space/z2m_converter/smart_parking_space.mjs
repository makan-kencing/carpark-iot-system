import * as m from "zigbee-herdsman-converters/lib/modernExtend";
import {Zcl} from "zigbee-herdsman";

const smartParkingSpaceExtend = {
    endpoints: () => m.deviceEndpoints({
        "endpoints": {
            "basic": 1,
        }
    }),
    addCustomCluster: () => m.deviceAddCustomCluster("genAnalogInput", {
            ID:  12 ,
            name: "genAnalogInput",
            attributes: {
                total: {
                    ID: 0x0000,
                    name: "total",
                    type: Zcl.DataType.UINT8,
                    min: 0,
                    max: 255,
                    read: true,
                    report: true
                },
                remaining: {
                    ID: 0x0001,
                    name: "remaining",
                    type: Zcl.DataType.UINT8,
                    min: 0,
                    max: 255,
                    read: true,
                    report: true
                }
            },
            commands: {},
            commandsResponse: {}
        }
    )
}

export const definitions = [
    {
        zigbeeModel: ["SPS3"],
        model: "SPS3",
        vendor: "ESPRESSIF",
        description: "Smart parking space (3 spaces)",
        extend: [
            smartParkingSpaceExtend.endpoints(),
            smartParkingSpaceExtend.addCustomCluster(),
            m.identify(),
            m.numeric({
                name: "total",
                cluster: "genAnalogInput",
                attribute: "total",
                description: "The total space for this parking space",
                access: "STATE"
            }),
            m.numeric({
                name: "remaining",
                cluster: "genAnalogInput",
                attribute: "remaining",
                description: "The remaining unoccupied space",
                access: "STATE"
            })
        ]
    }
];

export default definitions;
