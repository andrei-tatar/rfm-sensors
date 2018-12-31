import * as moment from 'moment';
import { combineLatest, concat, EMPTY, interval } from 'rxjs';
import { catchError, filter, map, startWith, tap, timestamp } from 'rxjs/operators';

import { PackageLayer } from '../communication/package';
import { getBaseLayer } from '../util';

module.exports = function (RED) {

    function AmpNode(config) {
        RED.nodes.createNode(this, config);
        const base = getBaseLayer(config.port, RED.log);
        const pckg = new PackageLayer(base);
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

            if (typeof msg !== 'object') {
                return;
            }

            if ('on' in msg) {
                todo.push(pckg.send(Buffer.from([0xE0, msg.on ? 1 : 0])));
            }

            if ('channel' in msg && typeof msg.channel === 'number' &&
                msg.channel >= 1 && msg.channel <= 8) {
                todo.push(pckg.send(Buffer.from([0xCA, msg.channel])));
            }

            if ('volume' in msg && typeof msg.volume === 'number' &&
                msg.volume > 0 && msg.volume <= 255) {
                todo.push(pckg.send(Buffer.from([0x10, msg.volume])));
            }


            concat(...todo)
                .pipe(
                    catchError(err => {
                        this.error(`while sending to amp: ${err.message}`);
                        return EMPTY;
                    })
                ).subscribe();
        });

        const subscription = combineLatest(base.connected,
            state.pipe(startWith(null)),
            interval(30000).pipe(startWith(0))
        ).subscribe(([connected, st]) => {
            const lastMessage = st ? `(${moment(st.timestamp).fromNow()})` : '';
            this.status(connected
                ? { fill: 'green', shape: 'dot', text: `connected ${lastMessage}` }
                : { fill: 'red', shape: 'ring', text: 'not connected' });
        });

        this.on('close', () => {
            subscription.unsubscribe();
            base.close();
        });

        base.connect();
    }

    RED.nodes.registerType('rfm-amp', AmpNode);
};
