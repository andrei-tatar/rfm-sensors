import { RadioNode } from '../communication/node';
import { SerialLayer } from '../communication/serial';
import { PackageLayer } from './../communication/package';
import { RadioLayer } from './../communication/radio';
import { getBaseLayer } from './../util';

import { Observable } from 'rxjs/Observable';
import { ReplaySubject } from 'rxjs/ReplaySubject';

import '../vendor';

module.exports = function (RED) {
    function BridgeNode(config) {
        RED.nodes.createNode(this, config);
        const node = this;

        const base = getBaseLayer(config.port, RED.log);
        const packageLayer = new PackageLayer(base);
        const radioLayer = new RadioLayer(packageLayer);

        node.connected = base.connected
            .concatMap(isConnected => {
                if (isConnected) {
                    return radioLayer
                        .init({
                            key: Buffer.from(config.key, 'hex'),
                            powerLevel: 31
                        })
                        .map(() => isConnected)
                        .concat(Observable.of(isConnected))
                        .distinctUntilChanged();
                }
                return Observable.of(isConnected);
            })
            .catch(err => {
                node.error(`while initializing communication ${err.message}`);
                return Observable.empty();
            })
            .share();

        node.create = (address: number) => new RadioNode(radioLayer, address);

        node.on('close', () => {
            base.close();
            node.connected.complete();
        });
    }

    RED.nodes.registerType('rfm-bridge', BridgeNode);
};
