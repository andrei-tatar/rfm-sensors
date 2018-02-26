import { RadioLayer } from './communication/radio';
import { SerialLayer } from './communication/serial';
import { PackageLayer } from './communication/package';

import 'rxjs/add/operator/first';

const serialLayer = new SerialLayer('COM7');
const packageLayer = new PackageLayer(serialLayer);
const radioLayer = new RadioLayer(packageLayer);

async function test() {
    await serialLayer.open();
    await radioLayer.init({ key: Buffer.from('16 byte AES KEY.') });

    radioLayer.send(2, Buffer.from('Hello world!'));
}

function delay(time: number) {
    return new Promise(resolve => setTimeout(resolve, time));
}

test().catch(err => {
    console.error(err);
});