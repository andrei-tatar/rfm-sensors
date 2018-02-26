import * as SerialPort from 'serialport';
import { Observable } from 'rxjs/Observable';

import { MessageLayer } from './message';
import { Subject } from 'rxjs/Subject';

export class SerialLayer implements MessageLayer {
    private serial: SerialPort;
    private _data = new Subject<Buffer>();

    constructor(port: string) {
        this.serial = new SerialPort(port, {
            baudRate: 230400,
            dataBits: 8,
            stopBits: 1,
            parity: 'none',
            autoOpen: false,
        });
        this.serial.on('data', data => this._data.next(data));
    }

    open() {
        return new Promise<void>((resolve, reject) => {
            this.serial.on('open', () => {
                resolve();
            });
            this.serial.on('error', reject);
            this.serial.open();
        });
    }

    get data() {
        return this._data.asObservable();
    }

    send(data: Buffer) {
        return new Promise<void>((resolve, reject) => {
            this.serial.write(data, (err) => {
                if (err) {
                    reject(err);
                } else {
                    resolve();
                }
            });
        });
    }
}