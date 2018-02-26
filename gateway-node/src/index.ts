import { RadioLayer } from './communication/radio';
import { SerialLayer } from './communication/serial';
import { PackageLayer } from './communication/package';

import 'rxjs/add/operator/first';

const serialLayer = new SerialLayer('COM5');
const packageLayer = new PackageLayer(serialLayer);
const radioLayer = new RadioLayer(packageLayer);

async function test() {
    const close = await serialLayer.open();
    await radioLayer.init({ key: Buffer.from('1234567890123456') });

    for (let packetSize = 1; packetSize < 57; packetSize++) {
        const packets = 200;
        const start = new Date().getTime();
        for (let i = 0; i < packets; i += 3) {
            await radioLayer.send(2, new Buffer(packetSize));
        }
        const time = (new Date().getTime() - start) / 1000 / packets;
        console.log(`${packetSize} -  ${1 / time * packetSize / 1024 * 8} kbps`)
    }
    close();
}

function delay(time: number) {
    return new Promise(resolve => setTimeout(resolve, time));
}

test().catch(err => {
    console.error(err);
});