import { concat, EMPTY, of } from 'rxjs';
import { catchError, concatMap, distinctUntilChanged, map, shareReplay } from 'rxjs/operators';

import { RadioNode } from '../communication/node';
import { PackageLayer } from './../communication/package';
import { RadioLayer } from './../communication/radio';
import { getBaseLayer } from './../util';

module.exports = function (RED) {
    function BridgeNode(config) {
        RED.nodes.createNode(this, config);

        const base = getBaseLayer(config.port, RED.log);
        const packageLayer = new PackageLayer(base);
        const radioLayer = new RadioLayer(packageLayer, RED.log);

        this.radio = radioLayer;
        this.connected = base.connected
            .pipe(
                concatMap(isConnected => {
                    if (isConnected) {
                        return concat(radioLayer
                            .init({
                                key: Buffer.from(config.key, 'hex'),
                                powerLevel: 31
                            }).pipe(
                                map(() => isConnected)
                            ), of(isConnected))
                            .pipe(distinctUntilChanged());
                    }
                    return of(isConnected);
                }),
                catchError(err => {
                    this.error(`while initializing communication ${err.message}`);
                    return EMPTY;
                }),
                shareReplay(1)
            );

        this.create = (address: number) => new RadioNode(radioLayer, address);

        this.on('close', () => {
            base.close();
        });

        base.connect();
    }

    RED.nodes.registerType('rfm-bridge', BridgeNode);
};
