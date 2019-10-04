import { Observable, Subject } from 'rxjs';
import { delay, distinctUntilChanged, publishReplay, refCount, retryWhen } from 'rxjs/operators';

import * as SerialPort from 'serialport';

import { Logger } from '../Logger';
import { ConnectableLayer } from './message';

export class SerialLayer implements ConnectableLayer<Buffer> {
    private serial: SerialPort;
    private _data = new Subject<Buffer>();

    readonly data = this._data.asObservable();
    readonly connected = this.connect().pipe(
        distinctUntilChanged(),
        retryWhen(err => err.pipe(delay(5000))),
    );

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
    }

    private connect() {
        return new Observable<boolean>(observer => {
            this.serial.open(err => {
                if (err) {
                    this.logger.warn(`serial.open: ${err.message}`);
                    observer.error(err);
                }
            });
            const openHandler = () => {
                this.serial.set({ dtr: false });
                observer.next(true);
            };
            const errorHandler = err => {
                this.logger.warn(`serial.error: ${err.message}`);
                observer.error(err);
            };
            this.serial.on('open', openHandler);
            this.serial.on('error', errorHandler);
            return () => {
                this.serial.off('open', openHandler);
                this.serial.off('error', errorHandler);
                this.serial.close(err => {
                    if (err) {
                        this.logger.warn(`serial.close: ${err.message}`);
                    }
                });
            };
        });
    }

    send(data: Buffer) {
        return new Observable<never>(observer => {
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
