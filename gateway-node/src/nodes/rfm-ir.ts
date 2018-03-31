import * as moment from 'moment';
import { Observable } from 'rxjs/Observable';
import { Subscription } from 'rxjs/Subscription';

import { getBaseLayer } from '../util';
import { PackageLayer } from './../communication/package';
import { Decoder } from './../decoders/decoder';

module.exports = function (RED) {

    function IrNode(config) {
        const decoder = new Decoder();

        RED.nodes.createNode(this, config);
        const base = getBaseLayer(config.port, RED.log);
        const pckg = new PackageLayer(base);
        const state = pckg.data
            .map(msg => {
                if (msg[0] === 1) {
                    const pulses: number[] = [];
                    for (let i = 1; i < msg.length; i++) {
                        pulses.push(msg[i] * 50);
                    }
                    return decoder.decode(pulses);
                }
                return null;
            })
            .filter(msg => !!msg)
            .do(s => {
                this.send({ payload: s });
            })
            .timestamp();

        this.on('input', msg => {
            if (typeof msg.payload !== 'string') { return; }

            const pulses = decoder.encode(msg.payload);
            if (!pulses) { return; }

            const buffer = Buffer.from([1, ...pulses.map(p => Math.round(p / 50))]);
            pckg.send(buffer).catch(err => this.error(`while sending code: ${err.message}`));
        });

        const subscription =
            Observable
                .combineLatest(base.connected, state.startWith(null), Observable.interval(30000).startWith(0))
                .subscribe(([connected, st]) => {
                    const lastMessage = st ? `(${moment(st.timestamp).fromNow()})` : '';
                    this.status(connected
                        ? { fill: 'green', shape: 'dot', text: `connected ${lastMessage}` }
                        : { fill: 'red', shape: 'ring', text: 'not connected' });
                });

        this.on('close', () => subscription.unsubscribe());

        base.connect();
    }

    RED.nodes.registerType('rfm-ir', IrNode);
};
