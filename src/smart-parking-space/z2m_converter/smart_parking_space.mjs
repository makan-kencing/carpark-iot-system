import * as m from "zigbee-herdsman-converters/lib/modernExtend";
import {Zcl} from "zigbee-herdsman";

export const definitions = [
    {
        zigbeeModel: ["SPS3"],
        model: "SPS3",
        vendor: "ESPRESSIF",
        description: "Smart parking space (3 spaces)",
        extend: [
            m.quirkAddEndpointCluster({
                endpointID: 1,
                inputClusters: ["genAnalogInput"]
            }),
            m.identify(),
            m.numeric({
                name: "total",
                cluster: "genAnalogInput",
                attribute: "maxPresentValue",
                description: "The total space supported by this parking sensor",
                access: "STATE_GET"
            }),
            m.numeric({
                name: "remaining",
                cluster: "genAnalogInput",
                attribute: "presentValue",
                description: "The remaining space unoccupied in this parking sensor",
                access: "STATE_GET"
            })
        ]
    }
];

export default definitions;
