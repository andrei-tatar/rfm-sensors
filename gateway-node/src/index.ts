import { RadioNode } from './node';
import { RadioLayer } from './communication/radio';
import { SerialLayer } from './communication/serial';
import { PackageLayer } from './communication/package';

import 'rxjs/add/operator/first';
import { readFile } from 'fs';

const serialLayer = new SerialLayer('COM5');
const packageLayer = new PackageLayer(serialLayer);
const radioLayer = new RadioLayer(packageLayer);

async function test() {
    try {
        await serialLayer.open();
        await radioLayer.init({ key: Buffer.from([0xd9, 0xc1, 0xbd, 0x60, 0x9c, 0x35, 0x3e, 0x8b, 0xab, 0xbc, 0x8d, 0x35, 0xc7, 0x04, 0x74, 0xef]) });

        const node = new RadioNode(radioLayer, 2);
        node.data.subscribe(d => console.log(`rx: ${d.toString()}`))

        const hexFile = await new Promise<Buffer>((resolve, reject) => {
            readFile(`D:/Proiecte/rfm-sensors/sensor-template/.pioenvs/pro16MHzatmega328/firmware.hex`, (err, data) => {
                if (err) { reject(err); } else { resolve(data); }
            });
        })
        await node.upload(hexFile);
        await delay(15000);

    } finally {
        await serialLayer.close().catch(e => { });
    }
}

function delay(time: number) {
    return new Promise(resolve => setTimeout(resolve, time));
}

test().catch(err => {
    console.error(err);
});