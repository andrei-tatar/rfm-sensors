import { Observable } from 'rxjs/Observable';
import * as SerialPort from 'serialport';

import { BehaviorSubject } from 'rxjs/BehaviorSubject';
import { Subject } from 'rxjs/Subject';

import { ConnectableLayer } from './message';

export class SerialLayer implements ConnectableLayer<Buffer> {
    private serial: SerialPort;
    private _data = new Subject<Buffer>();
    private _connected = new BehaviorSubject(false);

    readonly data = this._data.asObservable();
    readonly connected = this._connected.asObservable().distinctUntilChanged();

    constructor(
        private logger: Logger,
        port: string,
        baud: number = 230400,
    ) {
        this.serial = new SerialPort(port, {
            baudRate: baud,
            dataBits: 8,
            stopBits: 1,
            parity: 'none',
            autoOpen: false,
        });
        this.serial.on('data', data => this._data.next(data));
        this.serial.on('open', () => {
            this.serial.set({ dtr: false });
            this._connected.next(true);
        });
        this.serial.on('error', err => {
            this.logger.warn(`serial.error: ${err.message}`);
            this._connected.error(err);
        });
    }

    connect() {
        this.serial.open();
    }

    close() {
        this.serial.close(err => {
            if (err) {
                this.logger.warn(`serial.close: ${err.message}`);
                this._connected.error(err);
            }
        });
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
