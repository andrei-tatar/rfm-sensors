import { Observable } from 'rxjs/Observable';
import * as SerialPort from 'serialport';

import { Subject } from 'rxjs/Subject';
import { MessageLayer } from './message';

export class SerialLayer implements MessageLayer<Buffer> {
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

    close() {
        return new Promise<void>((resolve, reject) => {
            this.serial.close(err => {
                if (err) {
                    reject(err);
                } else {
                    resolve();
                }
            });
        });
    }

    get data() {
        return this._data.asObservable();
    }

    send(data: Buffer) {
        return new Observable<void>(observer => {
            this.serial.write(data, (err) => {
                if (err) {
                    observer.error(err);
                } else {
                    observer.complete();
                }
            });
        });
    }
}
