import * as moment from 'moment';
import { combineLatest, concat, EMPTY, interval, merge, of } from 'rxjs';
import { catchError, distinctUntilChanged, filter, ignoreElements, map, startWith, switchMap, tap, timestamp } from 'rxjs/operators';

import { getPackageLayer } from '../util';

module.exports = function (RED) {

    function AmpNode(config) {
        RED.nodes.createNode(this, config);
        const pckg = getPackageLayer(config.port, RED.log);
        const state = pckg.data.pipe(
            map(msg => {
                if (msg[0] === 1 && msg.length === 4) {
                    return {
                        on: msg[1] !== 0,
                        channel: msg[2],
                        volume: msg[3],
                    };
                }
                return null;
            }),
            filter(msg => !!msg),
            tap(s => {
                this.send({ payload: s });
            }),
            timestamp()
        );

        this.on('input', msg => {

            const todo = [];

            if (typeof msg !== 'object' || typeof msg.payload !== 'object') {
                return;
            }

            const p = msg.payload;
            if ('on' in p) {
                todo.push(pckg.send(Buffer.from([0xE0, p.on ? 1 : 0])));
            }

            if ('channel' in p && typeof p.channel === 'number' &&
                p.channel >= 1 && p.channel <= 8) {
                todo.push(pckg.send(Buffer.from([0xCA, p.channel])));
            }

            if ('volume' in p && typeof p.volume === 'number' &&
                p.volume > 0 && p.volume <= 255) {
                todo.push(pckg.send(Buffer.from([0x10, p.volume])));
            }


            concat(...todo)
                .pipe(
                    catchError(err => {
                        this.error(`while sending to amp: ${err.message}`);
                        return EMPTY;
                    })
                ).subscribe();
        });

        const connected$ = pckg.connected.pipe(
            switchMap(connected => {
                if (connected) {
                    return concat(
                        pckg.send(Buffer.from([0x01])).pipe(catchError(err => {
                            RED.log.warn(`rfm-amp: while sync ${err.message}`);
                            return EMPTY;
                        })),
                        of(connected),
                    );
                }
                return of(connected);
            }),
            distinctUntilChanged(),
        );

        const subscription = combineLatest(
            connected$,
            state.pipe(startWith(null)),
            interval(30000).pipe(startWith(0))
        ).subscribe(([connected, st]) => {
            const lastMessage = st ? `(${moment(st.timestamp).fromNow()})` : '';
            this.status(connected
                ? { fill: 'green', shape: 'dot', text: `connected ${lastMessage}` }
                : { fill: 'red', shape: 'ring', text: 'not connected' });
        });

        this.on('close', () => subscription.unsubscribe());
    }

    RED.nodes.registerType('rfm-amp', AmpNode);
};
