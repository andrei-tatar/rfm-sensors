import { Subject } from 'rxjs/Subject';
import { RadioNode } from './communication/node';
import { PackageLayer } from './communication/package';
import { RadioLayer } from './communication/radio';
import { Telnet } from './communication/telnet';

import './vendor';

import { readFile } from 'fs';

import { Observable } from 'rxjs/Observable';
import { queue } from 'rxjs/Scheduler/queue'
import { SerialLayer } from './communication/serial';

const logger: Logger = console;

async function test() {

    // const baseLayer = new Telnet(logger, '192.168.1.51');
    // baseLayer.open();
    // await baseLayer.connected.first(c => c).toPromise();

    const baseLayer = new SerialLayer('COM7');
    await baseLayer.open();

    try {
        const packageLayer = new PackageLayer(baseLayer);
        const radioLayer = new RadioLayer(packageLayer);

        const oldSend = radioLayer.send.bind(radioLayer);
        radioLayer.send = ({ addr, data }) => {
            logger.log(`tx (${addr}): ${data.toString('hex')}`);
            return oldSend({ addr, data });
        };
        radioLayer.data.subscribe(d => logger.log(`rx (${d.addr}): ${d.data.toString('hex')}`));

        await radioLayer
            .init({
                key: Buffer.from([0xd0, 0xd9, 0x8f, 0xfb, 0x97, 0x47, 0x79, 0xe6, 0x68, 0x77, 0xd7, 0xb9, 0x60, 0xa3, 0xb4, 0x53])
            })
            .toPromise();


        // for (let b = 0; b < 255; b += 10) {
        //     await node.send(Buffer.from([4, b]));
        //     await delay(100);
        // }

        const ids = Observable.range(14, 1);
        await Observable.concat(
            ids.map(id => new RadioNode(radioLayer, id))
                .concatMap(node => node.send(Buffer.from([1, 60]))),
            Observable.timer(2000),
            ids.map(id => new RadioNode(radioLayer, id))
                .concatMap(node => node.send(Buffer.from([1, 0]))),
            Observable.never(),
        ).toPromise();


        for (let id = 14; id <= 18; id++) {
            logger.info(`uploading to ${id}`);
            const node = new RadioNode(radioLayer, id);
            const hexFile = await new Promise<Buffer>((resolve, reject) => {
                readFile(`D:/Proiecte/rfm-sensors/sensor-light-dimmer/.pioenvs/pro16MHzatmega328/firmware.hex`, (err, data) => {
                    if (err) { reject(err); } else { resolve(data); }
                });
            });
            const progress = new Subject<number>();
            progress
                .debounceTime(600)
                .subscribe(p => logger.info(`progress: ${p}`));
            await node.upload(hexFile, progress).toPromise();
            await delay(1000);
        }

    } finally {
        baseLayer.close();
    }
}

function delay(time: number) {
    return new Promise(resolve => setTimeout(resolve, time));
}

test().catch(err => {
    logger.error(err);
});
