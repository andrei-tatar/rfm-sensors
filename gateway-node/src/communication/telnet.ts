import * as net from 'net';
import { defer, Observable, Subject } from 'rxjs';
import { delay, distinctUntilChanged, map, retryWhen, startWith, tap } from 'rxjs/operators';

import { ConnectableLayer } from './message';

export class Telnet implements ConnectableLayer<Buffer> {
    private readonly _data = new Subject<Buffer>();
    private socket: net.Socket | null = null;

    readonly data = this._data.asObservable();
    readonly connected = this.createSocket()
        .pipe(
            map(_ => true),
            startWith(false),
            retryWhen(errs => errs.pipe(
                tap(err => this.logger.info(`telnet: error, trying to reconnect: ${err.message}`)),
                delay(this.reconnectInterval),
            )),
            distinctUntilChanged(),
        );

    constructor(
        private logger: Logger,
        private host: string,
        private port: number = 23,
        private reconnectInterval: number = 5000,
    ) {
    }

    private createSocket() {
        return new Observable<net.Socket>(observer => {
            const socket = new net.Socket();
            socket.setNoDelay(true);
            socket.setKeepAlive(true, 5000);
            socket.on('data', data => this._data.next(data));
            socket.once('disconnect', () => observer.error(new Error('disconnected from server')));
            socket.once('error', err => observer.error(err));
            socket.once('end', (hadError: boolean) => {
                if (!hadError) {
                    observer.error(new Error('connection closed'));
                }
            });
            this.logger.info(`telnet: connecting to ${this.host}:${this.port}`);
            socket.connect(this.port, this.host, async () => {
                this.logger.info('telnet: connected');
                this.socket = socket;
                observer.next(socket);
            });
            return () => {
                this.socket = null;
                socket.end();
                socket.destroy();
            };
        });
    }

    send(data: Buffer): Observable<never> {
        return defer<never>(() => {
            if (!this.socket) { throw new Error('telnet: socket not connected'); }

            return new Promise((resolve, reject) => this.socket.write(data, (err) => {
                if (err) { reject(err); } else { resolve(); }
            }));
        });
    }
}
