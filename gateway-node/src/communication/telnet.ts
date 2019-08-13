import * as net from 'net';
import { merge, Observable, of, Subject, throwError, timer } from 'rxjs';
import {
    catchError, concatMap, delay, distinctUntilChanged, first,
    map, publishReplay, refCount, retryWhen, switchMap
} from 'rxjs/operators';

import { ConnectableLayer } from './message';

export class Telnet implements ConnectableLayer<Buffer> {
    private readonly _data = new Subject<Buffer>();
    private socket: net.Socket;

    readonly data = this._data.asObservable();
    readonly connected = this.connect()
        .pipe(
            distinctUntilChanged(),
            switchMap(isConnected => {
                const fwd = of(isConnected);
                if (isConnected) {
                    return merge(fwd,
                        timer(0, 5000).pipe(
                            // heartbeat
                            switchMap(_ => this.sendRaw(Buffer.from([0xDE, 0x5B, 0x01, 0xFF, 0x40, 0x79]))),
                            map(() => isConnected),
                            catchError(err => {
                                this.logger.warn(`could not send heartbeat ${err.message}`);
                                return fwd;
                            })
                        )
                    );
                }
                return fwd;
            }),
            distinctUntilChanged(),
            retryWhen(err => err.pipe(delay(this.reconnectInterval))),
            publishReplay(1),
            refCount(),
        );

    constructor(
        private logger: Logger,
        private host: string,
        private port: number = 23,
        private reconnectInterval: number = 5000,
    ) {
    }

    private connect() {
        return new Observable<boolean>(observer => {
            this.socket = new net.Socket();
            this.socket.on('data', data => this._data.next(data));
            this.socket.once('disconnect', () => {
                observer.error(new Error('disconnected from server'));
            });
            this.socket.once('error', err => {
                observer.error(err);
            });
            this.logger.debug('telnet: connecting');
            this.socket.connect(this.port, this.host, async () => {
                this.logger.debug('telnet: connected');
                observer.next(true);
            });
            return () => {
                this.socket.destroy();
                this.socket = null;
            };
        });

    }

    send(data: Buffer): Observable<void> {
        if (!this.socket) {
            return throwError(new Error('telnet: not connected'));
        }

        return this.connected.pipe(
            first(),
            concatMap<boolean, Promise<void>>(c => {
                if (!c) { throw new Error('telnet: not connected'); }
                return this.sendRaw(data);
            })
        );
    }

    private sendRaw(data: Buffer) {
        return new Promise<void>(resolve => this.socket.write(data, resolve));
    }
}
