import { Observable } from 'rxjs/Observable';
import { MessageLayer } from './communication/message';
import { RadioLayer } from './communication/radio';
import { parse } from 'intel-hex';
import { start } from 'repl';

export class RadioNode implements MessageLayer<Buffer> {
    get data() {
        return this.radio.data.filter(d => d.addr === this.address && [0xCA, 0xCB, 0xCC].indexOf(d.data[0]) === -1).map(p => p.data);
    }

    constructor(
        private radio: RadioLayer,
        private address: number,
    ) {
    }

    send(data: Buffer) {
        return this.radio.send({
            addr: this.address,
            data,
        });
    }

    upload(hex: Buffer) {
        const header = Buffer.from('FLXIMG:99:');
        const data: Buffer = parse(hex).data;
        header.writeUInt16BE(data.length, 7);

        const chunkSize = 52;
        return Observable.concat(
            this.beginOta(),
            Observable.range(0, Math.ceil(data.length / chunkSize))
                .concatMap(chunk => {
                    const position = chunk * chunkSize;
                    const thisChunkSize = Math.min(chunkSize, data.length - position);
                    const chunkBuffer = new Buffer(thisChunkSize);
                    data.copy(chunkBuffer, 0, position, position + chunkSize);
                    console.log(`${position}/${data.length}`);
                    return this.writeAt(header.length + position, chunkBuffer)
                }),
            this.writeAt(0, header),
            this.restart(),
        ).finally(() => console.log('done'));
    }

    private beginOta(timeout = 2000) {
        //begin OTA, erase flash
        return this.radio.data
            .filter(p => p.addr === this.address)
            .filter(p => {
                const r = p.data;
                if (r.length === 1 && r[0] === 0xCA) return true;
                if (r.length === 2 && r[0] === 0xCA && r[1] === 0xE1) throw new Error('no flash chip present');
            })
            .first()
            .timeout(timeout)
            .merge(this.send(Buffer.from([0xCA])));
    }

    private writeAt(addr: number, data: Buffer, tries = 3) {
        const pack = new Buffer(1 + 2 + data.length);
        pack[0] = 0xCB; //write at address
        pack.writeUInt16LE(addr, 1);
        data.copy(pack, 3, 0, data.length);
        return this.send(pack);
    }

    restart() {
        return this.send(Buffer.from([0xCC]));
    }
}