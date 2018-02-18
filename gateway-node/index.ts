import { SerialLayer } from './communication/serial';
import { PackageLayer } from './communication/package';

import 'rxjs/add/operator/first';

const serialLayer = new SerialLayer('COM7');
const packageLayer = new PackageLayer(serialLayer);

async function test() {
    await serialLayer.open();

    const start = new Date().getTime();
    packageLayer.data.first().subscribe(v => {
        const time = new Date().getTime() - start;
        console.log(time, v);
    });

    await packageLayer.send(Buffer.from([2, 1, 2, 3]));
    await delay(2000);
}

function delay(time: number) {
    return new Promise(resolve => setTimeout(resolve, time));
}

test().catch(err => {
    console.error(err);
});