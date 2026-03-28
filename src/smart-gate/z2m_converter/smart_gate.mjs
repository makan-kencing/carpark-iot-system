import * as m from "zigbee-herdsman-converters/lib/modernExtend";
import * as exposes from "zigbee-herdsman-converters/lib/exposes";
import {Zcl} from "zigbee-herdsman";

const e = exposes.presets;
const ea = exposes.access;

const smartGateExtend = {
    endpoints: () => m.deviceEndpoints({
        "endpoints": {
            "basic": 1,
        }
    }),
    addCustomCluster: (hasNFC) => {
        let attributes = {
            gateState: {
                ID: 0x0000,
                name: "gateState",
                type: Zcl.DataType.BOOLEAN,
                read: true,
                write: true,
                report: true
            },
            displayText: {
                ID: 0x0001,
                type: Zcl.DataType.CHAR_STR,
                name: "displayText",
                read: true,
                write: true,
                report: true
            }
        }
        let commands = {
            writeDisplayText: {
                ID: 0,
                name:
                    "writeDisplayText",
                parameters:
                    [
                        {name: "text", type: Zcl.DataType.CHAR_STR}
                    ]
            }
        }

        if (hasNFC) {
            attributes["nfcData"] = {
                ID: 0x0002,
                type: Zcl.DataType.ARRAY,
                name: "nfcData",
                read: true,
                report: true
            }
            commands["clearNfc"] = {
                ID: 1,
                name: "clearNfc",
                parameters: []
            }
        }

        return m.deviceAddCustomCluster("control", {
                ID: 0xfd10,
                name: "control",
                attributes: attributes,
                commands: commands,
                commandsResponse: {}
            }
        )
    },
    gate: () => m.binary({
        name: "gate",
        cluster: "control",
        attribute: "gateState",
        valueOn: ["ON", 1],
        valueOff: ["OFF", 0],
        description: "The status of the gate servo.",
        reporting: {
            min: "10_SECONDS",
            max: "1_HOUR",
            change: 1
        },
        access: "ALL"
    }),
    lcd: () => {
        let extend = m.text({
            name: "display_text",
            cluster: "control",
            attribute: "displayText",
            description: "The current displayed text on the lcd screen.",
            access: "ALL",
            validate(value) {
            }
        });
        extend.exposes.push(e.composite("write_display_text", "write_display_text", ea.STATE_SET)
            .withFeature(e.text("text", ea.STATE_SET))
            .withDescription("Display text on the lcd screen")
            .withCategory("config"));
        extend.toZigbee.push(
            {
                key: ["write_display_text"],
                convertSet: async (entity, key, values, meta) => {
                    await entity.command(
                        "control",
                        "writeDisplayText",
                        {
                            text: values.text
                        }
                    );
                },
            }
        );
        return extend;
    },
    nfc: () => {
        let extend = m.text({
            name: "nfc_data",
            cluster: "control",
            attribute: "nfcData",
            description: "The last read nfc data.",
            access: "STATE_GET"
        });
        extend.exposes.push(e.enum("clear_nfc", ea.SET, ["clear_nfc"])
            .withDescription("Clear NFC data")
            .withCategory("config"))
        extend.toZigbee.push({
                key: ["clear_nfc"],
                convertSet: async (entity, key, values, meta) => {
                    await entity.command(
                        "control",
                        "clearNfc",
                        {}
                    );
                },
            }
        );
        return extend;
    }
}

export const definitions = [
    {
        zigbeeModel: ["SGEX"],
        model: "SGEX",
        vendor: "ESPRESSIF",
        description: "Smart gate exit",
        extend: [
            smartGateExtend.endpoints(),
            smartGateExtend.addCustomCluster(true),
            m.identify(),
            smartGateExtend.gate(),
            smartGateExtend.lcd(),
            smartGateExtend.nfc()
        ]
    },
    {
        zigbeeModel: ["SGEN"],
        model: "SGEN",
        vendor: "ESPRESSIF",
        description: "Smart gate entry",
        extend: [
            smartGateExtend.endpoints(),
            smartGateExtend.addCustomCluster(false),
            m.identify(),
            smartGateExtend.gate(),
            smartGateExtend.lcd()
        ]
    }
];
