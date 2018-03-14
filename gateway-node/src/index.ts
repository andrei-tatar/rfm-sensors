import { RadioNode } from './communication/node';
import { PackageLayer } from './communication/package';
import { RadioLayer } from './communication/radio';
import { Telnet } from './communication/telnet';
import './vendor';

import { readFile } from 'fs';

const logger: Logger = console;

async function test() {
    const telnetLayer = new Telnet(logger, '192.168.1.51');
    try {
        telnetLayer.open();
        await telnetLayer.connected.first(c => c).toPromise();
        const packageLayer = new PackageLayer(telnetLayer);
        const radioLayer = new RadioLayer(packageLayer);
        const node = new RadioNode(radioLayer, 15);

        node.data.subscribe(d => logger.info(`rx: ${d.toString('hex')}`));

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
            readFile(`C:/test.hex`, (err, data) => {
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
    logger.error(err);
});
