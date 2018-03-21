import { Observable } from 'rxjs/Observable';
import { Observer } from 'rxjs/Observer';
import { MessageLayer } from './message';

export class RadioNode implements MessageLayer<Buffer> {
    get data() {
        return this.below.data.filter(d => d.addr === this.address && [0xCA, 0xCB, 0xCC].indexOf(d.data[0]) === -1).map(p => p.data);
    }

    constructor(
        private below: MessageLayer<{ addr: number, data: Buffer }>,
        public readonly address: number,
    ) {
    }

    send(data: Buffer) {
        return this.below
            .send({
                addr: this.address,
                data,
            })
            .catch(err => Observable.throw(err).materialize().delay(200).dematerialize())
            .retry(3);
    }

    upload(hex: Buffer, progress: Observer<number> = null) {
        const header = Buffer.from('FLXIMG:99:');
        const data: Buffer = require('intel-hex').parse(hex).data;
        header.writeUInt16BE(data.length, 7);
        const chunkSize = 52;
        const chunks = Math.ceil(data.length / chunkSize);
        let currentStep = 0;
        const reportStepInc = () => progress.next(currentStep++ / (chunks + 3) * 100);
        const reportStepWhenDone = progress
            ? (obs: Observable<any>) => obs.do(() => { }, () => { }, () => {
                reportStepInc();
            })
            : (obs: Observable<any>) => obs;
        if (progress) { reportStepInc(); }
        return Observable.concat(
            reportStepWhenDone(this.beginOta()),
            Observable.range(0, chunks)
                .concatMap(chunk => {
                    const position = chunk * chunkSize;
                    const thisChunkSize = Math.min(chunkSize, data.length - position);
                    const chunkBuffer = new Buffer(thisChunkSize);
                    data.copy(chunkBuffer, 0, position, position + chunkSize);
                    return reportStepWhenDone(this.writeAt(header.length + position, chunkBuffer));
                }),
            reportStepWhenDone(this.writeAt(0, header)),
            reportStepWhenDone(this.restart()),
        ).finally(() => {
            if (progress) { progress.complete(); }
        });
    }

    private beginOta(timeout = 2000) {
        // begin OTA, erase flash
        return this.below.data
            .filter(p => p.addr === this.address)
            .filter(p => {
                const r = p.data;
                if (r.length === 1 && r[0] === 0xCA) { return true; }
                if (r.length === 2 && r[0] === 0xCA && r[1] === 0xE1) { throw new Error('no flash chip present'); }
            })
            .first()
            .timeout(timeout)
            .merge(this.send(Buffer.from([0xCA])));
    }

    private writeAt(addr: number, data: Buffer, tries = 3) {
        const pack = new Buffer(1 + 2 + data.length);
        pack[0] = 0xCB; // write at address
        pack.writeUInt16LE(addr, 1);
        data.copy(pack, 3, 0, data.length);
        return this.send(pack);
    }

    restart() {
        return this.send(Buffer.from([0xCC]));
    }
}
