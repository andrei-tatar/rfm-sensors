import * as moment from 'moment';
import { combineLatest, interval, merge, Observable, Subject, defer, concat, of } from 'rxjs';
import {
    catchError, delay, filter, startWith,
    switchMap, takeUntil, throttleTime, timestamp, finalize
} from 'rxjs/operators';

import { RadioNode } from '../communication/node';

module.exports = function (RED) {

    function RawNode(config) {
        RED.nodes.createNode(this, config);
        const node = this;

        const bridge = RED.nodes.getNode(config.bridge);
        if (!bridge) { return; }

        const connected: Observable<boolean> = bridge.connected;
        const nodeLayer: RadioNode = bridge.create(parseInt(config.address, 10));
        const stop = new Subject();

        const updateStatus = new Subject();
        let uploading = false;

        combineLatest(connected,
            merge(nodeLayer.data.pipe(startWith(null)), updateStatus).pipe(timestamp()),
            interval(30000).pipe(startWith(0)))
            .pipe(
                takeUntil(stop),
                filter(() => !uploading)
            )
            .subscribe(([isConnected, msg]) => {
                const lastMessage = msg.value ? `(${moment(msg.timestamp).fromNow()})` : '';

                node.status(isConnected
                    ? { fill: 'green', shape: 'dot', text: `connected ${lastMessage}` }
                    : { fill: 'red', shape: 'ring', text: 'not connected' });
            });

        nodeLayer.data.pipe(takeUntil(stop))
            .subscribe(data =>
                node.send({
                    payload: data,
                    topic: config.topic,
                }));

        node.on('input', msg => {
            if (msg.type === 'firmware') {
                const hex = Buffer.isBuffer(msg.payload) ? msg.payload : Buffer.from(msg.payload);
                const progress = new Subject<number>();
                progress.pipe(throttleTime(1000)).subscribe(p => {
                    node.status({ fill: 'green', shape: 'dot', text: `upload ${Math.round(p)} %` });
                });
                uploading = true;
                concat(
                    nodeLayer.upload(hex, progress),
                    defer(() => node.status({ fill: 'green', shape: 'dot', text: `upload done!` }))
                ).pipe(
                    catchError(err => {
                        node.error(`while uploading hex: ${err.message}`);
                        return of(null);
                    }),
                    delay(5000),
                    finalize(() => {
                        uploading = false;
                        updateStatus.next();
                        progress.complete();
                    }),
                    takeUntil(stop),
                ).subscribe();
            } else {
                const data = Buffer.isBuffer(msg.payload) ? msg.payload : Buffer.from(msg.payload);
                nodeLayer.send(data).pipe(
                    catchError(err => node.error(`while setting bright: ${err.message}`)),
                    takeUntil(stop),
                ).subscribe();
            }
        });

        node.on('close', () => {
            stop.next();
            stop.complete();
        });
    }

    RED.nodes.registerType('rfm-raw', RawNode);
};
