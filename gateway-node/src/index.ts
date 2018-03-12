import { Observable } from 'rxjs/Observable';
import { MessageLayer } from './communication/message';
import './vendor';
import { RadioNode } from './node';
import { RadioLayer } from './communication/radio';
import { SerialLayer } from './communication/serial';
import { PackageLayer } from './communication/package';

import { readFile } from 'fs';
import { Telnet } from './communication/telnet';

async function test() {
    const telnetLayer = new Telnet('192.168.1.51');
    try {
        telnetLayer.open();
        await telnetLayer.connected.first(c => c).toPromise();

        const packageLayer = new PackageLayer(telnetLayer);
        const radioLayer = new RadioLayer(packageLayer);


        telnetLayer.open();
       
        const node = new RadioNode(radioLayer, 15);
        node.data.subscribe(d => console.log(`rx: ${d.toString('hex')}`))

        // for (let b = 0; b < 255; b += 10) {
        //     await node.send(Buffer.from([4, b]));
        //     await delay(100);
        // }

        // await Observable.concat(
        //     node.send(Buffer.from([1, 100])),
        //     Observable.timer(1000),
        //     node.send(Buffer.from([1, 0])),
        //     Observable.timer(1000),
        // ).toPromise();

        const hexFile = await new Promise<Buffer>((resolve, reject) => {
            readFile(`D:/Proiecte/rfm-sensors/sensor-light-dimmer/.pioenvs/pro16MHzatmega328/firmware.hex`, (err, data) => {
                if (err) { reject(err); } else { resolve(data); }
            });
        });
        await node.upload(hexFile).toPromise();

        // await delay(15000);

    } finally {
        telnetLayer.close();
    }
}

function delay(time: number) {
    return new Promise(resolve => setTimeout(resolve, time));
}

test().catch(err => {
    console.error(err);
});