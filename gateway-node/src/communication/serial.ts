import { Observable, Subject } from 'rxjs';
import { delay, distinctUntilChanged, map, retryWhen, startWith } from 'rxjs/operators';

import * as SerialPort from 'serialport';

import { Logger } from '../Logger';
import { ConnectableLayer } from './message';

export class SerialLayer implements ConnectableLayer<Buffer> {
    private _serial: SerialPort;
    private _data = new Subject<Buffer>();

    readonly data = this._data.asObservable();
    readonly connected = this.connect().pipe(
        map(() => true),
        startWith(false),
        distinctUntilChanged(),
        retryWhen(err => err.pipe(delay(5000))),
    );

    constructor(
        private logger: Logger,
        private port: string,
        private baud: number = 230400,
    ) {
    }

    private connect() {
        return new Observable<SerialPort>(observer => {
            this.logger.info(`serial: connecting to ${this.port}:${this.baud}`);
            const serial = new SerialPort(this.port, {
                baudRate: this.baud,
                dataBits: 8,
                stopBits: 1,
                parity: 'none',
                autoOpen: false,
            });
            serial.on('data', data => this._data.next(data));
            serial.open(err => {
                if (err) {
                    this.logger.error(`serial.open: ${err.message}`);
                    observer.error(err);
                } else {
                    this.logger.info(`serial: connected to ${this.port}:${this.baud}`);
                }
            });
            const openHandler = () => {
                serial.set({ dtr: false });
                this._serial = serial;
                observer.next(serial);
            };
            const errorHandler = err => {
                this.logger.warn(`serial.error: ${err.message}`);
                observer.error(err);
            };
            serial.on('open', openHandler);
            serial.on('error', errorHandler);
            return () => {
                this._serial = null;
                serial.off('open', openHandler);
                serial.off('error', errorHandler);
                serial.close(err => {
                    if (err) {
                        this.logger.warn(`serial.close: ${err.message}`);
                    }
                });
            };
        });
    }

    send(data: Buffer) {
        return new Observable<never>(observer => {
            if (!this._serial) { throw new Error('serial: port not connected'); }
            this._serial.write(data, (err) => {
                if (err) {
                    observer.error(err);
                } else {
                    observer.complete();
                }
            });
        });
    }
}
