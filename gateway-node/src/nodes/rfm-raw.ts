import { RadioNode } from '../communication/node';

import * as moment from 'moment';
import { Observable } from 'rxjs/Observable';
import { Subject } from 'rxjs/Subject';

module.exports = function (RED) {

    function RawNode(config) {
        RED.nodes.createNode(this, config);
        const node = this;

        const bridge = RED.nodes.getNode(config.bridge);
        if (!bridge) { return; }

        const connected: Observable<boolean> = bridge.connected;
        const nodeLayer: RadioNode = bridge.create(parseInt(config.address, 10));
        const periodicSync = Observable.interval(5 * 60000).startWith(0);
        const stop = new Subject();

        const updateStatus = new Subject();
        let uploading = false;

        Observable
            .combineLatest(connected,
                nodeLayer.data.startWith(null).merge(updateStatus).timestamp(),
                Observable.interval(30000).startWith(0))
            .takeUntil(stop)
            .filter(() => !uploading)
            .subscribe(([isConnected, msg]) => {
                const lastMessage = msg.value ? `(${moment(msg.timestamp).fromNow()})` : '';

                node.status(isConnected
                    ? { fill: 'green', shape: 'dot', text: `connected ${lastMessage}` }
                    : { fill: 'red', shape: 'ring', text: 'not connected' });
            });

        nodeLayer.data
            .takeUntil(stop)
            .subscribe(data =>
                node.send({
                    payload: data,
                    topic: config.topic,
                }));

        node.on('input', msg => {
            if (msg.type === 'firmware') {
                const hex = Buffer.isBuffer(msg.payload) ? msg.payload : Buffer.from(msg.payload);
                const progress = new Subject<number>();
                progress.throttleTime(1000).subscribe(p => {
                    node.status({ fill: 'green', shape: 'dot', text: `upload ${Math.round(p)} %` });
                });
                uploading = true;
                nodeLayer.upload(hex, progress)
                    .toPromise()
                    .then(() => {
                        node.status({ fill: 'green', shape: 'dot', text: `upload done!` });
                        return Observable.timer(1000).toPromise();
                    })
                    .catch(err => node.error(`while uploading hex: ${err.message}`))
                    .then(() => {
                        uploading = false;
                        updateStatus.next();
                    });
            } else {
                const data = Buffer.isBuffer(msg.payload) ? msg.payload : Buffer.from(msg.payload);
                nodeLayer
                    .send(data)
                    .toPromise()
                    .catch(err => node.error(`while setting bright: ${err.message}`));
            }
        });

        node.on('close', () => {
            stop.next();
            stop.complete();
        });
    }

    RED.nodes.registerType('rfm-raw', RawNode);
};
