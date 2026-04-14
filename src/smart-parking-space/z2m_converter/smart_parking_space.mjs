import * as m from "zigbee-herdsman-converters/lib/modernExtend";

const smartParkingSpaceExtend = {
    endpoints: () => m.deviceEndpoints({
        "endpoints": {
            "basic": 1,
        }
    }),
}

export const definitions = [
    {
        zigbeeModel: ["SPS3"],
        model: "SPS3",
        vendor: "ESPRESSIF",
        description: "Smart parking space (3 spaces)",
        extend: [
            smartParkingSpaceExtend.endpoints(),
            m.identify(),
            m.temperature(),
        ]
    }
];

export default definitions;
