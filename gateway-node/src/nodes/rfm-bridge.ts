import { Subject } from 'rxjs';

import { RadioNode } from '../communication/node';
import { RadioLayer } from '../communication/radio';
import { getRadioLayer } from './../util';
import { NodeRedNode } from './contracts';

module.exports = function (RED) {
    function BridgeNode(this: NodeRedNode & { create(addr: number): RadioNode }, config) {
        RED.nodes.createNode(this, config);
        const radioLayers = new Map<string, RadioLayer>();
        const close$ = new Subject();

        this.create = (address: number) => {
            const connectionString = address < 1000 ? config.port : config.port1;
            let radioLayer = radioLayers.get(connectionString);
            if (!radioLayer) {
                radioLayer = getRadioLayer(connectionString, RED.log);
                radioLayers.set(connectionString, radioLayer);
            }
            return new RadioNode(radioLayer, address % 1000);
        };
        this.on('close', () => {
            close$.next();
            close$.complete();
        });
    }

    RED.nodes.registerType('rfm-bridge', BridgeNode);
};
