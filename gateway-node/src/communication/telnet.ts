import * as net from 'net';
import { defer, merge, Observable, of, Subject, timer } from 'rxjs';
import {
    catchError, delay, distinctUntilChanged, ignoreElements,
    map, retryWhen, startWith, switchMap, tap
} from 'rxjs/operators';

import { ConnectableLayer } from './message';

export class Telnet implements ConnectableLayer<Buffer> {
    private readonly _data = new Subject<Buffer>();
    private socket: net.Socket | null = null;

    readonly data = this._data.asObservable();
    readonly connected = this.createSocket()
        .pipe(
            startWith(false),
            map(_ => true),
            switchMap(isConnected => {
                const fwd = of(isConnected);
                if (isConnected) {
                    return merge(fwd,
                        timer(0, 5000).pipe(
                            // heartbeat
                            switchMap(_ => this.send(Buffer.from([0xDE, 0x5B, 0x01, 0xFF, 0x40, 0x79]))),
                            catchError(err => {
                                throw new Error(`could not send heartbeat: ${err.message}`);
                            }),
                            ignoreElements(),
                        )
                    );
                }
                return fwd;
            }),
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
            socket.setTimeout(5000);
            socket.on('data', data => this._data.next(data));
            socket.once('disconnect', () => observer.error(new Error('disconnected from server')));
            socket.once('error', err => observer.error(err));
            socket.once('timeout', () => observer.error(new Error('socket timeout')));
            this.logger.info(`telnet: connecting to ${this.host}:${this.port}`);
            socket.connect(this.port, this.host, async () => {
                this.logger.info('telnet: connected');
                this.socket = socket;
                observer.next(socket);
            });
            return () => {
                this.socket = null;
                socket.end();
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
