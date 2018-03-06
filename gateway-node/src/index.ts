import { RadioLayer } from './communication/radio';
import { SerialLayer } from './communication/serial';
import { PackageLayer } from './communication/package';

import 'rxjs/add/operator/first';

const serialLayer = new SerialLayer('COM5');
const packageLayer = new PackageLayer(serialLayer);
const radioLayer = new RadioLayer(packageLayer);

async function test() {
    try {
        await serialLayer.open();
        await radioLayer.init({ key: Buffer.from([0xd9, 0xc1, 0xbd, 0x60, 0x9c, 0x35, 0x3e, 0x8b, 0xab, 0xbc, 0x8d, 0x35, 0xc7, 0x04, 0x74, 0xef]) });
        radioLayer.data.subscribe(m => console.log(m.from, m.data.toString()));

        while (true) {
            for (let packetSize = 35; packetSize < 58; packetSize++) {
                const packets = 200;
                const start = new Date().getTime();
                for (let i = 0; i < packets; i++) {
                    await radioLayer.send(2, new Buffer(packetSize));
                }
                const time = (new Date().getTime() - start) / 1000 / packets;
                console.log(`${packetSize} -  ${1 / time * packetSize / 1024 * 8} kbps`)
            }
        }
        // await delay(15000);

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