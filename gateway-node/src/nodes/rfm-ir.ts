import * as moment from 'moment';
import { combineLatest, EMPTY, interval } from 'rxjs';
import { catchError, filter, map, startWith, tap, timestamp } from 'rxjs/operators';

import { getPackageLayer } from '../util';
import { Decoder } from './../decoders/decoder';

module.exports = function (RED) {

    function IrNode(config) {
        const decoder = new Decoder();

        RED.nodes.createNode(this, config);
        const pckg = getPackageLayer(config.port, RED.log);
        const state = pckg.data.pipe(
            map(msg => {
                if (msg[0] === 1) {
                    const pulses: number[] = [];
                    for (let i = 1; i < msg.length; i++) {
                        pulses.push(msg[i] * 50);
                    }
                    return decoder.decode(pulses);
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
            if (typeof msg.payload !== 'string') { return; }

            const pulses = decoder.encode(msg.payload);
            if (!pulses) { return; }

            const buffer = Buffer.from([1, ...pulses.map(p => Math.round(p / 50))]);
            pckg.send(buffer)
                .pipe(
                    catchError(err => {
                        this.error(`while sending code: ${err.message}`);
                        return EMPTY;
                    })
                ).subscribe();
        });

        const subscription = combineLatest(pckg.connected,
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

    RED.nodes.registerType('rfm-ir', IrNode);
};
