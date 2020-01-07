import * as moment from 'moment';
import { combineLatest, concat, defer, EMPTY, interval, merge, of, Subject } from 'rxjs';
import {
    catchError, delay, filter, finalize,
    startWith, takeUntil, throttleTime, timestamp
} from 'rxjs/operators';

import { RadioNode } from '../communication/node';
import { timeSpan } from '../util';
import { NodeRedNode } from './contracts';

module.exports = function (RED) {

    function RawNode(this: NodeRedNode, config) {
        RED.nodes.createNode(this, config);

        const bridge = RED.nodes.getNode(config.bridge);
        if (!bridge) { return; }

        const nodeLayer: RadioNode = bridge.create(parseInt(config.address, 10));
        const stop = new Subject();

        const updateStatus = new Subject();
        let uploading = false;

        combineLatest([
            nodeLayer.connected,
            merge(nodeLayer.data.pipe(startWith(null)), updateStatus).pipe(timestamp()),
            interval(timeSpan(30, 'sec')).pipe(startWith(0)),
        ]).pipe(
            takeUntil(stop),
            filter(() => !uploading)
        ).subscribe(([isConnected, msg]) => {
            const lastMessage = msg.value ? `(${moment(msg.timestamp).fromNow()})` : '';

            this.status(isConnected
                ? { fill: 'green', shape: 'dot', text: `connected ${lastMessage}` }
                : { fill: 'red', shape: 'ring', text: 'not connected' });
        });

        nodeLayer.data.pipe(takeUntil(stop))
            .subscribe(data =>
                this.send({
                    payload: data,
                    topic: config.topic,
                }));

        this.on('input', msg => {
            if (msg.topic === 'firmware') {
                const hex = Buffer.isBuffer(msg.payload) ? msg.payload : Buffer.from(msg.payload);
                const progress = new Subject<number>();
                progress.pipe(throttleTime(timeSpan(1, 'sec'))).subscribe(p => {
                    this.status({ fill: 'green', shape: 'dot', text: `upload ${Math.round(p)} %` });
                });
                uploading = true;
                concat(
                    nodeLayer.upload(hex, progress),
                    defer(() => this.status({ fill: 'green', shape: 'dot', text: `upload done!` }))
                ).pipe(
                    catchError(err => {
                        this.error(`while uploading hex: ${err.message}`);
                        return of(null);
                    }),
                    delay(timeSpan(5, 'sec')),
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
                    catchError(err => {
                        this.error(`while sending: ${err.message}`);
                        return EMPTY;
                    }),
                    takeUntil(stop),
                ).subscribe();
            }
        });

        this.on('close', () => {
            stop.next();
            stop.complete();
        });
    }

    RED.nodes.registerType('rfm-raw', RawNode);
};
