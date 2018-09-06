import { RadioNode } from '../communication/node';

import * as moment from 'moment';
import { combineLatest, concat, EMPTY, interval, merge, Observable, of, Subject, defer } from 'rxjs';
import {
    catchError, delay, delayWhen, filter, map,
    startWith, switchMap, takeUntil, tap, throttleTime, timestamp, finalize
} from 'rxjs/operators';

module.exports = function (RED) {

    function DimmerNode(config) {
        RED.nodes.createNode(this, config);

        const bridge = RED.nodes.getNode(config.bridge);
        if (!bridge) { return; }

        const connected: Observable<boolean> = bridge.connected;
        const nodeLayer: RadioNode = bridge.create(parseInt(config.address, 10));
        const periodicSync = interval(5 * 60000).pipe(startWith(0));
        const stop = new Subject();

        let ledBrightness = config.ledbrightness;
        let mode = 0;
        if (config.manualdim) { mode |= 0x01; }
        if (!config.manual) { mode |= 0x02; }

        const syncState = () => concat(
            nodeLayer.send(Buffer.from([2])), // get status
            nodeLayer.send(Buffer.from([3, mode, config.maxbrightness])), // set mode
            nodeLayer.send(Buffer.from([4, ledBrightness])), // set led bright
        );

        combineLatest(connected, periodicSync)
            .pipe(
                filter(([isConnected]) => isConnected),
                takeUntil(stop),
                delayWhen(_ => of(Math.round(Math.random() * 50) * 200)),
                switchMap(_ => syncState().pipe(catchError(err => {
                    this.error(`while sync: ${err.message}`);
                    return EMPTY;
                })))
            )
            .subscribe();

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

                this.status(isConnected
                    ? { fill: 'green', shape: 'dot', text: `connected ${lastMessage}` }
                    : { fill: 'red', shape: 'ring', text: 'not connected' });
            });

        nodeLayer.data
            .pipe(
                filter(d => d.length === 1 && d[0] === 1),
                takeUntil(stop),
                switchMap(_ => syncState().pipe(catchError(err => {
                    this.error(`while sync: ${err.message}`);
                    return EMPTY;
                })))
            )
            .subscribe();

        nodeLayer.data
            .pipe(
                filter(d => d.length === 2 && d[0] === 2),
                map(d => d[1]),
                takeUntil(stop)
            ).subscribe(brightness => {
                this.send({
                    payload: brightness,
                    topic: config.topic,
                });
            });

        this.on('input', msg => {
            if (msg.type === 'firmware') {
                const hex = Buffer.isBuffer(msg.payload) ? msg.payload : Buffer.from(msg.payload);
                const progress = new Subject<number>();
                progress.pipe(throttleTime(1000)).subscribe(p => {
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
                    delay(5000),
                    finalize(() => {
                        uploading = false;
                        updateStatus.next();
                    }),
                    takeUntil(stop)
                ).subscribe();
            } else {
                const value = Math.min(100, Math.max(0, parseInt(msg.payload, 10)));
                if (!isFinite(value)) {
                    return;
                }
                if (msg.type === 'led') {
                    ledBrightness = value;
                    // set led bright
                    nodeLayer
                        .send(Buffer.from([4, ledBrightness]))
                        .pipe(
                            catchError(err => {
                                this.error(`while setting led bright: ${err.message}`);
                                return EMPTY;
                            }),
                            takeUntil(stop)
                        )
                        .subscribe();
                } else {
                    // set bright
                    concat(
                        nodeLayer.send(Buffer.from([1, value])),
                        defer(() => this.send({
                            payload: value,
                            topic: config.topic,
                        }))
                    ).pipe(
                        catchError(err => {
                            this.error(`while setting bright: ${err.message}`);
                            return EMPTY;
                        }),
                        takeUntil(stop)
                    ).subscribe();
                }
            }
        });

        this.on('close', () => {
            stop.next();
            stop.complete();
        });
    }

    RED.nodes.registerType('rfm-dimmer', DimmerNode);
};
