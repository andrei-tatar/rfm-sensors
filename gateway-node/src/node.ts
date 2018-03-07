import { MessageLayer } from './communication/message';
import { RadioLayer } from './communication/radio';
import { parse } from 'intel-hex';

export class RadioNode implements MessageLayer {
    get data() {
        return this.radio.data.filter(d => d.from === this.address && [0xCA, 0xCB, 0xCC].indexOf(d.data[0]) === -1).map(p => p.data);
    }

    constructor(
        private radio: RadioLayer,
        private address: number,
    ) {
    }

    async send(data: Buffer) {
        await this.radio.send(this.address, data);
    }

    async upload(hex: Buffer) {
        console.time('start');
        await this.beginOta();
        console.timeEnd('start');

        const header = Buffer.from('FLXIMG:99:');
        const data: Buffer = parse(hex).data;
        header.writeUInt16BE(data.length, 7);

        const chunkSize = 52;
        let position = 0;
        console.time('upload');
        while (position < data.length) {
            const count = Math.min(chunkSize, data.length - position);
            const chunk = new Buffer(count);
            data.copy(chunk, 0, position, position + chunkSize);
            const left = await this.writeAt(header.length + position, chunk);
            position += count;
        }

        await this.writeAt(0, header);
        console.timeEnd('upload');
        
        await this.restart();
        console.log('done!');
    }

    private async beginOta() {
        //begin OTA, erase flash
        this.send(Buffer.from([0xCA]));
        await this.radio.data
            .filter(p => p.from === this.address)
            .filter(p => {
                const r = p.data;
                if (r.length === 1 && r[0] === 0xCA) return true;
                if (r.length === 2 && r[0] === 0xCA && r[1] === 0xE1) throw new Error('no flash chip present');
            })
            .first()
            .timeout(2000)
            .toPromise();
    }

    private async writeAt(addr: number, data: Buffer) {
        const pack = new Buffer(1 + 2 + data.length);
        pack[0] = 0xCB; //write at address
        pack.writeUInt16LE(addr, 1);
        data.copy(pack, 3, 0, data.length);
        await this.send(pack);
    }

    private async restart() {
        await this.send(Buffer.from([0xCC]));
    }
}