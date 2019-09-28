import * as net from 'net';
import { merge, Observable, of, Subject, timer } from 'rxjs';
import {
    catchError, delay, distinctUntilChanged, first, map, publishReplay,
    refCount, retryWhen, startWith, switchMap
} from 'rxjs/operators';

import { ConnectableLayer } from './message';

export class Telnet implements ConnectableLayer<Buffer> {
    private readonly _data = new Subject<Buffer>();
    private socket$ = this.createSocket().pipe(publishReplay(1), refCount());

    readonly data = this._data.asObservable();
    readonly connected = this.socket$
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
            retryWhen(err => err.pipe(delay(this.reconnectInterval))),
            distinctUntilChanged(),
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

    private createSocket() {
        return new Observable<net.Socket>(observer => {
            const socket = new net.Socket();
            socket.on('data', data => this._data.next(data));
            socket.once('disconnect', () => {
                observer.error(new Error('disconnected from server'));
            });
            socket.once('error', err => {
                observer.error(err);
            });
            socket.connect(this.port, this.host, async () => {
                this.logger.debug('telnet: connected');
                observer.next(socket);
            });
            return () => socket.destroy();
        });
    }

    send(data: Buffer): Observable<void> {
        return this.socket$.pipe(
            first(),
            switchMap(socket => {
                return new Promise<void>(resolve => socket.write(data, resolve));
            }),
        );
    }
}
