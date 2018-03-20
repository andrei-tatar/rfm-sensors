import { RadioNode } from '../communication/node';

import * as moment from 'moment';
import { Observable } from 'rxjs/Observable';
import { Subject } from 'rxjs/Subject';
import { Subscription } from 'rxjs/Subscription';

module.exports = function (RED) {

    function DimmerNode(config) {
        RED.nodes.createNode(this, config);
        const node = this;

        const bridge = RED.nodes.getNode(config.bridge);
        if (!bridge) { return; }

        const connected: Observable<boolean> = bridge.connected;
        const nodeLayer: RadioNode = bridge.create(parseInt(config.address, 10));
        const periodicSync = Observable.interval(5 * 60000).startWith(0);
        const stop = new Subject();

        let ledBrightness = config.ledbrightness;
        let mode = 0;
        if (config.manualdim) { mode |= 0x01; }
        if (!config.manual) { mode |= 0x02; }

        const syncState = () => Observable.concat(
            nodeLayer.send(Buffer.from([2])), // get status
            nodeLayer.send(Buffer.from([3, mode, config.maxbrightness])), // set mode
            nodeLayer.send(Buffer.from([4, ledBrightness])), // set led bright
        );

        Observable
            .combineLatest(connected, periodicSync)
            .filter(([isConnected]) => isConnected)
            .takeUntil(stop)
            .subscribe(async ([isConnected, index]) => {
                try {
                    await syncState().toPromise();
                } catch (err) {
                    node.error(`while sync: ${err.message}`);
                }
            });

        Observable
            .combineLatest(connected,
                nodeLayer.data.startWith(null).timestamp(),
                Observable.interval(30000).startWith(0))
            .takeUntil(stop)
            .subscribe(([isConnected, msg]) => {
                const lastMessage = msg ? `(${moment(msg.timestamp).fromNow()})` : '';

                node.status(isConnected
                    ? { fill: 'green', shape: 'dot', text: `connected ${lastMessage}` }
                    : { fill: 'red', shape: 'ring', text: 'not connected' });
            });

        const syncRequest = nodeLayer.data.filter(d => d.length === 1 && d[0] === 1)
            .takeUntil(stop)
            .subscribe(async () => {
                try {
                    await syncState().toPromise();
                } catch (err) {
                    node.error(`while sync: ${err.message}`);
                }
            });

        const dimmerBrightness = nodeLayer.data
            .filter(d => d.length === 2 && d[0] === 2)
            .map(d => d[1])
            .distinctUntilChanged();

        dimmerBrightness
            .takeUntil(stop)
            .subscribe(brightness => {
                node.send({
                    payload: brightness,
                    topic: config.topic,
                });
            });

        node.on('input', msg => {
            const value = Math.min(100, Math.max(0, parseInt(msg.payload, 10) || 0));
            if (msg.type === 'led') {
                ledBrightness = value;
                // set led bright
                nodeLayer
                    .send(Buffer.from([4, ledBrightness]))
                    .toPromise()
                    .catch(err => node.error(`while setting led bright: ${err.message}`));
            } else {
                // set bright
                nodeLayer
                    .send(Buffer.from([1, value]))
                    .toPromise()
                    .catch(err => node.error(`while setting bright: ${err.message}`));
            }
        });

        node.on('close', () => {
            stop.next();
            stop.complete();
        });
    }

    RED.nodes.registerType('rfm-dimmer', DimmerNode);
};
